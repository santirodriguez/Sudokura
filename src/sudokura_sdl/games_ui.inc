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
  GameAttemptId focus_after_scan;
  bool detail_after_scan;
  int focus; /* 0: rows, 1: primary action, 2: Back */
} GamesBrowser;

static GamesBrowser games_browser;

typedef struct {
  SDL_Rect title, subtitle, list, detail, primary, back, replay, remove;
  int row_height, visible;
  bool side_by_side;
} GamesBrowserLayout;

static GamesBrowserLayout games_browser_layout(const Gfx *gfx) {
  GamesBrowserLayout layout;
  memset(&layout,0,sizeof(layout));
  int margin = gfx->width < 640 ? 12 : 24;
  int width = gfx->width - margin*2;
  int max_width = gfx->width >= 1600 ? 1480 : 1240;
  if (width > max_width) width = max_width;
  int x = (gfx->width - width)/2;
  int header_y = gfx->height < 520 ? 51 : 69;
  layout.title = (SDL_Rect){x,header_y,width,39};
  layout.subtitle = (SDL_Rect){x,header_y + 42,width,38};
  int action_gap = 8, button_height = 44;
  bool compact_actions = width < 760;
  int action_space = compact_actions ? 2*button_height + action_gap
                                     : button_height;
  int buttons_y = gfx->height - margin - action_space;
  int content_y = header_y + 83;
  int content_h = buttons_y - content_y - 12;
  if (content_h < 105) content_h = 105;
  layout.side_by_side = width >= 860 && content_h >= 320;
  if (layout.side_by_side) {
    int space = 14;
    int left_width = (width-space)*44/100;
    layout.list = (SDL_Rect){x,content_y,left_width,content_h};
    layout.detail = (SDL_Rect){x+left_width+space,content_y,
                                width-left_width-space,content_h};
  } else {
    int detail_h = content_h*40/100;
    if (detail_h < 86) detail_h = 86;
    if (detail_h > 206) detail_h = 206;
    int list_h = content_h-detail_h-10;
    if (list_h < 48) list_h = 48;
    layout.list = (SDL_Rect){x,content_y,width,list_h};
    layout.detail = (SDL_Rect){x,content_y+list_h+10,width,detail_h};
  }
  layout.row_height = gfx->height < 550 ? 51 : 64;
  layout.visible = layout.list.h / layout.row_height;
  if (layout.visible < 1) layout.visible = 1;
  int gap = width < 640 ? 6 : 10;
  if (compact_actions) {
    int half = (width-gap)/2;
    layout.back = (SDL_Rect){x,buttons_y,half,button_height};
    layout.primary = (SDL_Rect){x+half+gap,buttons_y,
                                 width-half-gap,button_height};
    layout.replay = (SDL_Rect){x,buttons_y+button_height+action_gap,
                                half,button_height};
    layout.remove = (SDL_Rect){x+half+gap,
                                buttons_y+button_height+action_gap,
                                width-half-gap,button_height};
  } else {
    int bw = (width-gap*3)/4;
    layout.back = (SDL_Rect){x,buttons_y,bw,button_height};
    layout.primary = (SDL_Rect){x+bw+gap,buttons_y,bw,button_height};
    layout.replay = (SDL_Rect){x+(bw+gap)*2,buttons_y,bw,button_height};
    layout.remove = (SDL_Rect){x+(bw+gap)*3,buttons_y,
                                width-(bw+gap)*3,button_height};
  }
  return layout;
}

static SDL_Rect games_browser_row_rect(GamesBrowserLayout layout, int row) {
  return (SDL_Rect){layout.list.x, layout.list.y + row * layout.row_height,
                     layout.list.w, layout.row_height};
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
  if (!p || !p->enabled) return false;
  unsigned bit = daily ? SAVE_DIRTY_DAILY : SAVE_DIRTY_NORMAL;
  ProfileSlot *slot = daily ? &p->profile.daily : &p->profile.normal;
  if (!slot->present || !game_attempt_id_valid(slot->attempt_id) ||
      slot->revision == UINT64_MAX) return false;
  if (slot->retained) return (p->keep_pending_mask & bit) == 0;
  if (p->capture_failed || (p->keep_pending_mask & ~bit) != 0)
    return false; /* A different pending record must not be acknowledged. */

  bool retry_pending = (p->keep_pending_mask & bit) != 0;
  p->keep_pending_mask |= bit;
  uint64_t started = now_ms();
  save_policy_mark(&p->save_policy, bit, started);
  save_policy_record_result(&p->save_policy, STORE_IO_ERROR, started);

  ProfileSlot retained;
  if (game_library_retain_hot_slot_runtime(
          p->profile_directory, slot, &p->game_cache[daily ? 1 : 0],
          retry_pending, &retained) != STORE_OK)
    return false;

  ProfileData candidate = p->profile;
  *(daily ? &candidate.daily : &candidate.normal) = retained;
  candidate.preferences = p->preferences;
  ProfileData disk;
  StoreStatus status = profile_load_file(p->profile_path, &disk);
  if (status != STORE_OK) return false; /* No reset over damaged storage. */

  /* A previous profile write may have renamed successfully before its
     directory-sync acknowledgement failed. Re-acknowledge the same bytes
     without rotating the last known good profile backup. */
  bool identical = profile_encoded_equal(&candidate, &disk);
  status = profile_save_runtime_file(
      p->profile_path, identical ? NULL : p->profile_backup_path,
      &candidate);
  if (status != STORE_OK ||
      profile_load_file(p->profile_path, &disk) != STORE_OK ||
      !profile_encoded_equal(&candidate, &disk))
    return false;

  p->profile = candidate;
  p->keep_pending_mask &= ~bit;
  p->game_cache[daily ? 1 : 0].valid = false;
  persistence_refresh_normal(p);
  /* The complete profile also committed preferences and recent results.
     Do not flush it a second time and rotate its backup on success. */
  save_policy_record_result(&p->save_policy, STORE_OK, now_ms());
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
    /* A visible Saved confirmation is a durable claim. Check both the full
       archive and the exact hot profile, including notes and entered digits. */
    const ProfileSlot *slot = ui->is_daily
        ? &p->profile.daily : &p->profile.normal;
    ProfileData disk;
    ok = game_library_verify_retained_snapshot(
             p->profile_directory,slot) == STORE_OK &&
         profile_load_file(p->profile_path,&disk) == STORE_OK &&
         profile_encoded_equal(&disk,&p->profile);
  }
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

