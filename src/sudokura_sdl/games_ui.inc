/* Retained Games browser. The library, not this view, owns durable records. */
typedef struct {
  GameLibraryEntry entry;
  bool hot_only;
} GamesRow;

typedef struct {
  GamesRow *rows;
  size_t count, capacity, selected, scroll;
  StoreStatus scan_status;
  bool incomplete, modal_owned, preview_valid, scanning;
  GameLibraryScanCursor scanner;
  SessionState preview;
  int focus; /* 0: rows, 1: primary action, 2: Back */
} GamesBrowser;

static GamesBrowser games_browser;

typedef struct {
  SDL_Rect title, list, detail, primary, back;
  int row_height, visible;
} GamesBrowserLayout;

static GamesBrowserLayout games_browser_layout(const Gfx *gfx) {
  GamesBrowserLayout layout;
  memset(&layout, 0, sizeof(layout));
  int margin = gfx->width < 640 ? 12 : 24;
  int width = gfx->width - margin * 2;
  if (width > 850) width = 850;
  int x = (gfx->width - width) / 2;
  int header_y = gfx->height < 520 ? 55 : 76;
  layout.title = (SDL_Rect){x, header_y, width, 42};
  int bottom = gfx->height - margin;
  int detail_h = gfx->height < 550 ? 76 : 104;
  int buttons_h = 40;
  int list_y = header_y + 49;
  int list_h = bottom - buttons_h - 12 - detail_h - 12 - list_y;
  if (list_h < 48) list_h = 48;
  layout.list = (SDL_Rect){x, list_y, width, list_h};
  layout.row_height = gfx->height < 550 ? 44 : 52;
  layout.visible = list_h / layout.row_height;
  if (layout.visible < 1) layout.visible = 1;
  layout.detail = (SDL_Rect){x, list_y + list_h + 8, width, detail_h};
  int button_y = layout.detail.y + detail_h + 8;
  int gap = 8;
  int back_w = width / 3;
  layout.back = (SDL_Rect){x, button_y, back_w, buttons_h};
  layout.primary = (SDL_Rect){x + back_w + gap, button_y,
                              width - back_w - gap, buttons_h};
  return layout;
}

static bool games_browser_push(const GameLibraryEntry *entry, bool hot) {
  if (games_browser.count == games_browser.capacity) {
    if (games_browser.capacity > SIZE_MAX / (2u * sizeof(GamesRow)))
      return false;
    size_t capacity = games_browser.capacity ? games_browser.capacity * 2u : 32u;
    GamesRow *rows = (GamesRow *)realloc(games_browser.rows,
                                         capacity * sizeof(GamesRow));
    if (!rows) return false;
    games_browser.rows = rows;
    games_browser.capacity = capacity;
  }
  games_browser.rows[games_browser.count++] = (GamesRow){*entry, hot};
  return true;
}

static bool games_browser_scan_entry(const GameLibraryEntry *entry, void *ctx) {
  (void)ctx;
  return games_browser_push(entry, false);
}

static int games_browser_row_cmp(const void *lhs, const void *rhs) {
  const GamesRow *a = (const GamesRow *)lhs;
  const GamesRow *b = (const GamesRow *)rhs;
  bool a_active = a->entry.result.status == SESSION_ACTIVE;
  bool b_active = b->entry.result.status == SESSION_ACTIVE;
  if (a_active != b_active) return a_active ? -1 : 1;
  uint64_t at = a->entry.finished_utc ? a->entry.finished_utc :
                a->entry.updated_utc;
  uint64_t bt = b->entry.finished_utc ? b->entry.finished_utc :
                b->entry.updated_utc;
  if (at != bt) return at > bt ? -1 : 1;
  if (a->entry.id.high != b->entry.id.high)
    return a->entry.id.high > b->entry.id.high ? -1 : 1;
  if (a->entry.id.low != b->entry.id.low)
    return a->entry.id.low > b->entry.id.low ? -1 : 1;
  return 0;
}

