#if !defined(_WIN32) && !defined(__APPLE__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

/* Sudokura SDL application composition. Core modules remain normal C units;
   all screens share one presentation vocabulary and one input/event owner. */
#include "session.h"
#include "seed.h"
#include "audio.h"
#include "input.h"
#include "app_modal.h"
#include "touch_input.h"
#include "progress.h"

static void ui_audio_control_cancel_interaction(void);

/* Retain the board's semantic palette while adapting screen surface tokens. */
#define theme_dark theme_base_dark
#define theme_light theme_base_light
#include "sudokura_sdl/01_runtime.inc"
#undef theme_dark
#undef theme_light
#include "sudokura_sdl/ui_shared_style.inc"
#include "sudokura_sdl/02_font_discovery.inc"
/* The board's cell/grid painter stays unchanged. Later screen primitives use
   the shared smooth surfaces; gameplay adds only an outer frame and controls. */
#define draw_round_rect draw_base_round_rect
#define draw_focus_ring draw_base_focus_ring
#include "sudokura_sdl/03_board_render.inc"
#undef draw_round_rect
#undef draw_focus_ring
#include "sudokura_sdl/04_screens.inc"

/* Preserve state/layout helpers and a fallback for unknown future screens.
   These aliases are local to each include, not runtime routing state. */
static void app_render_existing_ui(Gfx *gfx, const Game *game, UI *ui);
#define app_render_ui app_render_existing_ui
#define ui_draw_glyph ui_draw_base_glyph
#define ui_draw_icon_label ui_draw_base_icon_label
#include "sudokura_sdl/ui_present.inc"
#undef app_render_ui
#undef ui_draw_glyph
#undef ui_draw_icon_label
#define app_render_ui app_render_home_about
#include "sudokura_sdl/ui_design.inc"
#undef app_render_ui

static void ui_draw_glyph(SDL_Renderer *renderer, SDL_Rect box,
                           UiGlyph glyph, SDL_Color color);
static void ui_draw_icon_label(Gfx *gfx, SDL_Rect face, const char *text,
                                UiGlyph glyph, SDL_Color color, int preferred);
#include "sudokura_sdl/modal_ui.inc"
#include "sudokura_sdl/games_ui.inc"
#include "sudokura_sdl/audio_ui.inc"
#include "sudokura_sdl/input_ui.inc"
#include "sudokura_sdl/restart_ui.inc"
#include "sudokura_sdl/new_game_ui.inc"
#include "sudokura_sdl/ui_rollout.inc"

#include "sudokura_sdl/05_main.inc"