/* Daily's Home preflight also needs idempotent profile acknowledgement.
   Do not rotate the prior good copy on a repeated post-rename write. */
static bool games_browser_daily_flush_verified(Persistence *p) {
  if (!p || !p->enabled || p->capture_failed || p->keep_pending_mask)
    return false;
  if (!save_policy_dirty(&p->save_policy)) return true;
  ProfileData candidate = p->profile;
  candidate.preferences = p->preferences;
  StoreStatus status = profile_save_runtime_verified(
      p->profile_path, p->profile_backup_path, &candidate);
  save_policy_record_result(&p->save_policy, status, now_ms());
  if (status == STORE_OK) p->profile.preferences = p->preferences;
  return status == STORE_OK;
}

typedef enum {
  GAMES_CHECKPOINT_BACK = 0,
  GAMES_CHECKPOINT_READY,
  GAMES_CHECKPOINT_DISCARD
} GamesCheckpointGate;

static GamesCheckpointGate games_browser_checkpoint_gate(
    Persistence *p, Game *game, UI *ui, Gfx *gfx,
    bool daily, bool allow_discard, bool verified_daily,
    TextKey retry_prompt) {
  if (!p || !p->enabled || !game || !ui || !gfx)
    return GAMES_CHECKPOINT_BACK;
  bool flushed = verified_daily
      ? games_browser_daily_flush_verified(p) : persistence_flush(p);
  if (flushed && !p->capture_failed &&
      p->keep_pending_mask == 0 && !save_policy_dirty(&p->save_policy))
    return GAMES_CHECKPOINT_READY;
  for (;;) {
    const ProfileSlot *slot = daily ? &p->profile.daily : &p->profile.normal;
    bool unretained_active = slot->present && !slot->retained &&
                              slot->value.session.status == SESSION_ACTIVE;
    bool can_discard = allow_discard &&
        save_policy_replacement_discard_allowed(
            &p->save_policy, daily, unretained_active,
            p->capture_failed, p->keep_pending_mask);
    int choice = can_discard
        ? choice_box(gfx, game, ui,
                     tr(ui->language, T_UNSAVED_TITLE),
                     tr(ui->language, T_GAMES_SAFE_DISCARD_PROMPT),
                     tr(ui->language, T_BACK),
                     tr(ui->language, T_RETRY_SAVE),
                     tr(ui->language, T_DISCARD))
        : (confirm_box(gfx, game, ui,
                       tr(ui->language, T_UNSAVED_TITLE),
                       tr(ui->language, retry_prompt),
                       tr(ui->language, T_RETRY_SAVE),
                       tr(ui->language, T_BACK)) ? 1 : 0);
    if (choice == 0) return GAMES_CHECKPOINT_BACK;
    if (choice == 2) {
      if (can_discard && save_policy_replacement_discard_allowed(
              &p->save_policy, daily, unretained_active,
              p->capture_failed, p->keep_pending_mask))
        return GAMES_CHECKPOINT_DISCARD;
      return GAMES_CHECKPOINT_BACK;
    }
    bool retried = verified_daily && !p->capture_failed &&
                   p->keep_pending_mask == 0
        ? games_browser_daily_flush_verified(p)
        : persistence_retry_current(p, game, ui);
    if (retried && !p->capture_failed && p->keep_pending_mask == 0 &&
        !save_policy_dirty(&p->save_policy))
      return GAMES_CHECKPOINT_READY;
  }
}

static bool games_browser_commit_profile(Persistence *p, Game *game,
                                         UI *ui, Gfx *gfx, bool daily,
                                         const ProfileData *candidate,
                                         bool restore_on_back) {
  if (!p || !p->enabled || !candidate) return false;
  for (;;) {
    if (profile_save_runtime_verified(
            p->profile_path, p->profile_backup_path, candidate) == STORE_OK)
      return true;
    /* Even a post-rename directory-sync failure leaves the old live game
       intact. Keep its dirty authority until a confirmed write or recovery. */
    uint64_t failed_at = now_ms();
    save_policy_mark(&p->save_policy,
                     daily ? SAVE_DIRTY_DAILY : SAVE_DIRTY_NORMAL, failed_at);
    save_policy_record_result(&p->save_policy, STORE_IO_ERROR, failed_at);
    if (!confirm_box(gfx, game, ui, tr(ui->language, T_STORAGE_ERROR),
                     tr(ui->language, T_GAMES_ACTIVATE_RETRY_PROMPT),
                     tr(ui->language, T_RETRY_SAVE),
                     tr(ui->language, T_BACK))) {
      if (restore_on_back) {
        StoreStatus restored = profile_restore_runtime_activation(
            p->profile_path, &p->profile, candidate);
        if (restored == STORE_OK)
          save_policy_init(&p->save_policy);
        else
          (void)show_simple_message_box(
              gfx, game, ui, SDL_MESSAGEBOX_WARNING,
              tr(ui->language, T_STORAGE_ERROR),
              tr(ui->language, T_DAILY_BACK_UNCONFIRMED));
      }
      return false;
    }
  }
}