static void games_browser_add_hot(const ProfileSlot *slot) {
  if (!slot || !slot->present || !game_attempt_id_valid(slot->attempt_id))
    return;
  for (size_t i = 0; i < games_browser.count; ++i)
    if (game_attempt_id_equal(games_browser.rows[i].entry.id,
                              slot->attempt_id)) return;
  GameLibraryEntry entry;
  memset(&entry, 0, sizeof(entry));
  entry.id = slot->attempt_id;
  entry.status = STORE_OK;
  entry.previous_status = STORE_NOT_FOUND;
  entry.kind = GAME_RECORD_EXACT;
  entry.revision = slot->revision;
  const SessionState *s = &slot->value.session;
  entry.result.seed = s->game.seed;
  entry.result.generator_revision = s->game.generator_revision;
  entry.result.difficulty = s->game.difficulty;
  entry.result.mode = s->mode;
  entry.result.status = s->status;
  entry.result.assisted = slot->value.assisted;
  entry.result.elapsed_ms = s->elapsed_ms;
  entry.result.is_daily = s->is_daily;
  entry.result.daily_year = (uint16_t)s->daily_year;
  entry.result.daily_month = (uint8_t)s->daily_month;
  entry.result.daily_day = (uint8_t)s->daily_day;
  entry.result.attempt_id = slot->attempt_id;
  if (!games_browser_push(&entry, true)) games_browser.incomplete = true;
}

static void games_browser_preview_selected(const Persistence *p) {
  games_browser.preview_valid = false;
  if (!p || !p->enabled || games_browser.selected >= games_browser.count)
    return;
  const GamesRow *row = &games_browser.rows[games_browser.selected];
  if (row->entry.status != STORE_OK ||
      row->entry.kind != GAME_RECORD_EXACT) return;
  if (row->hot_only) {
    const ProfileSlot *slots[2] = {&p->profile.normal, &p->profile.daily};
    for (int i = 0; i < 2; ++i) {
      if (slots[i]->present && game_attempt_id_equal(
              slots[i]->attempt_id, row->entry.id)) {
        games_browser.preview = slots[i]->value.session;
        games_browser.preview_valid = true;
        return;
      }
    }
    return;
  }
  GameLibraryRecord record;
  if (game_library_load(p->profile_directory, row->entry.id,
                        &record) == STORE_OK &&
      record.kind == GAME_RECORD_EXACT) {
    games_browser.preview = record.exact.session;
    games_browser.preview_valid = true;
  }
}

static void games_browser_ensure_visible(const Gfx *gfx) {
  GamesBrowserLayout layout = games_browser_layout(gfx);
  size_t visible = (size_t)layout.visible;
  if (games_browser.selected < games_browser.scroll)
    games_browser.scroll = games_browser.selected;
  if (games_browser.selected >= games_browser.scroll + visible)
    games_browser.scroll = games_browser.selected + 1u - visible;
}

static void games_browser_close(Persistence *p, Game *game, UI *ui) {
  game_library_scan_close(&games_browser.scanner);
  bool resume = games_browser.modal_owned &&
                ui->prev_screen == SCR_PLAY;
  free(games_browser.rows);
  memset(&games_browser, 0, sizeof(games_browser));
  (void)app_return_aux(ui);
  if (resume) {
    (void)clock_resume(ui, PAUSE_MODAL);
    (void)checkpoint_current_session(p, game, ui);
  }
}

/* Enumerate at most sixteen directory names per frame. */
static void games_browser_finish_scan(const Persistence *p, Gfx *gfx) {
  game_library_scan_close(&games_browser.scanner);
  games_browser.scanning = false;
  games_browser.incomplete = games_browser.scan_status != STORE_OK;
  games_browser_add_hot(&p->profile.normal);
  games_browser_add_hot(&p->profile.daily);
  qsort(games_browser.rows, games_browser.count, sizeof(GamesRow),
        games_browser_row_cmp);
  games_browser.selected = games_browser.scroll = 0;
  games_browser.focus = games_browser.count ? 0 : 2;
  games_browser_preview_selected(p);
  games_browser_ensure_visible(gfx);
}

