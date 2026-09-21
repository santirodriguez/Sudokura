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


bool input_key_repeat_allowed(SDL_Keycode key, SDL_Scancode scancode) {
  if (input_digit_value(key, scancode) >= 0) return false;
  switch (key) {
    case SDLK_UP:
    case SDLK_DOWN:
    case SDLK_LEFT:
    case SDLK_RIGHT:
    case SDLK_w:
    case SDLK_a:
    case SDLK_s:
    case SDLK_d:
      return true;
    default:
      return false;
  }
}

bool input_mouse_button_is_primary(Uint8 button) {
  return button == SDL_BUTTON_LEFT;
}

bool input_activation_key(SDL_Keycode key) {
  return key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE;
}

int input_focus_step(int current, int count, int direction) {
  if (count <= 0) return -1;
  int step = direction < 0 ? -1 : 1;
  if (current < 0 || current >= count)
    return step < 0 ? count - 1 : 0;
  return (current + step + count) % count;
}

InputInfoShortcut input_info_shortcut(SDL_Keycode key) {
  if (key == SDLK_F1) return INPUT_INFO_HELP;
  if (key == SDLK_F2) return INPUT_INFO_ABOUT;
  return INPUT_INFO_NONE;
}


static bool input_primary_modifier_active(SDL_Keymod modifiers) {
#if defined(__APPLE__)
  return (modifiers & KMOD_GUI) != 0;
#else
  return (modifiers & KMOD_CTRL) != 0;
#endif
}

InputPlayShortcut input_play_shortcut(SDL_Keycode key, SDL_Keymod modifiers) {
  if (!input_primary_modifier_active(modifiers)) return INPUT_PLAY_NONE;
  bool shifted = (modifiers & KMOD_SHIFT) != 0;
  if (key == SDLK_z) return shifted ? INPUT_PLAY_REDO : INPUT_PLAY_UNDO;
  if (key == SDLK_y) return INPUT_PLAY_REDO;
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) return INPUT_PLAY_VERIFY;
  return INPUT_PLAY_NONE;
}

const char *input_primary_modifier_label(void) {
#if defined(__APPLE__)
  return "Cmd";
#else
  return "Ctrl";
#endif
}


InputAudioShortcut input_audio_shortcut(SDL_Keycode key, SDL_Keymod modifiers,
                                        bool popup_open,
                                        bool control_available) {
  if (key == SDLK_v) {
    if ((modifiers & KMOD_SHIFT) != 0)
      return control_available ? INPUT_AUDIO_POPUP : INPUT_AUDIO_NONE;
    return INPUT_AUDIO_MASTER;
  }
  if (!control_available || !popup_open) return INPUT_AUDIO_NONE;
  if (key == SDLK_m) return INPUT_AUDIO_MUSIC;
  if (key == SDLK_f) return INPUT_AUDIO_FX;
  if (key == SDLK_ESCAPE) return INPUT_AUDIO_CLOSE;
  return INPUT_AUDIO_NONE;
}

bool input_audio_press_is_long(Uint64 held_ms) {
  return held_ms >= UINT64_C(500);
}
