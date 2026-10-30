#include "audio.h"

static Result audio_previous_result = RES_NONE;
static bool audio_previous_result_context = false;
static bool audio_ui_seen = false;

static void audio_apply_preferences(Persistence *persistence) {
  if (!persistence) return;
  /* Apply the complete persisted mix before any screen transition can start
     playback. The first app tick selects the context and may then play. */
  audio_set_music_volume(persistence->preferences.music_volume);
  audio_set_fx_volume(persistence->preferences.fx_volume);
  audio_set_music_muted(persistence->preferences.music_muted);
  audio_set_fx_muted(persistence->preferences.fx_muted);
  audio_set_enabled(persistence->preferences.audio_enabled);
}

static void audio_store_preferences(Persistence *persistence) {
  if (!persistence) return;
  persistence->preferences.audio_enabled = audio_is_enabled();
  persistence->preferences.music_muted = audio_music_muted();
  persistence->preferences.fx_muted = audio_fx_muted();
  if (!persistence->enabled) return;
  if (audio_ui)
    (void)save_preferences(persistence, audio_ui);
  else
    persistence_stage_preferences(persistence);
}

static void audio_toggle_music_mute(Persistence *persistence) {
  bool muted = !audio_music_muted();
  audio_set_music_muted(muted);
  if (!muted) audio_play_effect(AUDIO_EFFECT_CLICK);
  audio_store_preferences(persistence);
}

static void audio_toggle_fx_mute(Persistence *persistence) {
  bool muted = !audio_fx_muted();
  audio_set_fx_muted(muted);
  if (!muted) audio_play_effect(AUDIO_EFFECT_CLICK);
  audio_store_preferences(persistence);
}

static void audio_toggle(Persistence *persistence) {
  bool enabled = !audio_is_enabled();
  if (enabled) audio_set_enabled(true);
  audio_play_effect(AUDIO_EFFECT_CLICK);
  if (!enabled) audio_set_enabled(false);
  audio_store_preferences(persistence);
}

static AudioContext audio_context_for_game(const Game *game, const UI *ui) {
  if (!ui) return AUDIO_CONTEXT_MAIN;
  if (app_result_context_active(ui) && ui->result == RES_LOSE)
    return AUDIO_CONTEXT_FAIL;

  bool gameplay_context =
      ui->screen == SCR_PLAY ||
      ((ui->screen == SCR_HELP || ui->screen == SCR_ABOUT ||
        ui->screen == APP_SCREEN_SETTINGS) &&
       ui->prev_screen == SCR_PLAY);
  if (!gameplay_context || !game || ui->result != RES_NONE)
    return AUDIO_CONTEXT_MAIN;
  if (game->difficulty == DIFFICULTY_HARD)
    return AUDIO_CONTEXT_FAIL;
  if (ui->mode == MODE_STRIKES && ui->strikes_max > 0 &&
      ui->strikes >= ui->strikes_max - 1)
    return AUDIO_CONTEXT_FAIL;
  if (ui->mode == MODE_TIME && ui->time_limit_s > 0 &&
      elapsed_time(ui) >= ui->time_limit_s * 0.75)
    return AUDIO_CONTEXT_FAIL;
  return AUDIO_CONTEXT_MAIN;
}

static void audio_sync_ui(const Game *game, const UI *ui) {
  if (!ui) return;
  if (app_suspended) {
    audio_set_focus_paused(true); /* Background must also silence Home. */
    return;
  }

  bool result_context = app_result_context_active(ui);
  bool entered_result =
      audio_ui_seen && result_context &&
      (!audio_previous_result_context || audio_previous_result != ui->result);
  bool left_result =
      audio_ui_seen && audio_previous_result_context && !result_context;
  bool opening_home = !audio_ui_seen && ui->screen == SCR_TITLE;
  bool requested_home_cue = ui_home_cue_requested && ui->screen == SCR_TITLE;

  if (left_result) audio_cancel_result();

  audio_set_context(audio_context_for_game(game, ui));
  audio_set_focus_paused((ui->pause_reasons & (PAUSE_FOCUS | PAUSE_MANUAL)) != 0);

  if (entered_result) {
    audio_play_result(ui->result == RES_LOSE ? AUDIO_RESULT_FAIL
                                             : AUDIO_RESULT_WIN);
  } else if (opening_home || requested_home_cue) {
    audio_play_result(AUDIO_RESULT_WIN);
  }
  if (requested_home_cue) ui_home_cue_requested = false;

  audio_update();

  audio_previous_result = ui->result;
  audio_previous_result_context = result_context;
  audio_ui_seen = true;
}

static void persistence_init(Persistence *persistence) {
  persistence_init_base(persistence);
  audio_persistence = persistence;
  audio_apply_preferences(persistence);
}

static bool app_rebuild_renderer_resources(Gfx *gfx) {
  if (!gfx) return false;
  gfx->renderer_ready = false;
  gfx->dirty = false;
  if (!gfx->ren) return false;
  text_cache_clear();
  if (!app_rebuild_base_textures(gfx)) return false;
  if (app_icon_texture) SDL_DestroyTexture(app_icon_texture);
  app_icon_texture = texture_from_rgba(gfx->ren, sudokura_icon_rgba,
                                       sudokura_icon_width,
                                       sudokura_icon_height);
  if (!app_icon_texture) {
    fprintf(stderr, "compact icon texture: %s\n", SDL_GetError());
    return false;
  }
  gfx->renderer_ready = true;
  gfx->dirty = true;
  return true;
}

