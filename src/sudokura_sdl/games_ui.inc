/* Retained Games browser. The library, not this view, owns durable records. */
typedef struct {
  GameLibraryEntry entry;
  bool hot_only;
} GamesRow;

typedef struct {
  GamesRow *rows;
  size_t count, capacity, selected, scroll;
  StoreStatus scan_status;
  bool incomplete, modal_owned, preview_valid, scanning, detail_open;
  GameLibraryScanCursor scanner;
  SessionState preview;
  GameLibraryRecord detail_record;
  bool detail_record_valid;
  GameAttemptId pending_replay_id, pending_source_id;
  GameAttemptId focus_after_scan;
  bool detail_after_scan;
  int focus; /* 0: rows, 1: primary action, 2: Back */
} GamesBrowser;

static GamesBrowser games_browser;

static void games_append_text(char *destination, size_t capacity,
                              const char *suffix) {
  if (!destination || !suffix || !capacity) return;
  size_t used = strlen(destination);
  size_t added = strlen(suffix);
  if (used >= capacity || added >= capacity - used) return;
  memcpy(destination + used, suffix, added + 1u);
}

typedef struct {
  SDL_Rect title, list, detail, primary, back, replay, remove;
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
  bool compact_actions = gfx->width < 760;
  int action_gap = 8;
  int action_space = compact_actions ? 2 * buttons_h + action_gap : buttons_h;
  int list_y = header_y + 49;
  int list_h = bottom - action_space - 12 - detail_h - 12 - list_y;
  if (list_h < 48) list_h = 48;
  layout.list = (SDL_Rect){x, list_y, width, list_h};
  layout.row_height = gfx->height < 550 ? 44 : 52;
  layout.visible = list_h / layout.row_height;
  if (layout.visible < 1) layout.visible = 1;
  layout.detail = (SDL_Rect){x, list_y + list_h + 8, width, detail_h};
  int button_y = layout.detail.y + detail_h + 8;
  int gap = width < 640 ? 5 : 8;
  if (compact_actions) {
    int half = (width - gap) / 2;
    layout.back = (SDL_Rect){x,button_y,half,buttons_h};
    layout.primary = (SDL_Rect){x+half+gap,button_y,
                                width-half-gap,buttons_h};
    layout.replay = (SDL_Rect){x,button_y+buttons_h+action_gap,
                               half,buttons_h};
    layout.remove = (SDL_Rect){x+half+gap,button_y+buttons_h+action_gap,
                               width-half-gap,buttons_h};
  } else {
    int button_w = (width - gap * 3) / 4;
    layout.back = (SDL_Rect){x, button_y, button_w, buttons_h};
    layout.primary = (SDL_Rect){x + button_w + gap, button_y,
                                button_w, buttons_h};
    layout.replay = (SDL_Rect){x + (button_w + gap) * 2, button_y,
                               button_w, buttons_h};
    layout.remove = (SDL_Rect){x + (button_w + gap) * 3, button_y,
                               width - (button_w + gap) * 3, buttons_h};
  }
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
  games_browser.detail_record_valid = false;
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
                        &record) == STORE_OK) {
    games_browser.detail_record = record;
    games_browser.detail_record_valid = true;
    if (record.kind == GAME_RECORD_EXACT) {
      games_browser.preview = record.exact.session;
      games_browser.preview_valid = true;
    }
  }
}

static void games_browser_ensure_visible(const Gfx *gfx);

