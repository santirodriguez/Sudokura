/* Sudokura SDL application is kept as one C translation unit. */
#include "session.c"
#include "seed.c"
#include "audio.h"
#include "assets/generated/flag_us.c"
#include "assets/generated/flag_ar.c"
#include "assets/generated/flag_ca.c"
#define persistence_init persistence_init_base
#include "src/sudokura_sdl/01_runtime.inc"
#undef persistence_init
#include "src/sudokura_sdl/02_font_discovery.inc"
#include "src/sudokura_sdl/03_board_render.inc"
#define app_init app_init_base
#define app_shutdown app_shutdown_base
#define app_frame app_frame_base
#include "src/sudokura_sdl/04_screens.inc"
#undef app_frame
#undef app_shutdown
#undef app_init
#include "src/sudokura_sdl/audio_ui.inc"
#define SDL_PollEvent audio_poll_event
#include "src/sudokura_sdl/05_main.inc"
#undef SDL_PollEvent