static bool games_browser_scan_pump(const Persistence *p, Gfx *gfx) {
  if (!games_browser.scanning || !p || !gfx) return false;
  bool finished = false;
  games_browser.scan_status = game_library_scan_step(
      &games_browser.scanner, 16u, games_browser_scan_entry, NULL, &finished);
  if (finished || games_browser.scan_status != STORE_OK)
    games_browser_finish_scan(p, gfx);
  return true;
}

static bool games_browser_open(Persistence *p, Game *game, UI *ui, Gfx *gfx) {
  if (!p || !p->enabled || !ui || !gfx ||
      (ui->screen != SCR_TITLE && ui->screen != SCR_PLAY)) return false;
  bool from_play = ui->screen == SCR_PLAY;
  if (from_play &&
      (!ui->session_open || ui->result != RES_NONE ||
       (ui->pause_reasons & (PAUSE_FOCUS | PAUSE_END | PAUSE_HOME)) != 0))
    return false;
  bool modal_owned = false;
  if (from_play) {
    modal_owned = clock_pause(ui, PAUSE_MODAL);
    if (!checkpoint_current_session(p, game, ui)) {
      if (modal_owned) (void)clock_resume(ui, PAUSE_MODAL);
      show_toast(ui, tr(ui->language, T_SAVE_FAILED));
      return false;
    }
  }
  free(games_browser.rows);
  memset(&games_browser, 0, sizeof(games_browser));
  games_browser.modal_owned = modal_owned;
  clear_hint_preview(ui);
  games_browser.scan_status = game_library_scan_begin(
      p->profile_directory, &games_browser.scanner);
  games_browser.scanning = games_browser.scan_status == STORE_OK &&
                           !games_browser.scanner.finished;
  games_browser.focus = 2; /* Back remains available during discovery. */
  if (!games_browser.scanning) games_browser_finish_scan(p, gfx);
  (void)app_open_aux(ui, APP_SCREEN_GAMES);
  return true;
}

static bool games_browser_save_hot(Persistence *p, bool daily) {
  ProfileSlot *slot = daily ? &p->profile.daily : &p->profile.normal;
  if (!slot->present || !game_attempt_id_valid(slot->attempt_id) ||
      slot->revision == UINT64_MAX) return false;
  if (slot->retained) return true;
  GameLibraryRecord record;
  if (!game_library_make_exact_runtime(&record, slot->attempt_id,
                               slot->revision + 1u, &slot->value, true))
    return false;
  if (game_library_save_runtime(p->profile_directory, &record,
                                &p->game_cache[daily ? 1 : 0]) != STORE_OK)
    return false;
  ProfileData candidate = p->profile;
  ProfileSlot *updated = daily ? &candidate.daily : &candidate.normal;
  updated->retained = true;
  updated->revision = record.revision;
  candidate.preferences = p->preferences;
  if (profile_save_runtime_file(p->profile_path, p->profile_backup_path,
                                &candidate) != STORE_OK)
    return false;
  p->profile = candidate;
  p->game_cache[daily ? 1 : 0].valid = false;
  persistence_refresh_normal(p);
  /* Save policy is cleared by the caller's checkpoint; keep unrelated dirt. */
  return true;
}

