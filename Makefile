CC ?= cc
CPPFLAGS += -I.
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic $(WERROR)
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null)
CORE = game.c geometry.c i18n.c
.PHONY: all test clean assets
all: sudokura
sudokura: sudokura_sdl.c $(CORE) version.h game.h geometry.h i18n.h assets/generated/window_icon.c assets/generated/wordmark.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_CFLAGS) sudokura_sdl.c $(CORE) assets/generated/window_icon.c assets/generated/wordmark.c -o $@ $(SDL_LIBS) -lm
tests/test_main: tests/test_main.c $(CORE)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_main.c $(CORE) -o $@
test: tests/test_main
	./tests/test_main
assets:
	./scripts/generate_assets.py
	./scripts/validate_assets.py
assets/generated/window_icon.c assets/generated/window_icon.h assets/generated/wordmark.c assets/generated/wordmark.h: assets/branding/source/Sudokura05.png assets/branding/source/Sudokura02.png scripts/generate_assets.py
	./scripts/generate_assets.py
clean:
	rm -f sudokura tests/test_main
