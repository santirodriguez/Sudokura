CC ?= cc
CPPFLAGS += -I. -Isrc
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic $(WERROR)
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_mixer 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf SDL2_mixer 2>/dev/null)
# SDL's MinGW pkg-config metadata targets GUI applications and may inject
# main=SDL_main, SDL2main, mingw32, and -mwindows. The SDL-based tests are
# normal console executables, so keep the library headers/libs but remove the
# application entry-point wrappers. Tests define SDL_MAIN_HANDLED themselves.
SDL_TEST_CFLAGS := $(filter-out -Dmain=SDL_main,$(SDL_CFLAGS))
SDL_TEST_LIBS := $(filter-out -lmingw32 -lSDL2main -mwindows,$(SDL_LIBS))
PLATFORM_LIBS :=
ifeq ($(OS),Windows_NT)
PLATFORM_LIBS += -lshell32
endif
CORE = src/game.c src/human.c src/geometry.c src/i18n.c
LIBRARY = src/game_library.c src/game_library_codec.c src/game_migration.c
AUDIO_ASSETS = assets/audio/music-main.ogg assets/audio/music-fail.ogg \
	assets/audio/jingle-win.ogg assets/audio/jingle-fail.ogg
GENERATED_UI = assets/generated/window_icon.c assets/generated/window_icon.h \
	assets/generated/wordmark.c assets/generated/wordmark.h \
	assets/generated/flag_us.c assets/generated/flag_us.h \
	assets/generated/flag_ar.c assets/generated/flag_ar.h \
	assets/generated/flag_ca.c assets/generated/flag_ca.h
