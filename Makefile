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
CORE = game.c geometry.c i18n.c
AUDIO_ASSETS = assets/audio/music-main.ogg assets/audio/music-fail.ogg \
	assets/audio/jingle-win.ogg assets/audio/jingle-fail.ogg
GENERATED_UI = assets/generated/window_icon.c assets/generated/window_icon.h \
	assets/generated/wordmark.c assets/generated/wordmark.h \
	assets/generated/flag_us.c assets/generated/flag_us.h \
	assets/generated/flag_ar.c assets/generated/flag_ar.h \
	assets/generated/flag_ca.c assets/generated/flag_ca.h
.PHONY: all test test-ui clean assets
all: sudokura
sudokura: sudokura_sdl.c src/sudokura_sdl/01_runtime.inc src/sudokura_sdl/02_font_discovery.inc src/sudokura_sdl/03_board_render.inc src/sudokura_sdl/04_screens.inc src/sudokura_sdl/05_main.inc src/sudokura_sdl/ui_geometry.inc src/sudokura_sdl/polish_ui.inc src/sudokura_sdl/audio_ui.inc $(CORE) audio.c audio.h version.h game.h geometry.h i18n.h session.h session.c seed.h seed.c $(GENERATED_UI) $(AUDIO_ASSETS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_CFLAGS) sudokura_sdl.c audio.c $(CORE) assets/generated/window_icon.c assets/generated/wordmark.c -o $@ $(SDL_LIBS) -lm
tests/test_main: tests/test_main.c $(CORE)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_main.c $(CORE) -o $@
tests/test_session: tests/test_session.c game.c session.c session.h game.h i18n.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_session.c game.c session.c -o $@
tests/test_seed: tests/test_seed.c seed.c seed.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_seed.c seed.c -o $@
tests/test_text_fit: tests/test_text_fit.c src/sudokura_sdl/ui_geometry.inc geometry.c i18n.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_text_fit.c geometry.c i18n.c -o $@ $(SDL_TEST_LIBS)
tests/test_audio: tests/test_audio.c audio.c audio.h $(AUDIO_ASSETS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_audio.c audio.c -o $@ $(SDL_TEST_LIBS) -lm
test-ui: tests/test_text_fit tests/test_audio
	./tests/test_text_fit
	./tests/test_audio
test: tests/test_main tests/test_session tests/test_seed
	./tests/test_main
	./tests/test_session
	./tests/test_seed
assets:
	./scripts/generate_assets.py
	./scripts/validate_assets.py
$(GENERATED_UI): assets/branding/source/android-chrome-512x512.png assets/branding/source/sudokura-head.png assets/flags/raster/us.png assets/flags/raster/ar.png assets/flags/raster/es-ct.png scripts/generate_assets.go scripts/generate_assets.py
	./scripts/generate_assets.py
clean:
	rm -f sudokura tests/test_main tests/test_session tests/test_seed tests/test_text_fit tests/test_audio