static bool games_browser_save_current(Persistence *p, Game *game, UI *ui,
                                       Gfx *gfx) {
  if (!p || !p->enabled || !game || !ui || !gfx ||
      !app_play_interactions_allowed(ui)) return false;
  bool ok = stage_current_session(p, game, ui);
  if (ok) ok = games_browser_save_hot(p, ui->is_daily);
  if (ok) ok = persistence_flush(p);
  if (ok) {
    p->last_periodic_checkpoint_ms = now_ms();
    show_toast(ui, tr(ui->language, T_SAVED));
    return true;
  }
  /* A failed write must not claim retained progress or clear dirty state. */
  uint64_t failed_at = now_ms();
  save_policy_mark(&p->save_policy,
                   ui->is_daily ? SAVE_DIRTY_DAILY : SAVE_DIRTY_NORMAL,
                   failed_at);
  save_policy_record_result(&p->save_policy, STORE_IO_ERROR, failed_at);
  bool modal_owned = clock_pause(ui, PAUSE_MODAL);
  (void)show_simple_message_box(gfx, game, ui, SDL_MESSAGEBOX_WARNING,
                                tr(ui->language, T_STORAGE_ERROR),
                                tr(ui->language, T_SAVE_FAILED));
  if (modal_owned) (void)clock_resume(ui, PAUSE_MODAL);
  return false;
}

static bool games_browser_activate(Persistence *p, Game *game, UI *ui,
                                   Gfx *gfx) {
  if (games_browser.scanning || games_browser.selected >= games_browser.count ||
      !p || !p->enabled)
    return false;
  GamesRow row = games_browser.rows[games_browser.selected];
  if (row.entry.status != STORE_OK) {
    (void)show_simple_message_box(
        gfx, game, ui, SDL_MESSAGEBOX_WARNING, tr(ui->language, T_GAMES),
        tr(ui->language, T_GAMES_UNAVAILABLE));
    return false;
  }
  /* Finished entries are deliberately read-only in this first browser slice. */
  if (row.entry.result.status != SESSION_ACTIVE) return false;

  GameLibraryRecord target;
  bool valid = false;
  if (row.hot_only) {
    const ProfileSlot *slots[2] = {&p->profile.normal, &p->profile.daily};
    for (int i = 0; i < 2; ++i) {
      if (slots[i]->present &&
          game_attempt_id_equal(slots[i]->attempt_id, row.entry.id)) {
        valid = game_library_make_exact(&target, row.entry.id,
                                        slots[i]->revision,
                                        &slots[i]->value, slots[i]->retained);
        break;
      }
    }
  } else {
    valid = game_library_load(p->profile_directory, row.entry.id,
                              &target) == STORE_OK;
  }
  if (!valid || target.kind != GAME_RECORD_EXACT ||
      target.exact.session.status != SESSION_ACTIVE) {
    (void)show_simple_message_box(gfx, game, ui, SDL_MESSAGEBOX_WARNING,
                                  tr(ui->language, T_GAMES),
                                  tr(ui->language, T_GAMES_UNAVAILABLE));
    return false;
  }
  bool daily = target.exact.session.is_daily;
  ProfileSlot *departing = daily ? &p->profile.daily : &p->profile.normal;
  if (departing->present &&
      game_attempt_id_equal(departing->attempt_id, target.id)) {
    bool play_current = ui->prev_screen == SCR_PLAY && ui->session_open &&
                        ui->is_daily == daily &&
                        game_attempt_id_equal(p->active_attempt_id,target.id);
    games_browser_close(p, game, ui);
    if (!play_current) restore_saved_session(p, game, ui, daily);
    return true;
  }
  if (!persistence_flush(p)) {
    (void)show_simple_message_box(gfx, game, ui, SDL_MESSAGEBOX_WARNING,
                                  tr(ui->language, T_STORAGE_ERROR),
                                  tr(ui->language, T_SAVE_FAILED));
    return false;
  }
  if (departing->present && !departing->retained &&
      session_has_meaningful_progress_runtime(&departing->value.session)) {
    int choice = choice_box(gfx, game, ui,
                            tr(ui->language, T_GAMES_KEEP_TITLE),
                            tr(ui->language, T_GAMES_KEEP_PROMPT),
                            tr(ui->language, T_CANCEL),
                            tr(ui->language, T_GAMES_SAVE_CONTINUE),
                            tr(ui->language, T_DISCARD));
    if (choice == 0) return false;
    if (choice == 1 && !games_browser_save_hot(p, daily)) {
      (void)show_simple_message_box(gfx, game, ui,
                                    SDL_MESSAGEBOX_WARNING,
                                    tr(ui->language, T_STORAGE_ERROR),
                                    tr(ui->language, T_SAVE_FAILED));
      return false;
    }
  }
  ProfileData candidate = p->profile;
  ProfileSlot *slot = daily ? &candidate.daily : &candidate.normal;
  memset(slot, 0, sizeof(*slot));
  slot->present = true;
  slot->retained = target.retained;
  slot->attempt_id = target.id;
  slot->revision = target.revision;
  slot->value = target.exact;
  candidate.preferences = p->preferences;
  /* The old board remains visible until the new hot pointer is durable. */
  if (profile_save_runtime_file(p->profile_path, p->profile_backup_path,
                                &candidate) != STORE_OK) {
    (void)show_simple_message_box(gfx, game, ui, SDL_MESSAGEBOX_WARNING,
                                  tr(ui->language, T_STORAGE_ERROR),
                                  tr(ui->language, T_SAVE_FAILED));
    return false;
  }
  p->profile = candidate;
  p->game_cache[daily ? 1 : 0].valid = false;
  persistence_refresh_normal(p);
  save_policy_init(&p->save_policy);
  games_browser.modal_owned = false;
  free(games_browser.rows);
  memset(&games_browser, 0, sizeof(games_browser));
  victory_animation_reset();
  restore_saved_session(p, game, ui, daily);
  return true;
}

