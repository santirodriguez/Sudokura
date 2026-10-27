cmake_minimum_required(VERSION 3.22.1)
project(SudokuraAndroid C)

get_filename_component(SUDOKURA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(DEPS "${SUDOKURA_ROOT}/android/.cache/source")
set(SDL_SRC "${DEPS}/SDL2-2.32.10")
set(TTF_SRC "${DEPS}/SDL2_ttf-2.24.0")
set(MIXER_SRC "${DEPS}/SDL2_mixer-2.8.1")

foreach(dep IN ITEMS "${SDL_SRC}" "${TTF_SRC}" "${MIXER_SRC}")
  if(NOT EXISTS "${dep}/.sudokura-source-sha256")
    message(FATAL_ERROR "Missing verified source: ${dep}. Run android/scripts/prepare_sources.py first.")
  endif()
endforeach()

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory("${SDL_SRC}" "${CMAKE_BINARY_DIR}/sdl2")

set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
add_subdirectory("${TTF_SRC}" "${CMAKE_BINARY_DIR}/ttf")

set(SDL2MIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_CMD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS STB CACHE STRING "" FORCE)
set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)
add_subdirectory("${MIXER_SRC}" "${CMAKE_BINARY_DIR}/mixer")

set(SUDOKURA_SOURCES
  src/sudokura_sdl.c
  src/app.c
  src/app_clock.c
  src/desktop.c
  src/url_launcher.c
  src/audio.c
  src/profile.c
  src/game_library.c
  src/game_library_codec.c
  src/game_migration.c
  src/save_policy.c
  src/store_io.c
  src/session.c
  src/seed.c
  src/progress.c
  src/input.c
  src/touch_input.c
  src/game.c
  src/human.c
  src/geometry.c
  src/i18n.c
  assets/generated/window_icon.c
  assets/generated/wordmark.c
  assets/generated/flag_us.c
  assets/generated/flag_ar.c
  assets/generated/flag_ca.c
)
foreach(source IN LISTS SUDOKURA_SOURCES)
  if(NOT EXISTS "${SUDOKURA_ROOT}/${source}")
    message(FATAL_ERROR "Missing shared Sudokura source/asset: ${source}")
  endif()
endforeach()
list(TRANSFORM SUDOKURA_SOURCES PREPEND "${SUDOKURA_ROOT}/")
add_library(main SHARED ${SUDOKURA_SOURCES})
target_include_directories(main PRIVATE "${SUDOKURA_ROOT}" "${SUDOKURA_ROOT}/src")
target_compile_features(main PRIVATE c_std_11)
target_compile_options(main PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(main PRIVATE SDL2 SDL2main SDL2_ttf SDL2_mixer log android m)
