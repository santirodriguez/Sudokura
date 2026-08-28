CC ?= cc
CPPFLAGS += -I.
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic $(WERROR)
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null)
# SDL's MinGW pkg-config metadata targets GUI applications and may inject
# main=SDL_main, SDL2main, mingw32, and -mwindows. The SDL_ttf test is a
# normal console executable, so keep the library headers/libs but remove the
# application entry-point wrappers. tests/test_text_fit.c defines
# SDL_MAIN_HANDLED itself before including SDL headers.
SDL_TEST_CFLAGS := $(filter-out -Dmain=SDL_main,$(SDL_CFLAGS))
SDL_TEST_LIBS := $(filter-out -lmingw32 -lSDL2main -mwindows,$(SDL_LIBS))
CORE = game.c geometry.c i18n.c
.PHONY: all test test-ui clean assets
all: sudokura
sudokura: sudokura_sdl.c $(CORE) version.h game.h geometry.h i18n.h assets/generated/window_icon.c assets/generated/wordmark.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_CFLAGS) sudokura_sdl.c $(CORE) assets/generated/window_icon.c assets/generated/wordmark.c -o $@ $(SDL_LIBS) -lm
tests/test_main: tests/test_main.c $(CORE)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_main.c $(CORE) -o $@
tests/test_text_fit: tests/test_text_fit.c geometry.c i18n.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_TEST_CFLAGS) tests/test_text_fit.c geometry.c i18n.c -o $@ $(SDL_TEST_LIBS)
test-ui: tests/test_text_fit
	./tests/test_text_fit
test: tests/test_main
	./tests/test_main
assets:
	./scripts/generate_assets.py
	./scripts/validate_assets.py
assets/generated/window_icon.c assets/generated/window_icon.h assets/generated/wordmark.c assets/generated/wordmark.h: assets/branding/source/sudokura-icon.png assets/branding/source/sudokura-head.png assets/branding/source/favicon-16x16.png assets/branding/source/favicon-32x32.png assets/flags/raster/us.png assets/flags/raster/ar.png assets/flags/raster/es-ct.png scripts/generate_assets.go scripts/generate_assets.py
	./scripts/generate_assets.py
clean:
	rm -f sudokura tests/test_main tests/test_text_fit
