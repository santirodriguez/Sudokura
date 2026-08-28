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

static void audio_sync_ui(const UI *ui) {
  if (!ui) return;

  bool entered_result =
      audio_ui_seen && ui->screen == SCR_END &&
      (audio_previous_screen != SCR_END || audio_previous_result != ui->result);
  bool left_result =
      audio_ui_seen && audio_previous_screen == SCR_END && ui->screen != SCR_END;

  if (left_result) audio_cancel_result();
  audio_set_context(ui->screen == SCR_END && ui->result == RES_LOSE
                        ? AUDIO_CONTEXT_FAIL
                        : AUDIO_CONTEXT_MAIN);
  audio_set_focus_paused((ui->pause_reasons & (PAUSE_FOCUS | PAUSE_MANUAL)) != 0);

  if (entered_result) {
    audio_play_result(ui->result == RES_LOSE ? AUDIO_RESULT_FAIL
                                             : AUDIO_RESULT_WIN);
  }
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
  (void)audio_init();
  return true;
}

static void app_shutdown(Gfx *gfx) {
  audio_shutdown();
  audio_persistence = NULL;
  audio_gfx = NULL;
  audio_ui = NULL;
  audio_ui_seen = false;
  app_shutdown_base(gfx);
}

static void app_frame(Gfx *gfx, const Game *game, UI *ui) {
  audio_gfx = gfx;
  audio_ui = ui;
  audio_sync_ui(ui);
  app_frame_base(gfx, game, ui);
}

static int audio_poll_event(SDL_Event *event) {
  while (SDL_PollEvent(event)) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_v) {
      audio_toggle(audio_persistence);
      continue;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT && audio_gfx && audio_ui &&
        audio_ui->screen == SCR_PLAY && audio_ui->pause_reasons == 0) {
      Layout layout = compute_layout(audio_gfx->width, audio_gfx->height,
                                     audio_ui->mode);
      SidebarRects controls;
      compute_sidebar_rects(&layout, &controls);
      if (point_in(controls.btn[5], event->button.x, event->button.y)) {
        audio_toggle(audio_persistence);
        continue;
      }
    }

    return 1;
  }
  return 0;
}
