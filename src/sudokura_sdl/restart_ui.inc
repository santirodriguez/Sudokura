/* Native-owner Restart continuation. No nested event pump or new persistence. */
static bool restart_discard_allowed(const Persistence *p, const UI *ui) {
  if (!p || !p->enabled || !ui || !ui->session_open ||
      ui->result != RES_NONE || p->replay_activation_unresolved)
    return false;
  const ProfileSlot *slot = ui->is_daily ? &p->profile.daily : &p->profile.normal;
  bool unretained = slot->present && !slot->retained &&
      slot->value.session.status == SESSION_ACTIVE &&
      game_attempt_id_valid(p->active_attempt_id) &&
      game_attempt_id_equal(slot->attempt_id, p->active_attempt_id);
  return save_policy_replacement_discard_allowed(
      &p->save_policy, ui->is_daily, unretained,
      p->capture_failed, p->keep_pending_mask);
}

static bool restart_open_save(Persistence *p, UI *ui, Gfx *gfx) {
  bool discard = restart_discard_allowed(p, ui);
  AppModalRequest request = {
    .kind = APP_MODAL_RECOVERY, .action = APP_MODAL_RESTART_SAVE,
    .binding = ui_modal_current_binding(p, ui),
    .title = T_UNSAVED_TITLE, .body = T_UNSAVED_REPLACE,
    .buttons = {T_BACK, T_RETRY_SAVE, T_DISCARD},
    .button_count = discard ? 3u : 2u,
    .destructive_buttons = discard ? 4u : 0u
  };
  return ui_modal_open_request(gfx, ui, &request);
}

static bool restart_open_confirm(Persistence *p, UI *ui, Gfx *gfx,
                                 bool discard) {
  AppModalRequest request = {
    .kind = APP_MODAL_CONFIRM,
    .action = discard ? APP_MODAL_RESTART_DISCARD : APP_MODAL_RESTART,
    .binding = ui_modal_current_binding(p, ui),
    .title = T_RESTART, .body = T_RESTART_PROMPT,
    .buttons = {T_CANCEL, T_RESTART},
    .button_count = 2u, .destructive_buttons = 2u
  };
  return ui_modal_open_request(gfx, ui, &request);
}

static void restart_commit(Persistence *p, Game *game, UI *ui) {
  victory_animation_reset();
  restart_current_session(p, game, ui);
}

static void restart_after_gate(Persistence *p, Game *game, UI *ui,
                               Gfx *gfx, bool discard) {
  if (!p || !game || !ui || !gfx || !ui->session_open) return;
  if (p->replay_activation_unresolved ||
      (discard ? !restart_discard_allowed(p, ui) :
       (p->capture_failed || save_policy_dirty(&p->save_policy)))) {
    (void)restart_open_save(p, ui, gfx);
    return;
  }
  if (!game_has_meaningful_progress(game, ui->mistakes, ui->strikes)) {
    restart_commit(p, game, ui);
    return;
  }
  (void)restart_open_confirm(p, ui, gfx, discard);
}

static void restart_requested(Persistence *p, Game *game, UI *ui, Gfx *gfx) {
  if (!p || !p->enabled || !game || !ui || !gfx || !ui->session_open ||
      (ui->screen != SCR_PLAY && ui->screen != SCR_END))
    return;
  if (p->replay_activation_unresolved) {
    (void)ui_modal_message(gfx, ui, T_STORAGE_ERROR, T_GAMES_ACTION_FAILED);
    return;
  }
  bool saved = !p->capture_failed && !save_policy_dirty(&p->save_policy);
  if (!saved) saved = persistence_retry_current(p, game, ui);
  if (!save_policy_transition_ready(&p->save_policy, saved) ||
      p->capture_failed)
    (void)restart_open_save(p, ui, gfx);
  else
    restart_after_gate(p, game, ui, gfx, false);
}

/* Chosen actions arrive only after lifecycle/window events have been drained. */
static void restart_dispatch(Persistence *p, Game *game, UI *ui, Gfx *gfx,
                              const AppModalResult *result) {
  if (!p || !game || !ui || !gfx || !result ||
      result->resolution != APP_MODAL_CHOSEN ||
      !app_modal_binding_equal(result->binding,
          ui_modal_dispatch_binding(p, ui, result->action)))
    return;
  if (result->action == APP_MODAL_RESTART_SAVE) {
    if (result->button == 1) {
      bool saved = persistence_retry_current(p, game, ui);
      if (save_policy_transition_ready(&p->save_policy, saved) &&
          !p->capture_failed)
        restart_after_gate(p, game, ui, gfx, false);
      else
        (void)restart_open_save(p, ui, gfx);
    } else if (result->button == 2) {
      if (restart_discard_allowed(p, ui))
        restart_after_gate(p, game, ui, gfx, true);
      else
        (void)restart_open_save(p, ui, gfx);
    }
  } else if ((result->action == APP_MODAL_RESTART ||
              result->action == APP_MODAL_RESTART_DISCARD) &&
             result->button == 1) {
    bool discard = result->action == APP_MODAL_RESTART_DISCARD;
    if (!p->replay_activation_unresolved &&
        (discard ? restart_discard_allowed(p, ui) :
         (!p->capture_failed && !save_policy_dirty(&p->save_policy))))
      restart_commit(p, game, ui);
    else
      (void)restart_open_save(p, ui, gfx);
  }
}
