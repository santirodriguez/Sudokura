CC ?= cc
CPPFLAGS += -I.
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
CORE = game.c human.c geometry.c i18n.c
AUDIO_ASSETS = assets/audio/music-main.ogg assets/audio/music-fail.ogg \
	assets/audio/jingle-win.ogg assets/audio/jingle-fail.ogg
GENERATED_UI = assets/generated/window_icon.c assets/generated/window_icon.h \
	assets/generated/wordmark.c assets/generated/wordmark.h \
	assets/generated/flag_us.c assets/generated/flag_us.h \
	assets/generated/flag_ar.c assets/generated/flag_ar.h \
	assets/generated/flag_ca.c assets/generated/flag_ca.h
.PHONY: all test test-ui quality clean assets
all: sudokura
sudokura: sudokura_sdl.c app.c app.h app_clock.c app_clock.h desktop.c desktop.h url_launcher.c url_launcher.h profile.c profile.h save_policy.c save_policy.h store_io.c store_io.h store_status.h storage.h src/sudokura_sdl/01_runtime.inc src/sudokura_sdl/02_font_discovery.inc src/sudokura_sdl/03_board_render.inc src/sudokura_sdl/04_screens.inc src/sudokura_sdl/05_main.inc src/sudokura_sdl/ui_present.inc src/sudokura_sdl/audio_ui.inc src/sudokura_sdl/input_ui.inc $(CORE) audio.c audio.h input.c input.h progress.c progress.h version.h game.h human.h geometry.h i18n.h session.h session.c seed.h seed.c $(GENERATED_UI) $(AUDIO_ASSETS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_CFLAGS) sudokura_sdl.c app.c app_clock.c desktop.c url_launcher.c audio.c profile.c save_policy.c store_io.c session.c seed.c progress.c input.c $(CORE) assets/generated/window_icon.c assets/generated/wordmark.c assets/generated/flag_us.c assets/generated/flag_ar.c assets/generated/flag_ca.c -o $@ $(SDL_LIBS) $(PLATFORM_LIBS) -lm
tests/test_main: tests/test_main.c $(CORE)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_main.c $(CORE) -o $@
tests/test_app: tests/test_app.c app.c app.h storage.h game.c game.h session.h i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_app.c app.c game.c human.c -o $@
tests/test_clock: tests/test_clock.c app_clock.c app_clock.h app.c app.h game.c game.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_clock.c app_clock.c app.c game.c human.c -o $@
tests/test_session: tests/test_session.c game.c session.c session.h store_io.c store_io.h store_status.h game.h i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_session.c game.c human.c session.c store_io.c -o $@
tests/test_store: tests/test_store.c store_io.c store_io.h store_status.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_STORE_TESTING tests/test_store.c store_io.c -o $@
tests/test_save_policy: tests/test_save_policy.c save_policy.c save_policy.h store_status.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_save_policy.c save_policy.c -o $@
tests/test_profile: tests/test_profile.c profile.c profile.h session.c session.h store_io.c store_io.h store_status.h game.c game.h i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_profile.c profile.c session.c store_io.c game.c human.c -o $@
tests/test_live_save: tests/test_live_save.c profile.c profile.h save_policy.c save_policy.h session.c session.h store_io.c store_io.h store_status.h game.c game.h i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_STORE_TESTING tests/test_live_save.c profile.c save_policy.c session.c store_io.c game.c human.c -o $@
tests/test_human: tests/test_human.c human.c human.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_human.c human.c -o $@
tests/test_human_calibration: tests/test_human_calibration.c human.c human.h tests/fixtures/human_difficulty_20240415.csv
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_human_calibration.c human.c -o $@
tests/test_generator_quality: tests/test_generator_quality.c game.c game.h human.c human.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_generator_quality.c game.c human.c -o $@
tests/test_seed: tests/test_seed.c seed.c seed.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_seed.c seed.c -o $@
tests/test_progress: tests/test_progress.c progress.c progress.h game.c game.h session.c session.h store_io.c store_io.h store_status.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_progress.c progress.c game.c human.c session.c store_io.c -o $@
tests/test_geometry_ui: tests/test_geometry_ui.c geometry.c geometry.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_geometry_ui.c geometry.c -o $@
tests/test_desktop: tests/test_desktop.c desktop.c desktop.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_desktop.c desktop.c -o $@
tests/test_url_launcher: tests/test_url_launcher.c url_launcher.c url_launcher.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_URL_LAUNCHER_TESTING tests/test_url_launcher.c url_launcher.c -o $@ $(PLATFORM_LIBS)
tests/test_text_fit: tests/test_text_fit.c geometry.c i18n.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_text_fit.c geometry.c i18n.c -o $@ $(SDL_TEST_LIBS)
tests/test_audio: tests/test_audio.c audio.c audio.h $(AUDIO_ASSETS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUDOKURA_AUDIO_TESTING $(SDL_TEST_CFLAGS) tests/test_audio.c audio.c -o $@ $(SDL_TEST_LIBS) -lm
tests/test_input: tests/test_input.c input.c input.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_input.c input.c -o $@
tests/test_i18n: tests/test_i18n.c i18n.c i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_i18n.c i18n.c -o $@
tests/test_interaction: tests/test_interaction.c app.c app.h input.c input.h game.c game.h human.c human.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_interaction.c app.c input.c game.c human.c -o $@
test-ui: tests/test_text_fit tests/test_audio tests/test_input tests/test_i18n tests/test_interaction
	./tests/test_text_fit
	./tests/test_audio
	./tests/test_input
	./tests/test_i18n
	./tests/test_interaction
quality: tests/test_generator_quality tests/test_human_calibration
	./tests/test_human_calibration
	./tests/test_generator_quality
test: tests/test_main tests/test_app tests/test_clock tests/test_session tests/test_store tests/test_save_policy tests/test_profile tests/test_live_save tests/test_human tests/test_generator_quality tests/test_human_calibration tests/test_seed tests/test_progress tests/test_geometry_ui tests/test_desktop tests/test_url_launcher
	./tests/test_main
	./tests/test_app
	./tests/test_clock
	./tests/test_session
	./tests/test_store
	./tests/test_save_policy
	./tests/test_profile
	./tests/test_live_save
	./tests/test_human
	./tests/test_seed
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
	rm -f sudokura tests/test_main tests/test_app tests/test_clock tests/test_session tests/test_store tests/test_save_policy tests/test_profile tests/test_live_save tests/test_human tests/test_human_calibration tests/test_generator_quality tests/test_seed tests/test_progress tests/test_geometry_ui tests/test_desktop tests/test_url_launcher tests/test_text_fit tests/test_audio tests/test_input tests/test_i18n tests/test_interaction
