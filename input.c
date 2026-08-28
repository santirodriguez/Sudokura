#include "input.h"

static int keypad_key_value(SDL_Keycode key) {
  switch (key) {
    case SDLK_KP_0: return 0;
    case SDLK_KP_1: return 1;
    case SDLK_KP_2: return 2;
    case SDLK_KP_3: return 3;
    case SDLK_KP_4: return 4;
    case SDLK_KP_5: return 5;
    case SDLK_KP_6: return 6;
    case SDLK_KP_7: return 7;
    case SDLK_KP_8: return 8;
    case SDLK_KP_9: return 9;
    default: return -1;
  }
}

static int keypad_scancode_value(SDL_Scancode scancode) {
  switch (scancode) {
    case SDL_SCANCODE_KP_0: return 0;
    case SDL_SCANCODE_KP_1: return 1;
    case SDL_SCANCODE_KP_2: return 2;
    case SDL_SCANCODE_KP_3: return 3;
    case SDL_SCANCODE_KP_4: return 4;
    case SDL_SCANCODE_KP_5: return 5;
    case SDL_SCANCODE_KP_6: return 6;
    case SDL_SCANCODE_KP_7: return 7;
    case SDL_SCANCODE_KP_8: return 8;
    case SDL_SCANCODE_KP_9: return 9;
    default: return -1;
  }
}

int input_digit_value(SDL_Keycode key, SDL_Scancode scancode) {
  if (key >= SDLK_0 && key <= SDLK_9) return (int)(key - SDLK_0);

  int value = keypad_key_value(key);
  if (value >= 0) return value;

  /* With Num Lock off SDL can report navigation keycodes while retaining the
     physical keypad scancode. Use the scancode as the stable fallback. */
  return keypad_scancode_value(scancode);
}
