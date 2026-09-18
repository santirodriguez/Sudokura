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

/* Shared keyboard accessibility helpers. */
bool input_activation_key(SDL_Keycode key);
int input_focus_step(int current, int count, int direction);

typedef enum {
  INPUT_INFO_NONE = 0,
  INPUT_INFO_HELP,
  INPUT_INFO_ABOUT
} InputInfoShortcut;

InputInfoShortcut input_info_shortcut(SDL_Keycode key);

typedef enum {
  INPUT_PLAY_NONE = 0,
  INPUT_PLAY_UNDO,
  INPUT_PLAY_REDO,
  INPUT_PLAY_VERIFY
} InputPlayShortcut;

InputPlayShortcut input_play_shortcut(SDL_Keycode key, SDL_Keymod modifiers);
const char *input_primary_modifier_label(void);

#endif