static void games_browser_select(Persistence *p, Gfx *gfx, size_t index) {
  if (index >= games_browser.count) return;
  games_browser.selected = index;
  games_browser_preview_selected(p);
  games_browser_ensure_visible(gfx);
}

static void games_browser_draw_button(Gfx *gfx, Theme theme, SDL_Rect rect,
                                      const char *text, bool focused,
                                      bool enabled, SDL_Color color) {
  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool pressed = enabled && point_in(rect, mx, my) &&
                 (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Rect face = polish_button_face(
      gfx, theme, rect, enabled ? theme.btn : theme.palette_bg, pressed,
      !enabled);
  (void)draw_fitted_text(gfx, text, enabled ? color : theme.dim, face, 17, 10);
  if (focused && enabled) draw_focus_ring(gfx->ren, rect, theme.sel_outline);
}

static void games_browser_render(Gfx *gfx, const Game *game, UI *ui) {
  (void)game;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  GamesBrowserLayout lay = games_browser_layout(gfx);
  (void)draw_fitted_text(gfx, tr(ui->language, T_GAMES), theme.btnfg,
                         lay.title, 30, 16);
  draw_round_rect(gfx->ren, lay.list, 10, theme.palette_bg);
  size_t end = games_browser.scroll + (size_t)lay.visible;
  if (end > games_browser.count) end = games_browser.count;
  for (size_t index = games_browser.scroll; index < end; ++index) {
    const GamesRow *row = &games_browser.rows[index];
    SDL_Rect rect = {lay.list.x + 5,
                     lay.list.y + (int)(index - games_browser.scroll) *
                         lay.row_height + 3,
                     lay.list.w - 10, lay.row_height - 5};
    bool selected = index == games_browser.selected;
    draw_round_rect(gfx->ren, rect, 6,
                    selected ? theme.hover : theme.palette_bg);
    const GameLibraryEntry *e = &row->entry;
    char label[192];
    const char *status = e->status != STORE_OK
        ? tr(ui->language, T_GAMES_UNAVAILABLE)
        : e->result.status == SESSION_ACTIVE
            ? (row->hot_only ? tr(ui->language, T_GAMES_AUTOSAVE)
                             : tr(ui->language, T_GAMES_IN_PROGRESS))
            : (e->result.status == SESSION_WON
                ? tr(ui->language, T_YOU_WIN)
                : tr(ui->language, T_YOU_LOSE));
    if (e->status == STORE_OK)
      snprintf(label, sizeof(label), "%s · %s · %s",
               tr(ui->language, mode_key(e->result.mode)),
               difficulty_name(ui->language, e->result.difficulty), status);
    else
      snprintf(label, sizeof(label), "%s", status);
    (void)draw_fitted_text(gfx, label, theme.btnfg, rect, 17, 10);
    if (selected && games_browser.focus == 0)
      draw_focus_ring(gfx->ren, rect, theme.sel_outline);
  }
  if (!games_browser.count) {
    SDL_Rect empty = {lay.list.x + 12, lay.list.y + 14,
                      lay.list.w - 24, lay.list.h - 28};
    (void)polish_draw_wrapped_fit(gfx, tr(ui->language, games_browser.scanning
                                        ? T_GAMES_LOADING : T_GAMES_EMPTY),
                                  theme.dim, empty, 18, 10, true);
  }
  if (games_browser.selected < games_browser.count) {
    const GamesRow *row = &games_browser.rows[games_browser.selected];
    const ProfileResult *r = &row->entry.result;
    char detail[256];
    unsigned long long total = (unsigned long long)(r->elapsed_ms / 1000u);
    if (row->entry.status != STORE_OK) {
      snprintf(detail, sizeof(detail), "%s",
               tr(ui->language, T_GAMES_UNAVAILABLE));
    } else if (r->is_daily) {
      snprintf(detail, sizeof(detail), "%04u-%02u-%02u · %s · %02llu:%02llu%s%s",
               (unsigned)r->daily_year, (unsigned)r->daily_month,
               (unsigned)r->daily_day,
               difficulty_name(ui->language, r->difficulty),
               total / 60u, total % 60u,
               r->assisted ? " · " : "",
               r->assisted ? tr(ui->language, T_ASSISTED) : "");
    } else {
      snprintf(detail, sizeof(detail), "%s · %s · %02llu:%02llu%s%s",
               tr(ui->language, mode_key(r->mode)),
               difficulty_name(ui->language, r->difficulty),
               total / 60u, total % 60u,
               r->assisted ? " · " : "",
               r->assisted ? tr(ui->language, T_ASSISTED) : "");
    }
    SDL_Rect line = lay.detail;
    line.h = 28;
    (void)draw_fitted_text(gfx, detail, theme.btnfg, line, 16, 10);
    line.y += 28;
    line.h = lay.detail.h - 28;
    if (games_browser.preview_valid && lay.detail.h >= 96)
      line.w -= 70; /* Reserve the right side for the board thumbnail. */
    const char *description = !games_browser.preview_valid
        ? tr(ui->language, T_GAMES_SUMMARY_ONLY)
        : tr(ui->language, T_GAMES_LATEST);
    (void)polish_draw_wrapped_fit(gfx, description, theme.dim,
                                  line, 15, 10, true);
    /* Neutral fill-only preview: no solution/correctness information. */
    if (games_browser.preview_valid && lay.detail.h >= 96) {
      const Game *board = &games_browser.preview.game;
      int size = 54;
      int x = lay.detail.x + lay.detail.w - size - 8;
      int y = lay.detail.y + 32;
      int side = size / 9;
      for (int cell = 0; cell < 81; ++cell) {
        if (board->puzzle[cell] != 0)
          draw_rect(gfx->ren, x + (cell % 9) * side,
                    y + (cell / 9) * side, side - 1, side - 1,
                    theme.title);
      }
    }
  }
  if (games_browser.incomplete) {
    SDL_Rect notice = lay.title;
    notice.y += 29;
    notice.h = 18;
    (void)draw_fitted_text(gfx, tr(ui->language, T_GAMES_SCAN_INCOMPLETE),
                           theme.text_wrong, notice, 13, 10);
  }
  bool active = !games_browser.scanning &&
                 games_browser.selected < games_browser.count &&
                games_browser.rows[games_browser.selected].entry.status == STORE_OK &&
                games_browser.rows[games_browser.selected].entry.result.status ==
                    SESSION_ACTIVE;
  games_browser_draw_button(gfx, theme, lay.back, tr(ui->language, T_BACK),
                            games_browser.focus == 2, true, theme.btnfg);
  games_browser_draw_button(gfx, theme, lay.primary,
                            tr(ui->language, active ? T_GAMES_LOAD : T_GAMES_VIEW),
                            games_browser.focus == 1, active, theme.btnfg);
}

static bool games_browser_key(Persistence *p, Game *game, UI *ui, Gfx *gfx,
                               SDL_Keycode key, SDL_Keymod mod, bool repeat) {
  if (key == SDLK_ESCAPE) {
    games_browser_close(p, game, ui);
    return true;
  }
  if (games_browser.scanning || !games_browser.count) {
    if (!repeat && input_activation_key(key))
      games_browser_close(p, game, ui); /* Back is the only enabled control. */
    return true;
  }
  if (repeat && key != SDLK_UP && key != SDLK_DOWN &&
      key != SDLK_PAGEUP && key != SDLK_PAGEDOWN) return true;
  if (key == SDLK_TAB) {
    int delta = (mod & KMOD_SHIFT) ? -1 : 1;
    for (int attempts = 0; attempts < 3; ++attempts) {
      games_browser.focus = (games_browser.focus + delta + 3) % 3;
      bool active = games_browser.selected < games_browser.count &&
                    games_browser.rows[games_browser.selected].entry.status == STORE_OK &&
                    games_browser.rows[games_browser.selected].entry.result.status ==
                        SESSION_ACTIVE;
      if (games_browser.focus != 1 || active) break;
    }
  } else if (key == SDLK_UP || key == SDLK_DOWN ||
             key == SDLK_PAGEUP || key == SDLK_PAGEDOWN) {
    if (games_browser.count) {
      size_t step = key == SDLK_PAGEUP || key == SDLK_PAGEDOWN
                        ? (size_t)games_browser_layout(gfx).visible : 1u;
      size_t next = games_browser.selected;
      if (key == SDLK_UP || key == SDLK_PAGEUP)
        next = next > step ? next - step : 0;
      else
        next = next + step < games_browser.count
            ? next + step : games_browser.count - 1;
      games_browser_select(p, gfx, next);
      games_browser.focus = 0;
    }
  } else if (input_activation_key(key)) {
    if (games_browser.focus == 2) games_browser_close(p, game, ui);
    else (void)games_browser_activate(p, game, ui, gfx);
  }
  return true;
}

static bool games_browser_click(Persistence *p, Game *game, UI *ui,
                                 Gfx *gfx, int x, int y) {
  GamesBrowserLayout lay = games_browser_layout(gfx);
  if (games_browser.scanning) {
    if (point_in(lay.back, x, y)) games_browser_close(p, game, ui);
    return true;
  }
  if (point_in(lay.back, x, y)) {
    games_browser.focus = 2;
    games_browser_close(p, game, ui);
  } else if (point_in(lay.primary, x, y)) {
    games_browser.focus = 1;
    (void)games_browser_activate(p, game, ui, gfx);
  } else if (point_in(lay.list, x, y)) {
    int row = (y - lay.list.y) / lay.row_height;
    size_t index = games_browser.scroll + (size_t)row;
    if (index < games_browser.count) {
      games_browser.focus = 0;
      games_browser_select(p, gfx, index);
    }
  }
  return true;
}

static void games_browser_wheel(Persistence *p, Gfx *gfx, int direction) {
  if (!games_browser.count || games_browser.scanning) return;
  size_t next = games_browser.selected;
  if (direction > 0) next = next ? next - 1 : 0;
  else if (direction < 0)
    next = next + 1 < games_browser.count ? next + 1 : next;
  games_browser_select(p, gfx, next);
}
