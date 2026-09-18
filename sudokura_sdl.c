/* Sudokura SDL application composition.

   Core/runtime modules are compiled as normal C translation units. The SDL
   presentation remains split into internal fragments, but their order and
   active render path are explicit here: base screens -> presentation polish
   -> desktop About adjustment -> final controls/audio -> input -> main loop.
*/
#include "session.h"
#include "seed.h"
#include "audio.h"
#include "input.h"
#include "progress.h"

#include "src/sudokura_sdl/01_runtime.inc"
#include "src/sudokura_sdl/02_font_discovery.inc"
#include "src/sudokura_sdl/ui_geometry.inc"
#include "src/sudokura_sdl/03_board_render.inc"
#include "src/sudokura_sdl/04_screens.inc"
#include "src/sudokura_sdl/polish_ui.inc"
#include "src/sudokura_sdl/about_overlay.inc"
#include "src/sudokura_sdl/rc2_ui.inc"
#include "src/sudokura_sdl/audio_ui.inc"
#include "src/sudokura_sdl/input_ui.inc"

#define SDL_PollEvent ui_poll_event
#include "src/sudokura_sdl/05_main.inc"
#undef SDL_PollEvent
