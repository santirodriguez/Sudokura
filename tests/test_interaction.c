#define SDL_MAIN_HANDLED
#include "app.h"
#include "input.h"

#include <assert.h>
#include <stdio.h>

static void begin_fixture(Game *game, AppState *state, uint64_t seed) {
  assert(game_new(game, seed));
  app_state_init(state);
  state->screen = APP_SCREEN_PLAY;
  state->prev_screen = APP_SCREEN_PLAY;
  state->session_open = true;
  state->has_session = true;
  state->mode = MODE_CLASSIC;
  state->result = APP_RESULT_NONE;
}

static int first_editable(const Game *game) {
  for (int i = 0; i < 81; ++i)
    if (!game->fixed[i]) return i;
  return -1;
}

static AppActionOutcome apply_cell(Game *game, AppState *state,
                                   AppActionKind kind, int index, int value) {
  AppAction action = {
      .kind = kind,
      .row = index / 9,
      .column = index % 9,
      .value = value,
  };
  return app_apply_action(game, state, action, 1.0);
}

static void complete_with_keyboard_contract(void) {
  Game game;
  AppState state;
  begin_fixture(&game, &state, UINT64_C(110));

  int cell = first_editable(&game);
  assert(cell >= 0);
  int note_value = 1;
  int mapped_note =
      input_digit_value((SDL_Keycode)(SDLK_0 + note_value), SDL_SCANCODE_1);
  assert(mapped_note == note_value);
  assert(apply_cell(&game, &state, APP_ACTION_NOTE, cell, mapped_note).changed);

#if defined(__APPLE__)
  SDL_Keymod primary = KMOD_GUI;
#else
  SDL_Keymod primary = KMOD_CTRL;
#endif
  InputPlayShortcut undo = input_play_shortcut(SDLK_z, primary);
  InputPlayShortcut redo =
      input_play_shortcut(SDLK_z, (SDL_Keymod)(primary | KMOD_SHIFT));
  assert(undo == INPUT_PLAY_UNDO);
  assert(redo == INPUT_PLAY_REDO);

  AppActionOutcome undone = app_apply_action(
      &game, &state, (AppAction){.kind = APP_ACTION_UNDO}, 1.1);
  assert(undone.changed);
  AppActionOutcome redone = app_apply_action(
      &game, &state, (AppAction){.kind = APP_ACTION_REDO}, 1.2);
  assert(redone.changed);

  AppActionOutcome cleared =
      apply_cell(&game, &state, APP_ACTION_CLEAR, cell, 0);
  assert(cleared.changed);

  assert(input_play_shortcut(SDLK_RETURN, primary) == INPUT_PLAY_VERIFY);
  AppActionOutcome verified = app_apply_action(
      &game, &state, (AppAction){.kind = APP_ACTION_VERIFY}, 1.3);
  assert(verified.revealed);

  const SDL_Keycode keypad[10] = {
      SDLK_KP_0, SDLK_KP_1, SDLK_KP_2, SDLK_KP_3, SDLK_KP_4,
      SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8, SDLK_KP_9,
  };
  for (int i = 0; i < 81; ++i) {
    if (game.fixed[i]) continue;
    int expected = game.solution[i];
    int mapped = (i & 1)
                     ? input_digit_value(keypad[expected], SDL_SCANCODE_UNKNOWN)
                     : input_digit_value((SDL_Keycode)(SDLK_0 + expected),
                                         SDL_SCANCODE_UNKNOWN);
    assert(mapped == expected);
    AppActionOutcome outcome =
        apply_cell(&game, &state, APP_ACTION_PLACE, i, mapped);
    assert(outcome.changed || outcome.terminal);
  }
  assert(game_is_solved(&game));
  assert(state.result == APP_RESULT_WIN);
}

static void complete_with_mouse_contract(void) {
  Game game;
  AppState state;
  begin_fixture(&game, &state, UINT64_C(111));

  assert(input_mouse_button_is_primary(SDL_BUTTON_LEFT));
  assert(!input_mouse_button_is_primary(SDL_BUTTON_RIGHT));
  assert(!input_mouse_button_is_primary(SDL_BUTTON_MIDDLE));

  int cell = first_editable(&game);
  assert(cell >= 0);
  int value = game.solution[cell];

  /* Right-click is reserved for the board-note gesture, never a primary UI
     activation. The visible Clear action removes it without keyboard input. */
  assert(apply_cell(&game, &state, APP_ACTION_NOTE, cell, value).changed);
  assert(apply_cell(&game, &state, APP_ACTION_CLEAR, cell, 0).changed);

  for (int i = 0; i < 81; ++i) {
    if (game.fixed[i]) continue;
    AppActionOutcome outcome =
        apply_cell(&game, &state, APP_ACTION_PLACE, i, game.solution[i]);
    assert(outcome.changed || outcome.terminal);
  }
  assert(game_is_solved(&game));
  assert(state.result == APP_RESULT_WIN);
}

int main(void) {
  complete_with_keyboard_contract();
  complete_with_mouse_contract();
  puts("keyboard and mouse interaction contracts complete full games");
  return 0;
}
