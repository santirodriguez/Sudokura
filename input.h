#ifndef SUDOKURA_INPUT_H
#define SUDOKURA_INPUT_H

#include <SDL2/SDL.h>

/* Return 0..9 for keyboard digits, including physical keypad scancodes.
   Return -1 when the event is not a numeric input. */
int input_digit_value(SDL_Keycode key, SDL_Scancode scancode);

#endif
