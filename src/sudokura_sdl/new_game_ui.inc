/* Event-driven New Game replacement on the existing SDL owner. */
static bool start_new_game_with_ui(Persistence *, Game *, UI *, Gfx *);

static bool new_game_ready(const Persistence *p) {
  return p && p->enabled && !p->capture_failed &&
         !p->keep_pending_mask && !persistence_activation_blocked(p) &&
         !save_policy_dirty(&p->save_policy);
}

/* Only a single, unretained, active normal slot can be abandoned after
   an unsuccessful checkpoint; other dirty domains must be preserved. */
static bool new_game_discard_allowed(const Persistence *p) {
  if (!p || !p->enabled || persistence_activation_blocked(p)) return false;
  const ProfileSlot *slot = &p->profile.normal;
  bool eligible = slot->present && !slot->retained &&
      slot->value.session.status == SESSION_ACTIVE &&
      game_attempt_id_valid(slot->attempt_id);
  return save_policy_replacement_discard_allowed(
      &p->save_policy, false, eligible,
      p->capture_failed, p->keep_pending_mask);
}

static bool new_game_open_save(Persistence *p, UI *ui, Gfx *gfx) {
  if (!p || !ui || !gfx) return false;
  bool can_discard = new_game_discard_allowed(p);
  AppModalRequest request = {
      .kind = APP_MODAL_RECOVERY, .action = APP_MODAL_NEW_SAVE,
      .binding = ui_modal_new_game_binding(p, ui),
      .title = T_UNSAVED_TITLE, .body = T_UNSAVED_REPLACE,
      .buttons = {T_BACK, T_RETRY_SAVE, T_DISCARD},
      .button_count = can_discard ? 3u : 2u,
      .destructive_buttons = can_discard ? 4u : 0u};
  return ui_modal_open_request(gfx, ui, &request);
}

static bool new_game_open_keep(Persistence *p, UI *ui, Gfx *gfx) {
  if (!p || !ui || !gfx) return false;
  AppModalRequest request = {
      .kind = APP_MODAL_CONFIRM, .action = APP_MODAL_NEW_KEEP,
      .binding = ui_modal_new_game_binding(p, ui),
      .title = T_GAMES_KEEP_TITLE, .body = T_GAMES_NEW_PROMPT,
      .buttons = {T_CANCEL, T_GAMES_SAVE_CONTINUE, T_DISCARD},
      .button_count = 3u, .destructive_buttons = 4u};
  return ui_modal_open_request(gfx, ui, &request);
}

static bool new_game_open_confirm(Persistence *p, UI *ui, Gfx *gfx) {
  if (!p || !ui || !gfx) return false;
  AppModalRequest request = {
      .kind = APP_MODAL_CONFIRM, .action = APP_MODAL_NEW_GAME,
      .binding = ui_modal_new_game_binding(p, ui),
      .title = T_NEW_GAME, .body = T_NEW_PROMPT,
      .buttons = {T_CANCEL, T_NEW}, .button_count = 2u,
      .destructive_buttons = 2u};
  return ui_modal_open_request(gfx, ui, &request);
}

static bool new_game_open_keep_retry(Persistence *p, UI *ui, Gfx *gfx) {
  if (!p || !ui || !gfx) return false;
  AppModalRequest request = {
      .kind = APP_MODAL_RECOVERY, .action = APP_MODAL_NEW_KEEP_RETRY,
      .binding = ui_modal_new_game_binding(p, ui),
      .title = T_STORAGE_ERROR, .body = T_GAMES_RETRY_SAVE_PROMPT,
      .buttons = {T_BACK, T_RETRY_SAVE}, .button_count = 2u};
  return ui_modal_open_request(gfx, ui, &request);
}

static bool new_game_open_generation_retry(Persistence *p, UI *ui, Gfx *gfx) {
  if (!p || !ui || !gfx) return false;
  AppModalRequest request = {
      .kind = APP_MODAL_CONFIRM, .action = APP_MODAL_NEW_GENERATE_RETRY,
      .binding = ui_modal_new_game_binding(p, ui),
      .title = T_GENERATION_FAILED, .body = T_GENERATION_FAILED_BODY,
      .buttons = {T_CANCEL, T_RETRY}, .button_count = 2u};
  return ui_modal_open_request(gfx, ui, &request);
}