static bool games_browser_focus_attempt(const Persistence *p, Gfx *gfx,
                                        GameAttemptId id, bool open_detail) {
  for (size_t index = 0; index < games_browser.count; ++index) {
    const GamesRow *row = &games_browser.rows[index];
    if (!game_attempt_id_equal(row->entry.id, id)) continue;
    games_browser.selected = index;
    games_browser.detail_open = false;
    games_browser.focus = 0;
    games_browser_preview_selected(p);
    games_browser_ensure_visible(gfx);
    if (open_detail && row->entry.status == STORE_OK &&
        row->entry.result.status != SESSION_ACTIVE &&
        (row->entry.kind == GAME_RECORD_LEGACY_SUMMARY ||
         games_browser.preview_valid)) {
      games_browser.detail_open = true;
      games_browser.focus = 2;
    }
    return games_browser.detail_open || !open_detail;
  }
  return false;
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
  if (game_attempt_id_valid(games_browser.focus_after_scan)) {
    (void)games_browser_focus_attempt(p, gfx, games_browser.focus_after_scan,
                                      games_browser.detail_after_scan);
  }
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

static bool games_browser_open_focused(Persistence *p, Game *game,
                                       UI *ui, Gfx *gfx, GameAttemptId id,
                                       bool open_detail) {
  if (!game_attempt_id_valid(id) ||
      !games_browser_open(p, game, ui, gfx)) return false;
  games_browser.focus_after_scan = id;
  games_browser.detail_after_scan = open_detail;
  if (!games_browser.scanning &&
      !games_browser_focus_attempt(p, gfx, id, open_detail)) {
    games_browser_close(p, game, ui);
    return false;
  }
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

static bool games_browser_gate_replacement(Persistence *p, Game *game,
                                           UI *ui, Gfx *gfx, bool daily) {
  if (!persistence_flush(p)) {
    (void)show_simple_message_box(gfx, game, ui, SDL_MESSAGEBOX_WARNING,
                                  tr(ui->language, T_STORAGE_ERROR),
                                  tr(ui->language, T_SAVE_FAILED));
    return false;
  }
  const ProfileSlot *departing = daily ? &p->profile.daily : &p->profile.normal;
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
      (void)show_simple_message_box(gfx, game, ui, SDL_MESSAGEBOX_WARNING,
                                    tr(ui->language, T_STORAGE_ERROR),
                                    tr(ui->language, T_SAVE_FAILED));
      return false;
    }
  }
  return true;
}

/* Preserve a departing Daily before selecting a different original. */
static bool games_browser_retain_daily_for_rollover(Persistence *p) {
  if (!p || !p->enabled || !persistence_flush(p)) return false;
  ProfileSlot *slot = &p->profile.daily;
  if (!slot->present) return true;
  if (!game_attempt_id_valid(slot->attempt_id)) return false;
  if (!slot->retained && !games_browser_save_hot(p, true)) return false;
  GameLibraryRecord saved;
  if (game_library_load(p->profile_directory, slot->attempt_id, &saved) != STORE_OK)
    return false;
  return saved.kind == GAME_RECORD_EXACT &&
         saved.revision >= slot->revision &&
         saved.exact.session.is_daily &&
         saved.result.status == slot->value.session.status;
}