static bool games_browser_gate_replacement(Persistence *p, Game *game,
                                           UI *ui, Gfx *gfx, bool daily) {
  GamesCheckpointGate gate = games_browser_checkpoint_gate(
      p, game, ui, gfx, daily, true, false, T_GAMES_RETRY_SAVE_PROMPT);
  if (gate == GAMES_CHECKPOINT_BACK) return false;
  if (gate == GAMES_CHECKPOINT_DISCARD) return true;
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
      /* Retention may have written a record before its profile link failed.
         Retry the same Save; never turn this into a silent Discard. */
      for (;;) {
        if (!confirm_box(gfx, game, ui,
                         tr(ui->language, T_STORAGE_ERROR),
                         tr(ui->language, T_GAMES_RETRY_SAVE_PROMPT),
                         tr(ui->language, T_RETRY_SAVE),
                         tr(ui->language, T_BACK)))
          return false;
        if (persistence_retry_current(p, game, ui) &&
            (daily ? p->profile.daily.retained : p->profile.normal.retained))
          break;
      }
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

static bool games_browser_retain_daily_with_retry(
    Persistence *p, Game *game, UI *ui, Gfx *gfx) {
  if (games_browser_checkpoint_gate(p, game, ui, gfx, true, false, true,
                                    T_DAILY_SAVE_RETRY_PROMPT) !=
      GAMES_CHECKPOINT_READY) return false;
  while (true) {
    if (games_browser_retain_daily_for_rollover(p)) return true;
    if (!confirm_box(gfx, game, ui,
                     tr(ui->language, T_UNSAVED_TITLE),
                     tr(ui->language, T_DAILY_SAVE_RETRY_PROMPT),
                     tr(ui->language, T_RETRY_SAVE),
                     tr(ui->language, T_BACK))) return false;
    (void)persistence_retry_current(p, game, ui);
  }
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
      target.exact.session.daily_day != day) {
    (void)show_simple_message_box(
        gfx, game, ui, SDL_MESSAGEBOX_WARNING,
        tr(ui->language, T_DAILY), tr(ui->language, T_DAILY_READ_ERROR));
    return false;
  }
  ProfileSlot *old = &p->profile.daily;
  if (old->present && game_attempt_id_equal(old->attempt_id, id) &&
      old->revision >= target.revision) {
    victory_animation_reset();
    restore_saved_session(p, game, ui, true);
    return true;
  }
  if (!games_browser_retain_daily_with_retry(p, game, ui, gfx))
    return false;
  ProfileData candidate = p->profile;
  ProfileSlot *replacement = &candidate.daily;
  memset(replacement, 0, sizeof(*replacement));
  replacement->present = true;
  replacement->retained = target.retained;
  replacement->attempt_id = target.id;
  replacement->revision = target.revision;
  replacement->value = target.exact;
  candidate.preferences = p->preferences;
  if (!profile_validate(&candidate)) {
    (void)show_simple_message_box(
        gfx, game, ui, SDL_MESSAGEBOX_WARNING,
        tr(ui->language, T_DAILY), tr(ui->language, T_GAMES_ACTION_FAILED));
    return false;
  }
  if (!games_browser_commit_profile(
          p, game, ui, gfx, true, &candidate, true))
    return false;
  p->profile = candidate;
  p->game_cache[1].valid = false;
  persistence_refresh_normal(p);
  save_policy_init(&p->save_policy);
  victory_animation_reset();
  restore_saved_session(p, game, ui, true);
  return true;
}

/* Recover one validated earlier library checkpoint on explicit user input.
   This does not publish a hot slot or replace the current board. */
static void games_browser_operation_error(Gfx *gfx, Game *game, UI *ui);

static bool games_browser_recover_selected(Persistence *p, Game *game,
                                           UI *ui, Gfx *gfx) {
  if (!p || !p->enabled || !game || !ui || !gfx ||
      games_browser.scanning || games_browser.selected >= games_browser.count)
    return false;
  GamesRow *row = &games_browser.rows[games_browser.selected];
  if (row->hot_only || !game_library_entry_recoverable(&row->entry))
    return false;
  if (!confirm_destructive_box(gfx, game, ui,
                               tr(ui->language, T_GAMES),
                               tr(ui->language, T_GAMES_RECOVER_PROMPT),
                               tr(ui->language, T_RECOVER)))
    return false;
  GameLibraryRecord current;
  StoreStatus primary = game_library_load(
      p->profile_directory, row->entry.id, &current);
  /* Stale catalogue metadata cannot justify overriding a healthy,
     incompatible or unreadable primary record. */
  if (primary != STORE_CORRUPT && primary != STORE_NOT_FOUND) {
    games_browser_operation_error(gfx, game, ui);
    return false;
  }
  GameLibraryRecord recovered;
  if (game_library_recover_previous(p->profile_directory, row->entry.id,
                                    &recovered) != STORE_OK) {
    games_browser_operation_error(gfx, game, ui);
    return false;
  }
  row->entry.kind = recovered.kind;
  row->entry.result = recovered.result;
  row->entry.revision = recovered.revision;
  row->entry.created_utc = recovered.created_utc;
  row->entry.updated_utc = recovered.updated_utc;
  row->entry.finished_utc = recovered.finished_utc;
  row->entry.status = STORE_OK;
  row->entry.previous_status = STORE_OK;
  row->entry.only_backup = false;
  games_browser.detail_open = false;
  games_browser.focus = 0;
  games_browser_preview_selected(p);
  (void)show_simple_message_box(gfx, game, ui, SDL_MESSAGEBOX_INFORMATION,
                                tr(ui->language, T_GAMES),
                                tr(ui->language, T_GAMES_RECOVERED));
  return true;
}