static bool new_game_launch(Persistence *p, Game *game, UI *ui, Gfx *gfx,
                            bool discard_failed_save) {
  if (!p || !game || !ui || !gfx) return false;
  if (discard_failed_save ? !new_game_discard_allowed(p) :
      !new_game_ready(p)) {
    (void)new_game_open_save(p, ui, gfx);
    return false;
  }
  victory_animation_reset();
  return start_new_game_with_ui(p, game, ui, gfx);
}

static void new_game_after_saved(Persistence *p, Game *game, UI *ui,
                                 Gfx *gfx) {
  if (!p || !game || !ui || !gfx) return;
  if (!new_game_ready(p)) {
    (void)new_game_open_save(p, ui, gfx);
    return;
  }
  const ProfileSlot *slot = &p->profile.normal;
  const SessionState *saved = profile_slot_session(slot);
  if (!saved || !session_has_meaningful_progress_runtime(saved)) {
    (void)new_game_launch(p, game, ui, gfx, false);
  } else if (slot->retained) {
    (void)new_game_open_confirm(p, ui, gfx);
  } else {
    (void)new_game_open_keep(p, ui, gfx);
  }
}

static void new_game_requested(Persistence *p, Game *game, UI *ui, Gfx *gfx) {
  if (!p || !p->enabled || !game || !ui || !gfx ||
      (ui->screen != SCR_TITLE && ui->screen != SCR_END))
    return;
  if (persistence_activation_blocked(p)) {
    (void)ui_modal_message(gfx, ui, T_STORAGE_ERROR, T_GAMES_ACTION_FAILED);
    return;
  }
  bool saved = new_game_ready(p);
  if (!saved) saved = persistence_retry_current(p, game, ui);
  if (!save_policy_transition_ready(&p->save_policy, saved) ||
      p->capture_failed || p->keep_pending_mask)
    (void)new_game_open_save(p, ui, gfx);
  else
    new_game_after_saved(p, game, ui, gfx);
}

static void new_game_dispatch(Persistence *p, Game *game, UI *ui, Gfx *gfx,
                              const AppModalResult *result) {
  if (!p || !game || !ui || !gfx || !result ||
      result->resolution != APP_MODAL_CHOSEN ||
      !app_modal_binding_equal(result->binding,
          ui_modal_new_game_binding(p, ui)))
    return;
  if (result->action == APP_MODAL_NEW_SAVE) {
    if (result->button == 1u) {
      bool saved = persistence_retry_current(p, game, ui);
      if (save_policy_transition_ready(&p->save_policy, saved) &&
          !p->capture_failed && !p->keep_pending_mask)
        new_game_after_saved(p, game, ui, gfx);
      else
        (void)new_game_open_save(p, ui, gfx);
    } else if (result->button == 2u) {
      (void)new_game_launch(p, game, ui, gfx, true);
    }
  } else if (result->action == APP_MODAL_NEW_GAME &&
             result->button == 1u) {
    if (p->profile.normal.retained && new_game_ready(p))
      (void)new_game_launch(p, game, ui, gfx, false);
    else
      new_game_after_saved(p, game, ui, gfx);
  } else if (result->action == APP_MODAL_NEW_KEEP) {
    if (result->button == 1u) {
      bool retained = games_browser_save_hot(p, false);
      if (retained && new_game_ready(p))
        (void)new_game_launch(p, game, ui, gfx, false);
      else
        (void)new_game_open_keep_retry(p, ui, gfx);
    } else if (result->button == 2u) {
      if (new_game_ready(p) && p->profile.normal.present &&
          !p->profile.normal.retained)
        (void)new_game_launch(p, game, ui, gfx, false);
      else
        new_game_after_saved(p, game, ui, gfx);
    }
  } else if (result->action == APP_MODAL_NEW_KEEP_RETRY &&
             result->button == 1u) {
    bool saved = persistence_retry_current(p, game, ui);
    if (saved && new_game_ready(p) && p->profile.normal.retained)
      (void)new_game_launch(p, game, ui, gfx, false);
    else
      (void)new_game_open_keep_retry(p, ui, gfx);
  } else if (result->action == APP_MODAL_NEW_GENERATE_RETRY &&
             result->button == 1u) {
    /* A verified, unchanged Save can retry generation immediately. A
       previously discarded failed Save must pass the gate again. */
    if (new_game_ready(p))
      (void)new_game_launch(p, game, ui, gfx, false);
    else
      new_game_requested(p, game, ui, gfx);
  }
}