/* A failed metrics refresh is as unsafe as a failed texture rebuild:
   invalidate any deferred action and keep the session concealed until retry. */
static void app_mark_renderer_unavailable(Gfx *gfx, UI *ui) {
  if (!gfx) return;
  ui_modal_invalidate();
  ui_pointer_cancel();
  ui_touch_cancel();
  gfx->renderer_ready = false;
  gfx->dirty = false;
  if (ui && ui->session_open && ui->result == RES_NONE)
    (void)clock_pause(ui, PAUSE_FOCUS);
  audio_set_focus_paused(true);
  if (gfx->win)
    SDL_SetWindowTitle(gfx->win,
        tr(ui ? ui->language : LANG_EN, T_RENDERER_UNAVAILABLE));
}

/* Retry on explicit R or a legitimate surface/reset event, never a frame loop. */
static bool app_restore_renderer_resources(Gfx *gfx, UI *ui) {
  if (!gfx) return false;
  gfx->renderer_ready = false;
  bool restored = gfx_refresh_metrics(gfx) &&
                  app_rebuild_renderer_resources(gfx);
  if (!restored) {
    app_mark_renderer_unavailable(gfx, ui);
  } else if (gfx->win) {
    SDL_SetWindowTitle(gfx->win, SUDOKURA_NAME_VERSION);
  }
  return restored;
}

/* Ordinary size/move changes retain valid textures, but metrics failures
   must fail closed. Only an already-unavailable renderer is fully rebuilt. */
static bool app_refresh_renderer_metrics(Gfx *gfx, UI *ui) {
  if (!gfx) return false;
  if (!gfx->renderer_ready)
    return app_restore_renderer_resources(gfx, ui);
  if (gfx_refresh_metrics(gfx)) return true;
  app_mark_renderer_unavailable(gfx, ui);
  return false;
}

static bool app_resize_renderer_metrics(Gfx *gfx, UI *ui,
                                        int width, int height) {
  if (!gfx) return false;
  if (!apply_window_resize(gfx, width, height)) {
    app_mark_renderer_unavailable(gfx, ui);
    return false;
  }
  return gfx->renderer_ready || app_restore_renderer_resources(gfx, ui);
}

static bool app_init(Gfx *gfx, const char *font_cli) {
  if (!app_init_base(gfx, font_cli)) return false;
  gfx->renderer_ready = true;
  audio_gfx = gfx;
  app_icon_texture = texture_from_rgba(gfx->ren, sudokura_icon_rgba,
                                       sudokura_icon_width,
                                       sudokura_icon_height);
  if (!app_icon_texture)
    fprintf(stderr, "compact icon texture: %s\n", SDL_GetError());
  (void)audio_init();
  return true;
}

static void app_shutdown(Gfx *gfx) {
  audio_shutdown();
  if (audio_persistence)
    store_writer_lock_release(&audio_persistence->writer_lock);
  if (app_icon_texture) SDL_DestroyTexture(app_icon_texture);
  app_icon_texture = NULL;
  audio_persistence = NULL;
  audio_gfx = NULL;
  audio_game = NULL;
  audio_ui = NULL;
  audio_ui_seen = false;
  audio_previous_result_context = false;
  settings_open = false;
  ui_home_cue_requested = false;
  app_shutdown_base(gfx);
}

static void app_tick(Gfx *gfx, Game *game, UI *ui) {
  if (app_suspended || !gfx || !gfx->renderer_ready) {
    audio_set_focus_paused(true);
    return;
  }
  audio_gfx = gfx;
  audio_game = game;
  audio_ui = ui;
  audio_sync_ui(game, ui);
}

static void app_frame(Gfx *gfx, Game *game, UI *ui) {
  if (!gfx || !gfx->renderer_ready) {
    if (gfx) gfx->dirty = false;
    return; /* Never render with missing or invalid resources. */
  }
  if (app_suspended) return; /* No rendering on a suspended surface. */
#if defined(__ANDROID__)
  if (gfx->width < 360 || gfx->height < 600) {
    /* A short rotated surface cannot contain a 270dp board and 48dp
       controls. Retain the game and use the existing safe Android Back. */
    if (ui && ui->session_open) (void)clock_pause(ui, PAUSE_FOCUS);
    audio_set_focus_paused(true);
    Theme theme = ui && ui->dark_theme ? theme_dark() : theme_light();
    SDL_SetRenderDrawColor(gfx->ren, theme.bg.r, theme.bg.g, theme.bg.b, 255);
    SDL_RenderClear(gfx->ren);
    if (ui && gfx->width > 48 && gfx->height > 48) {
      SDL_Rect hint = {12, gfx->height / 2 - 30,
                       gfx->width - 24, 60};
      (void)draw_fitted_text(gfx, tr(ui->language, T_ROTATE_PORTRAIT),
                             theme.btnfg, hint, 22, 12);
    }
    SDL_RenderPresent(gfx->ren);
    gfx->dirty = false;
    return;
  }
#endif
  app_tick(gfx,game,ui);
  app_render_ui(gfx, game, ui);
  SDL_RenderPresent(gfx->ren);
  gfx->dirty=false;
}
