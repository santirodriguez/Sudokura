#define SDL_MAIN_HANDLED
#include "input.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

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

  assert(!input_key_repeat_allowed(SDLK_1, SDL_SCANCODE_1));
  assert(!input_key_repeat_allowed(SDLK_UNKNOWN, SDL_SCANCODE_KP_1));
  assert(!input_key_repeat_allowed(SDLK_n, SDL_SCANCODE_N));
  assert(!input_key_repeat_allowed(SDLK_p, SDL_SCANCODE_P));
  assert(!input_key_repeat_allowed(SDLK_v, SDL_SCANCODE_V));
  assert(!input_key_repeat_allowed(SDLK_F1, SDL_SCANCODE_F1));
  assert(input_key_repeat_allowed(SDLK_UP, SDL_SCANCODE_UP));
  assert(input_key_repeat_allowed(SDLK_LEFT, SDL_SCANCODE_LEFT));
  assert(input_key_repeat_allowed(SDLK_w, SDL_SCANCODE_W));
  assert(input_key_repeat_allowed(SDLK_d, SDL_SCANCODE_D));

  assert(input_mouse_button_is_primary(SDL_BUTTON_LEFT));
  assert(!input_mouse_button_is_primary(SDL_BUTTON_RIGHT));
  assert(!input_mouse_button_is_primary(SDL_BUTTON_MIDDLE));

  assert(input_info_shortcut(SDLK_F1) == INPUT_INFO_HELP);
  assert(input_info_shortcut(SDLK_F2) == INPUT_INFO_ABOUT);
  assert(input_info_shortcut(SDLK_ESCAPE) == INPUT_INFO_NONE);

#if defined(__APPLE__)
  SDL_Keymod primary = KMOD_GUI;
  SDL_Keymod other = KMOD_CTRL;
  assert(strcmp(input_primary_modifier_label(), "Cmd") == 0);
#else
  SDL_Keymod primary = KMOD_CTRL;
  SDL_Keymod other = KMOD_GUI;
  assert(strcmp(input_primary_modifier_label(), "Ctrl") == 0);
#endif
  assert(input_play_shortcut(SDLK_z, primary) == INPUT_PLAY_UNDO);
  assert(input_play_shortcut(SDLK_z, (SDL_Keymod)(primary | KMOD_SHIFT)) ==
         INPUT_PLAY_REDO);
  assert(input_play_shortcut(SDLK_y, primary) == INPUT_PLAY_REDO);
  assert(input_play_shortcut(SDLK_RETURN, primary) == INPUT_PLAY_VERIFY);
  assert(input_play_shortcut(SDLK_KP_ENTER, primary) == INPUT_PLAY_VERIFY);
  assert(input_play_shortcut(SDLK_z, KMOD_NONE) == INPUT_PLAY_NONE);
  assert(input_play_shortcut(SDLK_z, other) == INPUT_PLAY_NONE);

  puts("keyboard digit, repeat, mouse-button, and info-shortcut policies passed");
  return 0;
}