.PHONY: all test test-ui quality clean assets
all: sudokura
sudokura: src/sudokura_sdl.c src/app.c src/app.h src/app_clock.c src/app_clock.h src/desktop.c src/desktop.h src/url_launcher.c src/url_launcher.h src/profile.c src/profile.h src/game_library.c src/game_library_codec.c src/game_migration.c src/game_library.h src/game_migration.h src/save_policy.c src/save_policy.h src/store_io.c src/store_io.h src/store_status.h src/storage.h src/sudokura_sdl/01_runtime.inc src/sudokura_sdl/02_font_discovery.inc src/sudokura_sdl/03_board_render.inc src/sudokura_sdl/04_screens.inc src/sudokura_sdl/05_main.inc src/sudokura_sdl/restart_ui.inc src/sudokura_sdl/ui_present.inc src/sudokura_sdl/modal_ui.inc src/app_modal.h src/sudokura_sdl/games_ui.inc src/button_feedback.h src/pointer_capture.h src/library_paging.h src/touch_input.h src/touch_input.c src/sudokura_sdl/audio_ui.inc src/sudokura_sdl/input_ui.inc $(CORE) src/audio.c src/audio.h src/input.c src/input.h src/progress.c src/progress.h src/version.h src/game.h src/human.h src/geometry.h src/i18n.h src/session.h src/session.c src/seed.h src/seed.c $(GENERATED_UI) $(AUDIO_ASSETS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_CFLAGS) src/sudokura_sdl.c src/app.c src/app_clock.c src/desktop.c src/url_launcher.c src/audio.c src/profile.c $(LIBRARY) src/save_policy.c src/store_io.c src/session.c src/seed.c src/progress.c src/input.c src/touch_input.c $(CORE) assets/generated/window_icon.c assets/generated/wordmark.c assets/generated/flag_us.c assets/generated/flag_ar.c assets/generated/flag_ca.c -o $@ $(SDL_LIBS) $(PLATFORM_LIBS) -lm
tests/test_main: tests/test_main.c $(CORE) src/version.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_main.c $(CORE) -o $@
tests/test_app: tests/test_app.c src/app.c src/app.h src/storage.h src/game.c src/game.h src/session.h src/i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_app.c src/app.c src/game.c src/human.c -o $@
tests/test_clock: tests/test_clock.c src/app_clock.c src/app_clock.h src/app.c src/app.h src/game.c src/game.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_clock.c src/app_clock.c src/app.c src/game.c src/human.c -o $@
tests/test_session: tests/test_session.c src/game.c src/session.c src/session.h src/store_io.c src/store_io.h src/store_status.h src/game.h src/i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_session.c src/game.c src/human.c src/session.c src/store_io.c -o $@
tests/test_store: tests/test_store.c src/store_io.c src/store_io.h src/store_status.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_STORE_TESTING tests/test_store.c src/store_io.c -o $@
tests/test_save_policy: tests/test_save_policy.c src/save_policy.c src/save_policy.h src/store_status.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_save_policy.c src/save_policy.c -o $@
tests/test_profile: tests/test_profile.c src/profile.c src/profile.h src/session.c src/session.h src/store_io.c src/store_io.h src/store_status.h src/game.c src/game.h src/i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_profile.c src/profile.c src/session.c src/store_io.c src/game.c src/human.c -o $@
tests/test_game_library: tests/test_game_library.c src/game_library.c src/game_library_codec.c src/game_library.h src/profile.c src/profile.h src/store_io.c src/seed.c src/session.c src/game.c src/human.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_STORE_TESTING -DSUDOKURA_GAME_TESTING tests/test_game_library.c $(LIBRARY) src/profile.c src/store_io.c src/seed.c src/session.c src/game.c src/human.c -o $@
tests/test_game_migration: tests/test_game_migration.c $(LIBRARY) src/game_migration.h src/game_library.h src/profile.c src/profile.h src/store_io.c src/seed.c src/session.c src/game.c src/human.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_STORE_TESTING tests/test_game_migration.c $(LIBRARY) src/profile.c src/store_io.c src/seed.c src/session.c src/game.c src/human.c -o $@
tests/test_live_save: tests/test_live_save.c src/profile.c src/profile.h src/save_policy.c src/save_policy.h src/session.c src/session.h src/store_io.c src/store_io.h src/store_status.h src/game.c src/game.h src/i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_STORE_TESTING tests/test_live_save.c src/profile.c src/save_policy.c src/session.c src/store_io.c src/game.c src/human.c -o $@
tests/test_human: tests/test_human.c src/human.c src/human.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_human.c src/human.c -o $@
tests/test_human_calibration: tests/test_human_calibration.c src/human.c src/human.h tests/fixtures/human_difficulty_20240415.csv
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_human_calibration.c src/human.c -o $@
tests/test_generator_quality: tests/test_generator_quality.c src/game.c src/game.h src/human.c src/human.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_generator_quality.c src/game.c src/human.c -o $@
tests/test_seed: tests/test_seed.c src/seed.c src/seed.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_seed.c src/seed.c -o $@
tests/test_deterministic_parity: tests/test_deterministic_parity.c src/game.c src/game.h src/human.c src/human.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_deterministic_parity.c src/game.c src/human.c -o $@
tests/test_progress: tests/test_progress.c src/progress.c src/progress.h src/game.c src/game.h src/session.c src/session.h src/store_io.c src/store_io.h src/store_status.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_progress.c src/progress.c src/game.c src/human.c src/session.c src/store_io.c -o $@
tests/test_geometry_ui: tests/test_geometry_ui.c src/geometry.c src/geometry.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_geometry_ui.c src/geometry.c -o $@
tests/test_desktop: tests/test_desktop.c src/desktop.c src/desktop.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_desktop.c src/desktop.c -o $@
tests/test_url_launcher: tests/test_url_launcher.c src/url_launcher.c src/url_launcher.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_URL_LAUNCHER_TESTING tests/test_url_launcher.c src/url_launcher.c -o $@ $(PLATFORM_LIBS)
tests/test_text_fit: tests/test_text_fit.c src/geometry.c src/i18n.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_text_fit.c src/geometry.c src/i18n.c -o $@ $(SDL_TEST_LIBS)
tests/test_audio: tests/test_audio.c src/audio.c src/audio.h $(AUDIO_ASSETS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_AUDIO_TESTING $(SDL_TEST_CFLAGS) tests/test_audio.c src/audio.c -o $@ $(SDL_TEST_LIBS) -lm
tests/test_input: tests/test_input.c src/input.c src/input.h src/button_feedback.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_input.c src/input.c -o $@
tests/test_i18n: tests/test_i18n.c src/i18n.c src/i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_i18n.c src/i18n.c -o $@
tests/test_interaction: tests/test_interaction.c src/app.c src/app.h src/input.c src/input.h src/game.c src/game.h src/human.c src/human.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_interaction.c src/app.c src/input.c src/game.c src/human.c -o $@
test-ui: tests/test_text_fit tests/test_audio tests/test_input tests/test_i18n tests/test_interaction
	./tests/test_text_fit
	./tests/test_audio
	./tests/test_input
	./tests/test_i18n
	./tests/test_interaction
quality: tests/test_generator_quality tests/test_human_calibration
	./tests/test_human_calibration
	./tests/test_generator_quality

tests/test_library_paging: tests/test_library_paging.c src/library_paging.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_library_paging.c -o $@

tests/test_pointer_capture: tests/test_pointer_capture.c src/pointer_capture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_pointer_capture.c -o $@
tests/test_touch_input: tests/test_touch_input.c src/touch_input.c src/touch_input.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_touch_input.c src/touch_input.c -o $@ -lm
tests/test_android_viewport: tests/test_android_viewport.c src/android_viewport.c src/android_viewport.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_android_viewport.c src/android_viewport.c -o $@ -lm
tests/test_mobile_geometry: tests/test_mobile_geometry.c src/geometry.c src/geometry.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_mobile_geometry.c src/geometry.c -o $@
tests/test_android_lifecycle: tests/test_android_lifecycle.c src/android_lifecycle.c src/android_lifecycle.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_android_lifecycle.c src/android_lifecycle.c -o $@

tests/test_app_modal: tests/test_app_modal.c src/app_modal.h src/app_clock.c src/app_clock.h src/app.h src/game.h src/human.h src/i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_app_modal.c src/app_clock.c -o $@

test: tests/test_app_modal tests/test_library_paging tests/test_pointer_capture tests/test_touch_input tests/test_android_viewport tests/test_mobile_geometry tests/test_android_lifecycle tests/test_main tests/test_app tests/test_clock tests/test_game_library tests/test_game_migration tests/test_session tests/test_store tests/test_save_policy tests/test_profile tests/test_live_save tests/test_human tests/test_generator_quality tests/test_human_calibration tests/test_seed tests/test_deterministic_parity tests/test_progress tests/test_geometry_ui tests/test_desktop tests/test_url_launcher
	./tests/test_app_modal
	./tests/test_library_paging
	./tests/test_pointer_capture
	./tests/test_touch_input
	./tests/test_android_viewport
	./tests/test_mobile_geometry
	./tests/test_android_lifecycle
	./tests/test_main
	./tests/test_app
	./tests/test_clock
	./tests/test_game_library
	./tests/test_game_migration
	./tests/test_session
	./tests/test_store
	./tests/test_save_policy
	./tests/test_profile
	./tests/test_live_save
	./tests/test_human
	./tests/test_seed
	./tests/test_deterministic_parity > /dev/null
	./tests/test_progress
	./tests/test_geometry_ui
	./tests/test_desktop
	./tests/test_url_launcher
assets:
	./scripts/generate_assets.py
	./scripts/validate_assets.py
$(GENERATED_UI): assets/branding/source/sudokura-512.png assets/branding/source/sudokura-head.png assets/flags/raster/us.png assets/flags/raster/ar.png assets/flags/raster/es-ct.png scripts/generate_assets.go scripts/generate_assets.py
	./scripts/generate_assets.py
clean:
	rm -f sudokura tests/test_app_modal tests/test_library_paging tests/test_pointer_capture tests/test_touch_input tests/test_android_viewport tests/test_mobile_geometry tests/test_android_lifecycle tests/test_main tests/test_app tests/test_clock tests/test_game_library tests/test_game_migration tests/test_session tests/test_store tests/test_save_policy tests/test_profile tests/test_live_save tests/test_human tests/test_human_calibration tests/test_generator_quality tests/test_seed tests/test_deterministic_parity tests/test_progress tests/test_geometry_ui tests/test_desktop tests/test_url_launcher tests/test_text_fit tests/test_audio tests/test_input tests/test_i18n tests/test_interaction