/* Canonical validation and durable hot-pointer commit happen before play. */
static bool games_browser_resume_daily(Persistence *p, Game *game, UI *ui,
                                       Gfx *gfx, GameAttemptId id,
                                       int year, int month, int day) {
  if (!p || !p->enabled || !game || !ui || !gfx) return false;
  GameLibraryRecord target;
  if (game_library_load(p->profile_directory, id, &target) != STORE_OK ||
      target.kind != GAME_RECORD_EXACT ||
      target.result.status != SESSION_ACTIVE || target.practice ||
      !target.original_daily ||
      target.exact.session.daily_year != year ||
      target.exact.session.daily_month != month ||
      target.exact.session.daily_day != day)
    return false;
  ProfileSlot *old = &p->profile.daily;
  if (old->present && game_attempt_id_equal(old->attempt_id, id) &&
      old->revision >= target.revision) {
    victory_animation_reset();
    restore_saved_session(p, game, ui, true);
    return true;
  }
  if (!games_browser_retain_daily_for_rollover(p)) return false;
  ProfileData candidate = p->profile;
  ProfileSlot *replacement = &candidate.daily;
  memset(replacement, 0, sizeof(*replacement));
  replacement->present = true;
  replacement->retained = target.retained;
  replacement->attempt_id = target.id;
  replacement->revision = target.revision;
  replacement->value = target.exact;
  candidate.preferences = p->preferences;
  if (!profile_validate(&candidate) ||
      profile_save_runtime_file(p->profile_path, p->profile_backup_path,
                                &candidate) != STORE_OK)
    return false;
  p->profile = candidate;
  p->game_cache[1].valid = false;
  persistence_refresh_normal(p);
  save_policy_init(&p->save_policy);
  victory_animation_reset();
  restore_saved_session(p, game, ui, true);
  return true;
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
  if (row.entry.result.status != SESSION_ACTIVE) {
    /* Never depict a damaged exact board as a healthy finished record.
       Legacy summaries are explicitly metadata-only and remain viewable. */
    if (row.entry.kind == GAME_RECORD_EXACT &&
        !games_browser.preview_valid) {
      (void)show_simple_message_box(
          gfx,game,ui,SDL_MESSAGEBOX_WARNING,
          tr(ui->language,T_GAMES),tr(ui->language,T_GAMES_UNAVAILABLE));
      return false;
    }
    games_browser.detail_open = true;
    games_browser.focus = 2;
    return true;
  }

  GameLibraryRecord target;
  bool valid = false;
  bool target_retained = true;
  if (row.hot_only) {
    const ProfileSlot *slots[2] = {&p->profile.normal, &p->profile.daily};
    for (int i = 0; i < 2; ++i) {
      if (slots[i]->present &&
          game_attempt_id_equal(slots[i]->attempt_id, row.entry.id)) {
        /* A hot-only autosave is valid to Resume without becoming retained. */
        target_retained = slots[i]->retained;
        valid = game_library_make_exact(&target, row.entry.id,
                                        slots[i]->revision,
                                        &slots[i]->value, true);
        break;
      }
    }
  } else {
    valid = game_library_load(p->profile_directory, row.entry.id,
                              &target) == STORE_OK;
    if (valid) target_retained = target.retained;
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
  if (!games_browser_gate_replacement(p, game, ui, gfx, daily))
    return false;
  ProfileData candidate = p->profile;
  ProfileSlot *slot = daily ? &candidate.daily : &candidate.normal;
  memset(slot, 0, sizeof(*slot));
  slot->present = true;
  slot->retained = target_retained;
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

static void games_browser_operation_error(Gfx *gfx, Game *game,
                                          UI *ui) {
  (void)show_simple_message_box(gfx,game,ui,SDL_MESSAGEBOX_WARNING,
                                tr(ui->language,T_STORAGE_ERROR),
                                tr(ui->language,T_GAMES_ACTION_FAILED));
}

/* Pending attempt ID stays stable when the record committed but its hot
   profile pointer could not be written. Retrying never forks again. */
static bool games_browser_replay_selected(Persistence *p,Game *game,
                                          UI *ui,Gfx *gfx) {
  if(!p||!p->enabled||games_browser.scanning||
     games_browser.selected>=games_browser.count)return false;
  const GamesRow row=games_browser.rows[games_browser.selected];
  if(row.hot_only||row.entry.status!=STORE_OK||
     row.entry.kind!=GAME_RECORD_EXACT||
     row.entry.result.status==SESSION_ACTIVE)return false;
  GameLibraryRecord source;
  if(game_library_load(p->profile_directory,row.entry.id,&source)!=STORE_OK ||
     source.kind!=GAME_RECORD_EXACT ||
     source.result.status==SESSION_ACTIVE) {
    games_browser_operation_error(gfx,game,ui);
    return false;
  }
  if(!games_browser_gate_replacement(p,game,ui,gfx,false))return false;

  GameLibraryRecord replay;
  GameLibraryHotCache new_cache={0};
  bool pending=game_attempt_id_valid(games_browser.pending_replay_id) &&
               game_attempt_id_equal(games_browser.pending_source_id,source.id);
  if(pending){
    if(game_library_load(p->profile_directory,
                         games_browser.pending_replay_id,&replay)!=STORE_OK ||
       !replay.practice || replay.result.status!=SESSION_ACTIVE ||
       !game_attempt_id_equal(replay.parent,source.id)){
      games_browser_operation_error(gfx,game,ui);
      return false;
    }
  }else{
    GameAttemptId fresh;
    if(!persistence_new_attempt_id(p,&fresh) ||
       !game_library_prepare_replay(&replay,&source,fresh) ||
       game_library_save_runtime(p->profile_directory,&replay,
                                 &new_cache)!=STORE_OK ||
       game_library_load(p->profile_directory,fresh,&replay)!=STORE_OK){
      games_browser_operation_error(gfx,game,ui);
      return false;
    }
    games_browser.pending_replay_id=fresh;
    games_browser.pending_source_id=source.id;
  }

  ProfileData candidate=p->profile;
  ProfileSlot *normal=&candidate.normal;
  memset(normal,0,sizeof(*normal));
  normal->present=true;
  normal->retained=true;
  normal->attempt_id=replay.id;
  normal->revision=replay.revision;
  normal->value=replay.exact;
  candidate.preferences=p->preferences;
  if(!profile_validate(&candidate) ||
     profile_save_runtime_file(p->profile_path,p->profile_backup_path,
                               &candidate)!=STORE_OK){
    games_browser_operation_error(gfx,game,ui);
    return false;
  }
  p->profile=candidate;
  p->game_cache[0]=new_cache;
  persistence_refresh_normal(p);
  save_policy_init(&p->save_policy);
  games_browser.modal_owned=false;
  game_library_scan_close(&games_browser.scanner);
  free(games_browser.rows);
  memset(&games_browser,0,sizeof(games_browser));
  victory_animation_reset();
  restore_saved_session(p,game,ui,false);
  return true;
}

/* Cancel is the native dialog's default/escape action; deleting is explicit. */
static bool games_browser_confirm_delete(Gfx *gfx,Game *game,UI *ui){
  return confirm_destructive_box(gfx,game,ui,
           tr(ui->language,T_GAMES_DELETE),
           tr(ui->language,T_GAMES_DELETE_PROMPT),
           tr(ui->language,T_GAMES_DELETE));
}

static bool games_browser_delete_selected(Persistence *p,Game *game,
                                          UI *ui,Gfx *gfx){
  if(!p||!p->enabled||games_browser.scanning||
     games_browser.selected>=games_browser.count)return false;
  GameAttemptId id=games_browser.rows[games_browser.selected].entry.id;
  if(!game_attempt_id_valid(id)||!games_browser_confirm_delete(gfx,game,ui))
    return false;
  if(!persistence_flush(p)){
    games_browser_operation_error(gfx,game,ui);
    return false;
  }
  ProfileData candidate;
  if(!game_library_forget_attempt(&candidate,&p->profile,id)){
    games_browser_operation_error(gfx,game,ui);
    return false;
  }
  bool deleting_live=ui->session_open &&
      game_attempt_id_equal(p->active_attempt_id,id);
  bool removed_normal=p->profile.normal.present &&
      game_attempt_id_equal(p->profile.normal.attempt_id,id);
  bool removed_daily=p->profile.daily.present &&
      game_attempt_id_equal(p->profile.daily.attempt_id,id);
  bool receipt_committed=false;
  StoreStatus cleanup=game_library_delete_with_receipt(
      p->profile_directory,id,&receipt_committed);
  StoreStatus receipt=game_library_receipt_status(p->profile_directory,id);
  if(!receipt_committed || receipt!=STORE_OK){
    games_browser_operation_error(gfx,game,ui);
    return false;
  }
  /* Receipt is durable even if unlink/profile update fails. Never stage
     this attempt again, and never let an older profile resurrect it. */
  if(deleting_live){
    ui->session_open=false;
    p->active_attempt_id=(GameAttemptId){0,0};
    persistence_set_replay_origin(p,NULL);
  }
  p->profile=candidate;
  if(removed_normal)p->game_cache[0].valid=false;
  if(removed_daily)p->game_cache[1].valid=false;
  persistence_refresh_normal(p);
  refresh_resume_info(p,ui);
  p->profile.preferences=p->preferences;
  bool profile_written=profile_save_runtime_file(
      p->profile_path,p->profile_backup_path,&p->profile)==STORE_OK;
  if(profile_written)save_policy_init(&p->save_policy);
  else{
    uint64_t now=now_ms();
    save_policy_mark(&p->save_policy,SAVE_DIRTY_RESULTS|
                     SAVE_DIRTY_NORMAL|SAVE_DIRTY_DAILY,now);
    save_policy_record_result(&p->save_policy,STORE_IO_ERROR,now);
  }
  if(deleting_live){
    games_browser.modal_owned=false;
    games_browser_close(p,game,ui);
    go_home_base(p,game,ui);
  }else{
    size_t index=games_browser.selected;
    if(index+1u<games_browser.count)
      memmove(&games_browser.rows[index],&games_browser.rows[index+1],
              (games_browser.count-index-1u)*sizeof(GamesRow));
    --games_browser.count;
    if(games_browser.selected>=games_browser.count)
      games_browser.selected=games_browser.count?games_browser.count-1u:0;
    games_browser.detail_open=false;
    games_browser.focus=games_browser.count?0:2;
    games_browser_preview_selected(p);
    games_browser_ensure_visible(gfx);
  }
  if(cleanup!=STORE_OK || !profile_written)
    (void)show_simple_message_box(gfx,game,ui,SDL_MESSAGEBOX_WARNING,
       tr(ui->language,T_STORAGE_ERROR),
       tr(ui->language,T_GAMES_DELETE_PENDING));
  return true;
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
  if (games_browser.detail_open) {
    const GamesRow *selected = &games_browser.rows[games_browser.selected];
    const ProfileResult *result = &selected->entry.result;
    int y = lay.title.y + lay.title.h + 7;
    int usable = lay.back.y - y - 92;
    int side = lay.list.w < usable ? lay.list.w : usable;
    if (side > 432) side = 432;
    if (side < 81) side = 81;
    int cell = side / 9;
    side = cell * 9;
    int x = (gfx->width - side) / 2;
    if (games_browser.preview_valid) {
      const Game *board = &games_browser.preview.game;
      SDL_Rect surface = {x, y, side, side};
      draw_round_rect(gfx->ren, surface, 4, theme.palette_bg);
      for (int index = 0; index < 81; ++index) {
        int px = x + index % 9 * cell;
        int py = y + index / 9 * cell;
        if (!board->puzzle[index]) continue;
        char number[2] = {(char)('0' + board->puzzle[index]), '\0'};
        SDL_Rect box = {px+1,py+1,cell-2,cell-2};
        SDL_Color ink = board->initial[index] ? theme.btnfg : theme.title;
        (void)draw_fitted_text(gfx, number, ink, box, cell-3, 10);
        if (board->hinted[index])
          draw_rect(gfx->ren, px+cell-5,py+2,2,2,theme.sel_outline);
      }
      SDL_SetRenderDrawColor(gfx->ren,theme.dim.r,theme.dim.g,theme.dim.b,255);
      for (int index = 0; index <= 9; ++index) {
        int thick = index % 3 == 0 ? 2 : 1;
        SDL_Rect vertical = {x+index*cell,y,thick,side};
        SDL_Rect horizontal = {x,y+index*cell,side,thick};
        SDL_RenderFillRect(gfx->ren,&vertical);
        SDL_RenderFillRect(gfx->ren,&horizontal);
      }
    } else {
      SDL_Rect empty = {lay.list.x,y,lay.list.w,side};
      (void)polish_draw_wrapped_fit(
          gfx,tr(ui->language,T_GAMES_SUMMARY_ONLY),
          theme.dim,empty,19,10,true);
    }
    unsigned long long seconds =
        (unsigned long long)(result->elapsed_ms / 1000u);
    char summary[256];
    (void)snprintf(summary,sizeof(summary),"%s · %s · %s · %02llu:%02llu%s%s",
        tr(ui->language,result->status==SESSION_WON?T_YOU_WIN:T_YOU_LOSE),
        tr(ui->language,mode_key(result->mode)),
        difficulty_name(ui->language,result->difficulty),
        seconds/60u,seconds%60u,
        result->assisted?" · ":"",
        result->assisted?tr(ui->language,T_ASSISTED):"");
    SDL_Rect info={lay.list.x,y+side+8,lay.list.w,
                   lay.back.y-(y+side+8)-8};
    int line_h=info.h>=96?23:18;
    SDL_Rect line={info.x,info.y,info.w,line_h};
    (void)draw_fitted_text(gfx,summary,theme.btnfg,line,16,10);
    int reveals=0;
    if(games_browser.preview_valid)
      for(int index=0;index<81;++index)
        if(games_browser.preview.game.hinted[index])++reveals;
    char details[256];
    (void)snprintf(details,sizeof(details),"%s: %u · %s: %u · %s: %d",
        tr(ui->language,T_ERRORS),(unsigned)result->mistakes,
        tr(ui->language,T_GAMES_STRIKES),(unsigned)result->strikes,
        tr(ui->language,T_GAMES_REVEALS),reveals);
    line.y+=line_h;
    (void)draw_fitted_text(gfx,details,theme.btnfg,line,14,10);
    char provenance[256];
    (void)snprintf(provenance,sizeof(provenance),"%s%s%s%s",
        result->assisted?tr(ui->language,T_ASSISTED):"",
        result->assisted && result->practice?" · ":"",
        result->practice?tr(ui->language,T_GAMES_PRACTICE):"",
        "");
    if(result->practice && games_browser.detail_record_valid){
      char id[33];
      game_attempt_id_hex(games_browser.detail_record.parent,id);
      char suffix[32];
      (void)snprintf(suffix, sizeof(suffix), " · #%.*s", 8, id);
      games_append_text(provenance, sizeof(provenance), suffix);
    }
    if(!provenance[0])
      (void)snprintf(provenance,sizeof(provenance),"%s",
                     tr(ui->language,T_GAMES_FINISHED));
    line.y+=line_h;
    (void)draw_fitted_text(gfx,provenance,theme.dim,line,13,10);
    char date[160];
    (void)snprintf(date,sizeof(date),"%s",
                   tr(ui->language,T_GAMES_UNKNOWN_DATE));
    uint64_t utc=selected->entry.finished_utc;
    time_t stamp=(time_t)utc;
    if(utc && stamp>0 && (uint64_t)stamp==utc){
      struct tm *finished=gmtime(&stamp);
      if(finished)
        (void)snprintf(date,sizeof(date),"%s: %04d-%02d-%02d",
            tr(ui->language,T_GAMES_FINISHED_AT),
            finished->tm_year+1900,finished->tm_mon+1,finished->tm_mday);
    }
    if(games_browser.detail_record_valid &&
       games_browser.detail_record.original_daily){
      const GameLibraryRecord *record=&games_browser.detail_record;
      char suffix[128];
      (void)snprintf(suffix, sizeof(suffix), " · %.*s %04u-%02u-%02u",
          48, tr(ui->language,T_GAMES_SOURCE_DAILY),
          (unsigned)record->original_daily_year,
          (unsigned)record->original_daily_month,
          (unsigned)record->original_daily_day);
      games_append_text(date, sizeof(date), suffix);
    }
    line.y+=line_h;
    if(line.y+line_h<=lay.back.y)
      (void)draw_fitted_text(gfx,date,theme.dim,line,13,10);
    bool can_replay=!selected->hot_only &&
        selected->entry.kind==GAME_RECORD_EXACT &&
        selected->entry.status==STORE_OK;
    games_browser_draw_button(gfx,theme,lay.back,tr(ui->language,T_BACK),
                              games_browser.focus==2,true,theme.btnfg);
    games_browser_draw_button(gfx,theme,lay.replay,
                              tr(ui->language,T_GAMES_REPLAY),
                              games_browser.focus==3,can_replay,theme.btnfg);
    games_browser_draw_button(gfx,theme,lay.remove,
                              tr(ui->language,T_GAMES_DELETE),
                              games_browser.focus==4,true,theme.btnfg);
    return;
  }
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
    /* Keep list labels short. The full diagnostic is shown below.
       Long status sentences must not disappear on compact viewports. */
    const char *status = e->status != STORE_OK
        ? tr(ui->language, T_UNAVAILABLE)
        : e->result.practice ? tr(ui->language, T_GAMES_PRACTICE)
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
    int heading_h = row->entry.status == STORE_OK ? 28 : 44;
    line.h = heading_h;
    if (row->entry.status == STORE_OK)
      (void)draw_fitted_text(gfx, detail, theme.btnfg, line, 16, 10);
    else
      (void)polish_draw_wrapped_fit(gfx, detail, theme.btnfg,
                                   line, 16, 10, true);
    line.y += heading_h;
    line.h = lay.detail.h - heading_h;
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
  bool can_open = !games_browser.scanning &&
                  games_browser.selected < games_browser.count &&
                  games_browser.rows[games_browser.selected].entry.status == STORE_OK;
  games_browser_draw_button(gfx, theme, lay.primary,
                            tr(ui->language, active ? T_GAMES_LOAD : T_GAMES_VIEW),
                            games_browser.focus == 1, can_open, theme.btnfg);
  bool can_replay=can_open &&
      !games_browser.rows[games_browser.selected].hot_only &&
      games_browser.rows[games_browser.selected].entry.kind==GAME_RECORD_EXACT &&
      games_browser.rows[games_browser.selected].entry.result.status!=SESSION_ACTIVE;
  games_browser_draw_button(gfx, theme, lay.replay,
                            tr(ui->language, T_GAMES_REPLAY),
                            games_browser.focus == 3, can_replay,theme.btnfg);
  games_browser_draw_button(gfx, theme, lay.remove,
                            tr(ui->language, T_GAMES_DELETE),
                            games_browser.focus == 4,
                            !games_browser.scanning && games_browser.count>0,
                            theme.btnfg);
}

static bool games_browser_focus_enabled(int focus) {
  if(focus==2)return true;
  if(games_browser.scanning||games_browser.selected>=games_browser.count)
    return false;
  const GamesRow *row=&games_browser.rows[games_browser.selected];
  if(focus==4)return true;
  if(focus==3)
    return !row->hot_only && row->entry.status==STORE_OK &&
           row->entry.kind==GAME_RECORD_EXACT &&
           row->entry.result.status!=SESSION_ACTIVE;
  if(games_browser.detail_open)return false;
  if(focus==0)return true;
  return focus==1 && row->entry.status==STORE_OK;
}

static bool games_browser_activate_focus(Persistence *p,Game *game,
                                          UI *ui,Gfx *gfx) {
  switch(games_browser.focus){
    case 2:
      if(games_browser.detail_open){
        games_browser.detail_open=false;
        games_browser.focus=0;
      }else games_browser_close(p,game,ui);
      return true;
    case 1:
    case 0:
      if(!games_browser.detail_open)
        return games_browser_activate(p,game,ui,gfx);
      return false;
    case 3:
      return games_browser_focus_enabled(3) &&
             games_browser_replay_selected(p,game,ui,gfx);
    case 4:
      return games_browser_focus_enabled(4) &&
             games_browser_delete_selected(p,game,ui,gfx);
    default: return false;
  }
}

static bool games_browser_key(Persistence *p,Game *game,UI *ui,Gfx *gfx,
                               SDL_Keycode key,SDL_Keymod mod,bool repeat) {
  if(key==SDLK_ESCAPE){
    games_browser.focus=2;
    (void)games_browser_activate_focus(p,game,ui,gfx);
    return true;
  }
  if(games_browser.scanning || !games_browser.count){
    if(!repeat && input_activation_key(key)){
      games_browser.focus=2;
      (void)games_browser_activate_focus(p,game,ui,gfx);
    }
    return true;
  }
  if(key==SDLK_TAB && !repeat){
    int delta=(mod & KMOD_SHIFT)?-1:1;
    for(int attempts=0;attempts<5;++attempts){
      games_browser.focus=(games_browser.focus+delta+5)%5;
      if(games_browser_focus_enabled(games_browser.focus))break;
    }
  }else if(!games_browser.detail_open &&
           (key==SDLK_UP||key==SDLK_DOWN||
            key==SDLK_PAGEUP||key==SDLK_PAGEDOWN)){
    size_t step=(key==SDLK_PAGEUP||key==SDLK_PAGEDOWN)
        ?(size_t)games_browser_layout(gfx).visible:1u;
    size_t next=games_browser.selected;
    if(key==SDLK_UP||key==SDLK_PAGEUP)
      next=next>step?next-step:0;
    else
      next=next+step<games_browser.count
          ?next+step:games_browser.count-1;
    games_browser_select(p,gfx,next);
    games_browser.focus=0;
  }else if(!repeat && key==SDLK_DELETE && games_browser_focus_enabled(4)){
    games_browser.focus=4;
    (void)games_browser_activate_focus(p,game,ui,gfx);
  }else if(!repeat && key==SDLK_r && persistence_has_save_error(p)){
    (void)persistence_flush(p);
  }else if(!repeat && input_activation_key(key)){
    (void)games_browser_activate_focus(p,game,ui,gfx);
  }
  return true;
}

static bool games_browser_click(Persistence *p,Game *game,UI *ui,
                                 Gfx *gfx,int x,int y) {
  GamesBrowserLayout lay=games_browser_layout(gfx);
  if(point_in(lay.back,x,y)){
    games_browser.focus=2;
    (void)games_browser_activate_focus(p,game,ui,gfx);
    return true;
  }
  if(games_browser.scanning)return true;
  if(point_in(lay.replay,x,y)){
    games_browser.focus=3;
    (void)games_browser_activate_focus(p,game,ui,gfx);
  }else if(point_in(lay.remove,x,y)){
    games_browser.focus=4;
    (void)games_browser_activate_focus(p,game,ui,gfx);
  }else if(games_browser.detail_open){
    return true;
  }else if(point_in(lay.primary,x,y)){
    games_browser.focus=1;
    (void)games_browser_activate_focus(p,game,ui,gfx);
  }else if(point_in(lay.list,x,y)){
    int row=(y-lay.list.y)/lay.row_height;
    size_t index=games_browser.scroll+(size_t)row;
    if(index<games_browser.count){
      games_browser.focus=0;
      games_browser_select(p,gfx,index);
    }
  }
  return true;
}

static void games_browser_wheel(Persistence *p, Gfx *gfx, int direction) {
  if (!games_browser.count || games_browser.scanning ||
      games_browser.detail_open) return;
  size_t next = games_browser.selected;
  if (direction > 0) next = next ? next - 1 : 0;
  else if (direction < 0)
    next = next + 1 < games_browser.count ? next + 1 : next;
  games_browser_select(p, gfx, next);
}
