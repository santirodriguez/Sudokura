#define SDL_MAIN_HANDLED
#include "input.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
  for (int value = 0; value <= 9; ++value)
    assert(input_digit_value((SDL_Keycode)(SDLK_0 + value), SDL_SCANCODE_UNKNOWN) == value);

  const SDL_Keycode keypad_keys[10] = {
      SDLK_KP_0, SDLK_KP_1, SDLK_KP_2, SDLK_KP_3, SDLK_KP_4,
      SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8, SDLK_KP_9,
  };
  const SDL_Scancode keypad_scancodes[10] = {
      SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2,
      SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5,
      SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8,
      SDL_SCANCODE_KP_9,
  };
  for (int value = 0; value <= 9; ++value) {
    assert(input_digit_value(keypad_keys[value], SDL_SCANCODE_UNKNOWN) == value);
    assert(input_digit_value(SDLK_UNKNOWN, keypad_scancodes[value]) == value);
  }

  /* Typical Num-Lock-off navigation keycodes must still resolve by scancode. */
  assert(input_digit_value(SDLK_END, SDL_SCANCODE_KP_1) == 1);
  assert(input_digit_value(SDLK_DOWN, SDL_SCANCODE_KP_2) == 2);
  assert(input_digit_value(SDLK_PAGEDOWN, SDL_SCANCODE_KP_3) == 3);
  assert(input_digit_value(SDLK_LEFT, SDL_SCANCODE_KP_4) == 4);
  assert(input_digit_value(SDLK_RIGHT, SDL_SCANCODE_KP_6) == 6);
  assert(input_digit_value(SDLK_HOME, SDL_SCANCODE_KP_7) == 7);
  assert(input_digit_value(SDLK_UP, SDL_SCANCODE_KP_8) == 8);
  assert(input_digit_value(SDLK_PAGEUP, SDL_SCANCODE_KP_9) == 9);
  assert(input_digit_value(SDLK_a, SDL_SCANCODE_A) == -1);

  puts("keyboard digit mapping passed for top row and physical keypad");
  return 0;
}
