#include "audio.h"

static Persistence *audio_persistence = NULL;
static Gfx *audio_gfx = NULL;
static UI *audio_ui = NULL;
static Screen audio_previous_screen = SCR_TITLE;
static Result audio_previous_result = RES_NONE;
static bool audio_ui_seen = false;

static void audio_apply_preferences(Persistence *persistence) {
  if (!persistence) return;
  audio_set_enabled(persistence->preferences.audio_enabled);
  audio_set_context(AUDIO_CONTEXT_MAIN);
}

static void audio_toggle(Persistence *persistence) {
  bool enabled = !audio_is_enabled();
  if (enabled) audio_set_enabled(true);
  audio_play_effect(AUDIO_EFFECT_CLICK);
  if (!enabled) audio_set_enabled(false);
  if (!persistence) return;
  persistence->preferences.audio_enabled = enabled;
  if (persistence->enabled)
    (void)preferences_save_file(persistence->preferences_path,
                                &persistence->preferences);
}

static AudioContext audio_context_for_game(const Game *game, const UI *ui) {
  if (!ui) return AUDIO_CONTEXT_MAIN;
  if (ui->screen == SCR_END && ui->result == RES_LOSE)
    return AUDIO_CONTEXT_FAIL;

  bool gameplay_context =
      ui->screen == SCR_PLAY ||
      ((ui->screen == SCR_HELP || ui->screen == SCR_ABOUT) &&
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

  bool entered_result =
      audio_ui_seen && ui->screen == SCR_END &&
      (audio_previous_screen != SCR_END || audio_previous_result != ui->result);
  bool left_result =
      audio_ui_seen && audio_previous_screen == SCR_END && ui->screen != SCR_END;
  bool opening_home = !audio_ui_seen && ui->screen == SCR_TITLE;
  bool returned_home_from_result =
      audio_ui_seen && ui->screen == SCR_TITLE && audio_previous_screen == SCR_END;

  if (left_result) audio_cancel_result();

  /* Screen/game state owns the background context. In particular, returning
     to Home from an unfinished game always restores the Clear loop without
     inserting a second jingle. */
  audio_set_context(audio_context_for_game(game, ui));
  audio_set_focus_paused((ui->pause_reasons & (PAUSE_FOCUS | PAUSE_MANUAL)) != 0);

  if (entered_result) {
    audio_play_result(ui->result == RES_LOSE ? AUDIO_RESULT_FAIL
                                             : AUDIO_RESULT_WIN);
  } else if (opening_home || returned_home_from_result) {
    /* Preserve the established startup/result-to-Home Clear jingle. */
    audio_play_result(AUDIO_RESULT_WIN);
  }

  /* Guarantees that an enabled/available context cannot remain accidentally
     silent after a transition once no result jingle is pending. */
  audio_update();

  audio_previous_screen = ui->screen;
  audio_previous_result = ui->result;
  audio_ui_seen = true;
}

static void persistence_init(Persistence *persistence) {
  persistence_init_base(persistence);
  audio_persistence = persistence;
  audio_apply_preferences(persistence);
}

static bool app_init(Gfx *gfx, const char *font_cli) {
  if (!app_init_base(gfx, font_cli)) return false;
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
  if (app_icon_texture) SDL_DestroyTexture(app_icon_texture);
  app_icon_texture = NULL;
  audio_persistence = NULL;
  audio_gfx = NULL;
  audio_ui = NULL;
  audio_ui_seen = false;
  app_shutdown_base(gfx);
}

static void app_frame(Gfx *gfx, const Game *game, UI *ui) {
  audio_gfx = gfx;
  audio_ui = ui;
  audio_sync_ui(game, ui);
  (void)app_frame_base;
  app_render(gfx, game, ui);
  SDL_RenderPresent(gfx->ren);
}