static bool games_browser_activate(Persistence *p, Game *game, UI *ui,
                                   Gfx *gfx) {
  if (games_browser.scanning || games_browser.selected >= games_browser.count ||
      !p || !p->enabled)
    return false;
  GamesRow row = games_browser.rows[games_browser.selected];
  if (row.entry.status != STORE_OK) {
    if (!row.hot_only && game_library_entry_recoverable(&row.entry))
      return games_browser_recover_selected(p, game, ui, gfx);
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
  bool same_attempt = departing->present &&
      game_attempt_id_equal(departing->attempt_id, target.id);
  bool prefer_record = !same_attempt;
  if (same_attempt && !row.hot_only) {
    StoreStatus resolution = game_library_resolve_hot_record(
        departing, &target, &prefer_record);
    if (resolution != STORE_OK) {
      games_browser_operation_error(gfx, game, ui);
      return false;
    }
  }
  if (same_attempt && !prefer_record) {
    bool play_current = ui->prev_screen == SCR_PLAY && ui->session_open &&
                        ui->is_daily == daily &&
                        game_attempt_id_equal(p->active_attempt_id, target.id);
    games_browser_close(p, game, ui);
    if (!play_current) restore_saved_session(p, game, ui, daily);
    return true;
  }
  /* Reconcile newer disk state without ever offering Discard of this same
     attempt. After Retry, resolve again in case the live slot advanced. */
  if (same_attempt) {
    if (games_browser_checkpoint_gate(
            p, game, ui, gfx, daily, false, false,
            T_GAMES_RETRY_SAVE_PROMPT) != GAMES_CHECKPOINT_READY)
      return false;
    if (game_library_load(p->profile_directory, target.id, &target) != STORE_OK ||
        game_library_resolve_hot_record(
            daily ? &p->profile.daily : &p->profile.normal,
            &target, &prefer_record) != STORE_OK) {
      games_browser_operation_error(gfx, game, ui);
      return false;
    }
    if (!prefer_record) {
      bool play_current = ui->prev_screen == SCR_PLAY && ui->session_open &&
                          ui->is_daily == daily &&
                          game_attempt_id_equal(p->active_attempt_id, target.id);
      games_browser_close(p, game, ui);
      if (!play_current) restore_saved_session(p, game, ui, daily);
      return true;
    }
  } else if (!games_browser_gate_replacement(p, game, ui, gfx, daily)) {
    return false;
  }
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
  if (!games_browser_commit_profile(
          p, game, ui, gfx, daily, &candidate, false))
    return false;
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

/* The intent is a tiny durable operation marker, not a collection index.
   A failed profile write can be retried after Games closes or Sudokura
   restarts without allocating a second practice attempt. */
static bool games_browser_replay_selected(Persistence *p, Game *game,
                                          UI *ui, Gfx *gfx) {
  if (!p || !p->enabled || !game || !ui || !gfx ||
      games_browser.scanning || games_browser.selected >= games_browser.count)
    return false;
  const GamesRow row = games_browser.rows[games_browser.selected];
  if (row.hot_only || row.entry.status != STORE_OK ||
      row.entry.kind != GAME_RECORD_EXACT ||
      row.entry.result.status == SESSION_ACTIVE)
    return false;

  GameLibraryRecord source;
  if (game_library_load(p->profile_directory, row.entry.id, &source) !=
          STORE_OK ||
      source.kind != GAME_RECORD_EXACT ||
      source.result.status == SESSION_ACTIVE) {
    games_browser_operation_error(gfx, game, ui);
    return false;
  }
  if (!games_browser_gate_replacement(p, game, ui, gfx, false))
    return false;

  GameReplayIntent intent = {0};
  StoreStatus status = game_library_replay_intent_load(
      p->profile_directory, &intent);
  if (status != STORE_OK && status != STORE_NOT_FOUND) {
    games_browser_operation_error(gfx, game, ui);
    return false;
  }
  bool pending = status == STORE_OK &&
                 intent.state == GAME_REPLAY_PREPARED;

  if (pending) {
    GameLibraryRecord interrupted;
    StoreStatus record_status = game_library_load(
        p->profile_directory, intent.attempt_id, &interrupted);
    bool linked = p->profile.normal.present &&
        p->profile.normal.retained &&
        game_attempt_id_equal(p->profile.normal.attempt_id,
                              intent.attempt_id);
    StoreStatus receipt = record_status == STORE_NOT_FOUND
        ? game_library_receipt_status(p->profile_directory,
                                      intent.attempt_id)
        : STORE_NOT_FOUND;
    if (receipt != STORE_NOT_FOUND && receipt != STORE_OK) {
      games_browser_operation_error(gfx, game, ui);
      return false;
    }
    if (linked) {
      bool resume_selected = false;
      if (record_status != STORE_OK ||
          game_library_replay_linked_selection(
              &intent, source.id, &p->profile.normal, &interrupted,
              &resume_selected) != STORE_OK) {
        games_browser_operation_error(gfx, game, ui);
        return false;
      }
      /* A previously committed hot pointer settles the old intent, but
         must never override the user's distinct newly selected source. */
      intent.state = GAME_REPLAY_COMMITTED;
      if (game_library_replay_intent_write(p->profile_directory, &intent) !=
          STORE_OK) {
        games_browser_operation_error(gfx, game, ui);
        return false;
      }
      if (resume_selected) {
        games_browser.modal_owned = false;
        game_library_scan_close(&games_browser.scanner);
        free(games_browser.rows);
        memset(&games_browser, 0, sizeof(games_browser));
        victory_animation_reset();
        restore_saved_session(p, game, ui, false);
        return true;
      }
      /* Start a fresh Replay of the requested source. The old committed
         practice attempt and its profile record remain discoverable. */
      pending = false;
    } else if (!game_attempt_id_equal(intent.source_id, source.id)) {
      if (!confirm_destructive_box(gfx, game, ui,
             tr(ui->language, T_GAMES_REPLAY),
             tr(ui->language, T_GAMES_REPLAY_PENDING_OTHER),
             tr(ui->language, T_DISCARD)))
        return false;
      intent.state = GAME_REPLAY_ABANDONED;
      if (game_library_replay_intent_write(p->profile_directory, &intent) !=
          STORE_OK) {
        games_browser_operation_error(gfx, game, ui);
        return false;
      }
      pending = false;
    } else if ((record_status == STORE_OK &&
                 !game_library_replay_pending_matches(&source, &interrupted)) ||
               receipt == STORE_OK) {
      /* It might be a genuine older practice attempt. Never overwrite it
         or silently call it an unfinished duplicate. */
      if (!confirm_destructive_box(gfx, game, ui,
             tr(ui->language, T_GAMES_REPLAY),
             tr(ui->language, T_GAMES_REPLAY_PENDING_CONFLICT),
             tr(ui->language, T_DISCARD)))
        return false;
      intent.state = GAME_REPLAY_ABANDONED;
      if (game_library_replay_intent_write(p->profile_directory, &intent) !=
          STORE_OK) {
        games_browser_operation_error(gfx, game, ui);
        return false;
      }
      pending = false;
    } else if (record_status != STORE_NOT_FOUND &&
               record_status != STORE_OK) {
      games_browser_operation_error(gfx, game, ui);
      return false;
    }
  }

  GameLibraryRecord replay;
  GameLibraryHotCache new_cache = {0};
  if (!pending) {
    GameAttemptId fresh;
    if (!persistence_new_attempt_id(p, &fresh)) {
      games_browser_operation_error(gfx, game, ui);
      return false;
    }
    intent = (GameReplayIntent){source.id, fresh, GAME_REPLAY_PREPARED};
    if (game_library_replay_intent_write(p->profile_directory, &intent) !=
        STORE_OK) {
      games_browser_operation_error(gfx, game, ui);
      return false;
    }
  }

  status = game_library_load(p->profile_directory, intent.attempt_id, &replay);
  if (status == STORE_NOT_FOUND) {
    /* A crash after intent publication but before record commit reuses the
       reserved attempt ID. No second Replay gets silently created. */
    if (!game_library_prepare_replay(&replay, &source, intent.attempt_id) ||
        game_library_save_runtime(p->profile_directory, &replay, &new_cache) !=
            STORE_OK ||
        game_library_load(p->profile_directory, intent.attempt_id, &replay) !=
            STORE_OK) {
      games_browser_operation_error(gfx, game, ui);
      return false;
    }
  } else if (status != STORE_OK) {
    games_browser_operation_error(gfx, game, ui);
    return false;
  }
  if (!game_library_replay_pending_matches(&source, &replay)) {
    games_browser_operation_error(gfx, game, ui);
    return false;
  }

  ProfileData candidate = p->profile;
  ProfileSlot *normal = &candidate.normal;
  memset(normal, 0, sizeof(*normal));
  normal->present = true;
  normal->retained = true;
  normal->attempt_id = replay.id;
  normal->revision = replay.revision;
  normal->value = replay.exact;
  candidate.preferences = p->preferences;
  if (!profile_validate(&candidate)) {
    games_browser_operation_error(gfx, game, ui);
    return false;
  }
  if (!games_browser_commit_profile(
          p, game, ui, gfx, false, &candidate, false))
    return false; /* The Prepared intent and record remain available. */

  p->profile = candidate;
  p->game_cache[0] = new_cache;
  persistence_refresh_normal(p);
  save_policy_init(&p->save_policy);
  games_browser.modal_owned = false;
  game_library_scan_close(&games_browser.scanner);
  free(games_browser.rows);
  memset(&games_browser, 0, sizeof(games_browser));
  victory_animation_reset();
  restore_saved_session(p, game, ui, false);

  /* Profile activation already succeeded: a marker-cleanup error must
     never strand the live board or pretend the Replay was not saved. */
  intent.state = GAME_REPLAY_COMMITTED;
  if (game_library_replay_intent_write(p->profile_directory, &intent) !=
      STORE_OK) {
    (void)show_simple_message_box(gfx, game, ui, SDL_MESSAGEBOX_WARNING,
          tr(ui->language, T_STORAGE_ERROR),
          tr(ui->language, T_GAMES_REPLAY_FINALIZE_FAILED));
  }
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
    go_home_base(p,game,ui,false);
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

/* Record previews show only visible saved digits and notes, never the
   hidden solution. The entered digits use the established accent color. */
static void games_browser_draw_saved_board(Gfx *gfx, Theme theme,
                                            const Game *board, SDL_Rect area) {
  if (!gfx || !board || area.w < 72 || area.h < 72) return;
  int side = area.w < area.h ? area.w : area.h;
  int cell = side/9;
  if (cell < 8) return;
  side = cell*9;
  int x = area.x + (area.w-side)/2;
  int y = area.y + (area.h-side)/2;
  draw_round_rect(gfx->ren,(SDL_Rect){x-3,y-3,side+6,side+6},5,theme.btn);
  for (int index = 0; index < 81; ++index) {
    int col = index%9, row = index/9;
    SDL_Rect square = {x+col*cell+1,y+row*cell+1,cell-2,cell-2};
    if (board->puzzle[index]) {
      char number[2] = {(char)('0'+board->puzzle[index]),0};
      SDL_Color ink = board->fixed[index] ? theme.btnfg : theme.title;
      (void)draw_fitted_text(gfx,number,ink,square,
                             cell > 33 ? 24 : cell-5,10);
    } else if (board->notes[index] && cell >= 18) {
      for (int digit = 1; digit <= 9; ++digit) {
        /* Notes use bit positions 1..9, not 0..8. */
        if (!(board->notes[index] & (1u<<digit))) continue;
        int nx = square.x+(digit-1)%3*(cell/3);
        int ny = square.y+(digit-1)/3*(cell/3);
        if (cell >= 30) {
          char note[2] = {(char)('0'+digit),0};
          int width=0,height=0;
          SDL_Texture *texture = render_text(
              gfx,cached_font(gfx,12),note,theme.dim,&width,&height);
          int available=cell/3-2;
          if (!texture || width<=0 || height<=0 || available<=0) continue;
          /* draw_fitted_text reserves 8px horizontally and 4px vertically,
             which rejects most pencil glyphs in a 3x3 mini-cell. Scale
             the cached glyph directly, preserving its proportions. */
          if (width>available) {
            height=(height*available+width/2)/width;
            width=available;
          }
          if (height>available) {
            width=(width*available+height/2)/height;
            height=available;
          }
          if (width<1 || height<1) continue;
          SDL_Rect dst={nx+(cell/3-width)/2,ny+(cell/3-height)/2,
                        width,height};
          (void)SDL_RenderCopy(gfx->ren,texture,NULL,&dst);
        } else {
          draw_rect(gfx->ren,nx+cell/6,ny+cell/6,2,2,theme.dim);
        }
      }
    }
  }
  for (int index = 0; index <= 9; ++index) {
    SDL_Color border = index%3==0 ? theme.btnfg : theme.dim;
    int grid_x = x+index*cell, grid_y = y+index*cell;
    draw_line(gfx->ren,grid_x,y,grid_x,y+side,border);
    draw_line(gfx->ren,x,grid_y,x+side,grid_y,border);
    if (index%3==0 && index<9) {
      draw_line(gfx->ren,grid_x+1,y,grid_x+1,y+side,border);
      draw_line(gfx->ren,x,grid_y+1,x+side,grid_y+1,border);
    }
  }
}

static void games_browser_progress(const Game *board,int *filled,int *total) {
  *filled=0;
  *total=0;
  if (!board) return;
  for (int i=0;i<81;++i) {
    if (!board->fixed[i]) {
      ++*total;
      if (board->puzzle[i]) ++*filled;
    }
  }
}

static void games_browser_draw_button(Gfx *gfx, Theme theme, SDL_Rect rect,
                                      const char *text, bool focused,
                                      bool enabled, SDL_Color color,
                                      UiGlyph glyph, bool primary) {
  int mx=0,my=0;
  Uint32 mouse=SDL_GetMouseState(&mx,&my);
  bool hovered=enabled&&point_in(rect,mx,my);
  bool pressed=hovered&&(mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Color background=!enabled ? theme.palette_bg :
                       primary ? theme.title :
                       hovered ? theme.hover : theme.btn;
  SDL_Color foreground=!enabled ? theme.dim :
                       primary ? theme.bg : color;
  SDL_Rect face=polish_button_face(gfx,theme,rect,background,
                                    pressed,!enabled);
  ui_draw_icon_label(gfx,face,text,glyph,foreground,17);
  if (focused&&enabled)draw_focus_ring(gfx->ren,rect,theme.sel_outline);
}

static void games_browser_render(Gfx *gfx, const Game *game, UI *ui) {
  (void)game;
  Theme theme=ui->dark_theme?theme_dark():theme_light();
  GamesBrowserLayout lay=games_browser_layout(gfx);
  SDL_Rect heading=lay.title;
  ui_draw_glyph(gfx->ren,
                (SDL_Rect){heading.x+4,heading.y+8,24,24},
                UI_GLYPH_FOLDER,theme.title);
  heading.x+=31;
  heading.w-=31;
  (void)draw_fitted_text(gfx,tr(ui->language,T_GAMES_LIBRARY_TITLE),
                         theme.btnfg,heading,30,15);
  (void)polish_draw_wrapped_fit(gfx,tr(ui->language,T_GAMES_HELP),
                                theme.dim,lay.subtitle,15,10,true);

  if (games_browser.detail_open) {
    const GamesRow *row=&games_browser.rows[games_browser.selected];
    const ProfileResult *result=&row->entry.result;
    SDL_Rect surface={lay.title.x,lay.list.y,lay.title.w,
                       lay.back.y-lay.list.y-12};
    draw_round_rect(gfx->ren,surface,11,theme.palette_bg);
    /* Short landscape windows need a side-by-side finished preview:
       a tiny 9px Sudoku cell hides its number entirely. */
    bool compact_landscape=surface.w>=520 && surface.h<300;
    int free_height=surface.h-128;
    int side=surface.w-40;
    if (compact_landscape) {
      side=surface.h-34;
      if (side>210)side=210;
    } else if (side>free_height) side=free_height;
    int detail_max_side=gfx->width>=2200?560:
                         gfx->width>=1500?480:420;
    if (side>detail_max_side)side=detail_max_side;
    if (side<80)side=80;
    SDL_Rect board=compact_landscape
        ? (SDL_Rect){surface.x+18,
                     surface.y+(surface.h-side)/2,side,side}
        : (SDL_Rect){surface.x+(surface.w-side)/2,
                     surface.y+12,side,side};
    if(games_browser.preview_valid)
      games_browser_draw_saved_board(
          gfx,theme,&games_browser.preview.game,board);
    else
      (void)polish_draw_wrapped_fit(
          gfx,tr(ui->language,T_GAMES_SUMMARY_ONLY),theme.dim,board,17,10,true);
    char heading_info[250];
    unsigned long long seconds=(unsigned long long)(result->elapsed_ms/1000u);
    (void)snprintf(heading_info,sizeof(heading_info),
                   "%s · %s · %s · %02llu:%02llu",
                   tr(ui->language,result->status==SESSION_WON
                       ?T_YOU_WIN:T_YOU_LOSE),
                   tr(ui->language,mode_key(result->mode)),
                   difficulty_name(ui->language,result->difficulty),
                   seconds/60u,seconds%60u);
    SDL_Rect info=compact_landscape
        ? (SDL_Rect){board.x+side+18,surface.y+35,
                     surface.x+surface.w-board.x-side-34,26}
        : (SDL_Rect){surface.x+16,board.y+side+15,surface.w-32,26};
    (void)draw_fitted_text(gfx,heading_info,theme.btnfg,info,17,10);
    char detail[260];
    int revealed=0;
    if(games_browser.preview_valid)
      for(int k=0;k<81;k++)
        if(games_browser.preview.game.hinted[k])++revealed;
    (void)snprintf(detail,sizeof(detail),"%s: %u · %s: %u · %s: %d",
                   tr(ui->language,T_ERRORS),(unsigned)result->mistakes,
                   tr(ui->language,T_GAMES_STRIKES),(unsigned)result->strikes,
                   tr(ui->language,T_GAMES_REVEALS),revealed);
    info.y+=29;info.h=21;
    (void)draw_fitted_text(gfx,detail,theme.dim,info,14,10);
    const char *provenance=result->practice
        ?tr(ui->language,T_GAMES_PRACTICE)
        :tr(ui->language,T_GAMES_FINISHED);
    info.y+=22;
    (void)draw_fitted_text(gfx,provenance,theme.dim,info,13,10);
    if(games_browser.detail_record_valid &&
       games_browser.detail_record.original_daily) {
      const GameLibraryRecord *record=&games_browser.detail_record;
      char source[120];
      (void)snprintf(source,sizeof(source),"%s · %04u-%02u-%02u",
          tr(ui->language,T_GAMES_SOURCE_DAILY),
          (unsigned)record->original_daily_year,
          (unsigned)record->original_daily_month,
          (unsigned)record->original_daily_day);
      info.y+=21;
      (void)draw_fitted_text(gfx,source,theme.dim,info,13,10);
    }
    bool can_replay=!row->hot_only&&row->entry.kind==GAME_RECORD_EXACT&&
                    row->entry.status==STORE_OK;
    games_browser_draw_button(gfx,theme,lay.back,tr(ui->language,T_BACK),
        games_browser.focus==2,true,theme.btnfg,UI_GLYPH_BACK,false);
    games_browser_draw_button(gfx,theme,lay.replay,
        tr(ui->language,T_GAMES_REPLAY),games_browser.focus==3,can_replay,
        theme.btnfg,UI_GLYPH_REPLAY,false);
    games_browser_draw_button(gfx,theme,lay.remove,
        tr(ui->language,T_GAMES_DELETE),games_browser.focus==4,true,
        theme.btnfg,UI_GLYPH_TRASH,false);
    return;
  }

  draw_round_rect(gfx->ren,lay.list,11,theme.palette_bg);
  draw_round_rect(gfx->ren,lay.detail,11,theme.palette_bg);

  size_t last=games_browser.scroll+(size_t)lay.visible;
  if(last>games_browser.count)last=games_browser.count;
  for(size_t index=games_browser.scroll;index<last;++index) {
    const GamesRow *row=&games_browser.rows[index];
    const GameLibraryEntry *entry=&row->entry;
    SDL_Rect hit = games_browser_row_rect(lay, (int)(index-games_browser.scroll));
    SDL_Rect rect={hit.x+7,hit.y+5,hit.w-14,hit.h-7};
    bool selected=index==games_browser.selected;
    bool pressed=ui_pointer_pressed(hit);
    draw_round_rect(gfx->ren,rect,8,
                    pressed?theme.sel:selected?theme.hover:theme.palette_bg);
    if(selected)
      draw_round_rect(gfx->ren,(SDL_Rect){rect.x,rect.y,4,rect.h},
                      2,theme.title);
    SDL_Rect glyph={rect.x+9,rect.y+(rect.h-21)/2,21,21};
    ui_draw_glyph(gfx->ren,glyph,
        entry->result.status==SESSION_ACTIVE?UI_GLYPH_PLAY:UI_GLYPH_EYE,
        selected?theme.title:theme.dim);
    char caption[180];
    const char *status=game_library_entry_recoverable(entry)
        ?tr(ui->language,T_GAMES_RECOVER_LABEL)
        :entry->status!=STORE_OK?tr(ui->language,T_GAMES_UNAVAILABLE)
        :entry->result.practice?tr(ui->language,T_GAMES_PRACTICE)
        :entry->result.status==SESSION_ACTIVE
            ?(row->hot_only?tr(ui->language,T_GAMES_AUTOSAVE)
                            :tr(ui->language,T_GAMES_IN_PROGRESS))
            :tr(ui->language,entry->result.status==SESSION_WON
                                  ?T_YOU_WIN:T_YOU_LOSE);
    if(entry->status==STORE_OK)
      (void)snprintf(caption,sizeof(caption),"%s · %s",
           tr(ui->language,mode_key(entry->result.mode)),
           difficulty_name(ui->language,entry->result.difficulty));
    else
      (void)snprintf(caption,sizeof(caption),"%s",
                     tr(ui->language,T_GAMES_UNAVAILABLE));
    int label_x=rect.x+38;
    SDL_Rect main_label={label_x,rect.y+2,rect.w-48,26};
    SDL_Rect sub_label={label_x,rect.y+rect.h-25,rect.w-48,21};
    (void)draw_fitted_text(gfx,caption,theme.btnfg,main_label,17,10);
    (void)draw_fitted_text(gfx,status,theme.dim,sub_label,13,10);
    if(selected&&games_browser.focus==0)
      draw_focus_ring(gfx->ren,rect,theme.sel_outline);
  }
  if(!games_browser.count) {
    SDL_Rect empty={lay.list.x+18,lay.list.y+24,lay.list.w-36,
                    lay.list.h-48};
    (void)polish_draw_wrapped_fit(gfx,
        tr(ui->language,games_browser.scanning
            ?T_GAMES_LOADING:T_GAMES_EMPTY),theme.dim,empty,17,10,true);
  }

  SDL_Rect detail_header={lay.detail.x+12,lay.detail.y+8,
                          lay.detail.w-24,27};
  (void)draw_fitted_text(gfx,tr(ui->language,T_GAMES_PREVIEW),
                         theme.btnfg,detail_header,17,11);
  if(games_browser.selected<games_browser.count) {
    const GamesRow *row=&games_browser.rows[games_browser.selected];
    const ProfileResult *result=&row->entry.result;
    const Game *board=games_browser.preview_valid
        ?&games_browser.preview.game:NULL;
    bool spacious=lay.side_by_side;
    SDL_Rect preview;
    SDL_Rect summary;
    if(spacious) {
      int side=lay.detail.w-48;
      int height_limit=lay.detail.h-167;
      if(side>height_limit)side=height_limit;
      int preview_max_side=gfx->width>=2500?600:
                           gfx->width>=1500?520:380;
      if(side>preview_max_side)side=preview_max_side;
      if(side<81)side=81;
      preview=(SDL_Rect){lay.detail.x+(lay.detail.w-side)/2,
                         lay.detail.y+44,side,side};
      summary=(SDL_Rect){lay.detail.x+18,preview.y+side+16,
                          lay.detail.w-36,lay.detail.h-side-68};
    } else if(lay.detail.h>=118 && lay.detail.w>=300) {
      int side=lay.detail.h-53;
      if(side>118)side=118;
      preview=(SDL_Rect){lay.detail.x+12,lay.detail.y+38,side,side};
      summary=(SDL_Rect){lay.detail.x+side+22,lay.detail.y+37,
                          lay.detail.w-side-34,lay.detail.h-42};
    } else {
      preview=(SDL_Rect){0,0,0,0};
      summary=(SDL_Rect){lay.detail.x+15,lay.detail.y+37,
                          lay.detail.w-30,lay.detail.h-43};
    }
    if(board && preview.w>=72)
      games_browser_draw_saved_board(gfx,theme,board,preview);
    else if(!board && preview.w>=72)
      (void)polish_draw_wrapped_fit(
          gfx,tr(ui->language,T_GAMES_SUMMARY_ONLY),theme.dim,
          preview,13,10,true);

    char summary_line[220];
    unsigned long long sec=(unsigned long long)(result->elapsed_ms/1000u);
    (void)snprintf(summary_line,sizeof(summary_line),"%s · %s · %02llu:%02llu",
         tr(ui->language,mode_key(result->mode)),
         difficulty_name(ui->language,result->difficulty),
         sec/60u,sec%60u);
    SDL_Rect text_rect={summary.x,summary.y,summary.w,26};
    (void)draw_fitted_text(gfx,summary_line,theme.btnfg,
                           text_rect,16,10);
    if(board) {
      int filled=0,total=0;
      games_browser_progress(board,&filled,&total);
      char progress[160];
      (void)snprintf(progress,sizeof(progress),
                     tr(ui->language,T_GAMES_FILLED_FMT),filled,total);
      text_rect.y+=28;text_rect.h=25;
      (void)draw_fitted_text(gfx,progress,theme.dim,text_rect,14,10);
      int bar_y=text_rect.y+text_rect.h+5;
      if(bar_y+8<summary.y+summary.h && summary.w>36) {
        SDL_Rect track={summary.x+7,bar_y,summary.w-14,6};
        draw_round_rect(gfx->ren,track,3,theme.btn);
        if(total>0&&filled>0) {
          SDL_Rect bar=track;
          bar.w=track.w*filled/total;
          if(bar.w<3)bar.w=3;
          draw_round_rect(gfx->ren,bar,3,theme.title);
        }
      }
    } else {
      text_rect.y+=28;
      text_rect.h=summary.h>39?24:summary.h-27;
      if(text_rect.h>10)
        (void)polish_draw_wrapped_fit(gfx,
            tr(ui->language,game_library_entry_recoverable(&row->entry)
                ?T_GAMES_RECOVER_HINT:T_GAMES_SUMMARY_ONLY),
            theme.dim,text_rect,13,10,true);
    }
    if(games_browser.incomplete) {
      SDL_Rect warning={lay.detail.x+12,lay.detail.y+lay.detail.h-25,
                        lay.detail.w-24,19};
      (void)draw_fitted_text(gfx,
          tr(ui->language,T_GAMES_SCAN_INCOMPLETE),
          theme.text_wrong,warning,12,10);
    }
  } else if(!games_browser.scanning) {
    SDL_Rect message={lay.detail.x+12,lay.detail.y+37,
                      lay.detail.w-24,lay.detail.h-51};
    (void)polish_draw_wrapped_fit(gfx,
        tr(ui->language,T_GAMES_EMPTY),theme.dim,message,16,10,true);
  }

  bool selected=games_browser.selected<games_browser.count;
  bool active=!games_browser.scanning && selected &&
      games_browser.rows[games_browser.selected].entry.status==STORE_OK &&
      games_browser.rows[games_browser.selected].entry.result.status==
          SESSION_ACTIVE;
  bool can_open=!games_browser.scanning && selected &&
      games_browser.rows[games_browser.selected].entry.status==STORE_OK;
  bool can_recover=!games_browser.scanning && selected &&
      !games_browser.rows[games_browser.selected].hot_only &&
      game_library_entry_recoverable(
          &games_browser.rows[games_browser.selected].entry);
  bool can_replay=can_open &&
      !games_browser.rows[games_browser.selected].hot_only &&
      games_browser.rows[games_browser.selected].entry.kind==GAME_RECORD_EXACT &&
      games_browser.rows[games_browser.selected].entry.result.status!=
          SESSION_ACTIVE;
  games_browser_draw_button(gfx,theme,lay.back,tr(ui->language,T_BACK),
      games_browser.focus==2,true,theme.btnfg,UI_GLYPH_BACK,false);
  games_browser_draw_button(gfx,theme,lay.primary,
      tr(ui->language,can_recover?T_RECOVER:
         active?T_GAMES_LOAD:T_GAMES_VIEW),
      games_browser.focus==1,can_open||can_recover,theme.btnfg,
      active?UI_GLYPH_PLAY:UI_GLYPH_EYE,active);
  games_browser_draw_button(gfx,theme,lay.replay,
      tr(ui->language,T_GAMES_REPLAY),games_browser.focus==3,can_replay,
      theme.btnfg,UI_GLYPH_REPLAY,false);
  games_browser_draw_button(gfx,theme,lay.remove,
      tr(ui->language,T_GAMES_DELETE),games_browser.focus==4,
      !games_browser.scanning&&games_browser.count>0,
      theme.btnfg,UI_GLYPH_TRASH,false);
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
  return focus==1 && (row->entry.status==STORE_OK ||
                       (!row->hot_only &&
                        game_library_entry_recoverable(&row->entry)));
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
    (void)persistence_retry_current(p,game,ui);
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
