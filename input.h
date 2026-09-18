#ifndef SUDOKURA_INPUT_H
#define SUDOKURA_INPUT_H

#include <SDL2/SDL.h>
#include <stdbool.h>

/* Return 0..9 for keyboard digits, including physical keypad scancodes.
   Return -1 when the event is not a numeric input. */
int input_digit_value(SDL_Keycode key, SDL_Scancode scancode);

/* Auto-repeat is reserved for continuous selection movement. Digits and
   toggles/actions must be generated once per physical key press. */
bool input_key_repeat_allowed(SDL_Keycode key, SDL_Scancode scancode);

/* UI controls use the primary mouse button. The secondary button remains
   available only for the documented board-note gesture. */
bool input_mouse_button_is_primary(Uint8 button);

#endif
