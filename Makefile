CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -I./src
LDFLAGS ?=

ifeq ($(OS),Windows_NT)
SDL_CFLAGS ?=
SDL_LIBS ?= -lSDL2 -lSDL2main -lSDL2_ttf
else
UNAME_S := $(shell uname -s)
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null)
SDL_LIBS ?= $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null)
ifeq ($(UNAME_S),Darwin)
SDL_LIBS ?= -F/Library/Frameworks -framework SDL2 -framework SDL2_ttf
endif
endif
ifeq ($(strip $(SDL_LIBS)),)
SDL_LIBS := -lSDL2 -lSDL2_ttf
endif
ifeq ($(strip $(SDL_CFLAGS)),)
SDL_CFLAGS :=
endif

SRC_APP = src/main.c \
          src/core/game.c \
          src/ui/render.c \
          src/platform/fonts.c

OBJ_APP = $(SRC_APP:.c=.o)

APP = sudokura
TEST_BIN = tests/test_game

.PHONY: all clean test

all: $(APP)

$(APP): $(OBJ_APP)
	$(CC) $(CFLAGS) $(OBJ_APP) $(LDFLAGS) $(SDL_LIBS) -lm -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(TEST_BIN): tests/test_game.c src/core/game.c src/core/game.h
	$(CC) $(CFLAGS) tests/test_game.c src/core/game.c -o $@


test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(OBJ_APP) $(APP) $(TEST_BIN)
