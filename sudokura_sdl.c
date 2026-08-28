/* Sudokura SDL application is kept as one C translation unit. */
#include "session.c"
#include "seed.c"
#include "audio.h"
#include "input.c"
#include "assets/generated/flag_us.c"
#include "assets/generated/flag_ar.c"
#include "assets/generated/flag_ca.c"
#define persistence_init persistence_init_base
#define start_new_session start_new_session_base
#define start_daily_session start_daily_session_base
#define go_home go_home_base
#include "src/sudokura_sdl/01_runtime.inc"
#undef go_home
#undef start_daily_session
#undef start_new_session
#undef persistence_init
#include "src/sudokura_sdl/02_font_discovery.inc"
#include "src/sudokura_sdl/ui_geometry.inc"
#define geometry_compute ui_geometry_compute
#include "src/sudokura_sdl/03_board_render.inc"
#undef geometry_compute
#define app_init app_init_base
#define app_shutdown app_shutdown_base
#define app_frame app_frame_base
#define app_render app_render_base
#include "src/sudokura_sdl/04_screens.inc"
#undef app_render
#undef app_frame
#undef app_shutdown
#undef app_init
#define app_render app_render_polish_base
#include "src/sudokura_sdl/polish_ui.inc"
#undef app_render
#include "src/sudokura_sdl/about_overlay.inc"
#include "src/sudokura_sdl/audio_ui.inc"
#include "src/sudokura_sdl/input_ui.inc"
#define SDL_PollEvent ui_poll_event
#include "src/sudokura_sdl/05_main.inc"
#undef SDL_PollEvent
