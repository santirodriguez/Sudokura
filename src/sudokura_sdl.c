#if !defined(_WIN32) && !defined(__APPLE__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

/* Sudokura SDL application composition.

   Core/runtime modules are compiled as normal C translation units. The SDL
   presentation has one explicit UI surface after base-screen helpers:
   base helpers -> consolidated UI presentation -> audio/input -> main loop.
*/
#include "session.h"
#include "seed.h"
#include "audio.h"
#include "input.h"
#include "progress.h"

static void ui_audio_control_cancel_interaction(void);

#include "sudokura_sdl/01_runtime.inc"
#include "sudokura_sdl/02_font_discovery.inc"
#include "sudokura_sdl/03_board_render.inc"
#include "sudokura_sdl/04_screens.inc"
#include "sudokura_sdl/ui_present.inc"
#include "sudokura_sdl/audio_ui.inc"
#include "sudokura_sdl/input_ui.inc"

#include "sudokura_sdl/05_main.inc"
