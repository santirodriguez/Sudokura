#include "app.h"
#include "storage.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int first_playable(const Game *game) {
  for (int i = 0; i < SUDOKU_CELLS; ++i)
    if (!game->fixed[i]) return i;
  return -1;
}

static int wrong_value(const Game *game, int index) {
  int value = game->solution[index] % 9 + 1;
  assert(value != game->solution[index]);
  return value;
}

static AppState playing_state(GameMode mode) {
  AppState state;
  app_state_init(&state);
  state.screen = APP_SCREEN_PLAY;
  state.prev_screen = APP_SCREEN_PLAY;
  state.session_open = true;
  state.mode = mode;
  state.strikes_max = 3;
  state.time_limit_s = mode == MODE_TIME ? 600.0 : 0.0;
  return state;
}

static void fill_except(Game *game, int index) {
  memcpy(game->puzzle, game->solution, sizeof(game->puzzle));
  game->puzzle[index] = 0;
  game->notes[index] = 0;
}

static void test_navigation_contract(void) {
  AppState state;
  app_state_init(&state);
  assert(state.screen == APP_SCREEN_HOME);

  assert(app_open_aux(&state, APP_SCREEN_HELP));
  assert(state.screen == APP_SCREEN_HELP);
  assert(state.prev_screen == APP_SCREEN_HOME);
  assert(app_return_aux(&state));
  assert(state.screen == APP_SCREEN_HOME);

  assert(app_navigate(&state, APP_SCREEN_PLAY));
  assert(app_open_aux(&state, APP_SCREEN_SETTINGS));
  assert(state.prev_screen == APP_SCREEN_PLAY);
  assert(app_return_aux(&state));
  assert(state.screen == APP_SCREEN_PLAY);

  assert(app_open_aux(&state, APP_SCREEN_ABOUT));
  assert(state.prev_screen == APP_SCREEN_PLAY);
  assert(app_return_aux(&state));
  assert(!app_open_aux(&state, APP_SCREEN_RESULT));
}

static void test_no_change_and_strikes(void) {
  Game game;
  game_new(&game, 99);
  AppState state = playing_state(MODE_STRIKES);
  int index = first_playable(&game);
  assert(index >= 0);

  AppAction wrong = {
      .kind = APP_ACTION_PLACE,
      .row = index / 9,
      .column = index % 9,
      .value = wrong_value(&game, index),
  };
  AppActionOutcome first = app_apply_action(&game, &state, wrong, 10.0);
  assert(first.changed && first.error && !first.terminal);
  assert(state.mistakes == 1 && state.strikes == 1);

  AppActionOutcome repeated = app_apply_action(&game, &state, wrong, 11.0);
  assert(!repeated.changed && repeated.no_effect && !repeated.error);
  assert(state.mistakes == 1 && state.strikes == 1);
}

static void test_grouped_events_stop_at_loss(void) {
  Game game;
  game_new(&game, 1234);
  int index = first_playable(&game);
  assert(index >= 0);
  fill_except(&game, index);

  AppState state = playing_state(MODE_STRIKES);
  state.strikes = 2;
  state.mistakes = 2;

  AppAction wrong = {
      .kind = APP_ACTION_PLACE,
      .row = index / 9,
      .column = index % 9,
      .value = wrong_value(&game, index),
  };
  AppActionOutcome loss = app_apply_action(&game, &state, wrong, 20.0);
  assert(loss.changed && loss.error && loss.terminal);
  assert(loss.result == APP_RESULT_LOSE);
  assert(state.strikes == 3 && state.result == APP_RESULT_LOSE);

  AppAction correction = wrong;
  correction.value = game.solution[index];
  int before = game.puzzle[index];
  AppActionOutcome after_loss =
      app_apply_action(&game, &state, correction, 20.1);
  assert(after_loss.terminal && after_loss.no_effect && !after_loss.changed);
  assert(game.puzzle[index] == before);
  assert(!game_is_solved(&game));
  assert(state.result == APP_RESULT_LOSE);
}

static void test_win_is_terminal(void) {
  Game game;
  game_new(&game, 77);
  int index = first_playable(&game);
  assert(index >= 0);
  fill_except(&game, index);

  AppState state = playing_state(MODE_CLASSIC);
  AppAction final_move = {
      .kind = APP_ACTION_PLACE,
      .row = index / 9,
      .column = index % 9,
      .value = game.solution[index],
  };
  AppActionOutcome win = app_apply_action(&game, &state, final_move, 5.0);
  assert(win.changed && win.terminal && !win.error);
  assert(win.result == APP_RESULT_WIN);
  assert(state.result == APP_RESULT_WIN);
}

static void test_time_limit_preempts_input(void) {
  Game game;
  game_new(&game, 7);
  int index = first_playable(&game);
  assert(index >= 0);
  int before = game.puzzle[index];

  AppState state = playing_state(MODE_TIME);
  AppAction move = {
      .kind = APP_ACTION_PLACE,
      .row = index / 9,
      .column = index % 9,
      .value = game.solution[index],
  };
  AppActionOutcome outcome = app_apply_action(&game, &state, move, 601.0);
  assert(outcome.terminal && outcome.result == APP_RESULT_LOSE);
  assert(!outcome.changed && !outcome.no_effect);
  assert(game.puzzle[index] == before);
}

static void test_continue_action(void) {
  Game game;
  game_new(&game, 55);
  AppState state;
  app_state_init(&state);
  state.has_session = true;
  state.result = APP_RESULT_NONE;

  AppAction action = {.kind = APP_ACTION_CONTINUE};
  AppActionOutcome active = app_apply_action(&game, &state, action, 3.0);
  assert(active.changed && !active.terminal);
  assert(state.session_open && state.screen == APP_SCREEN_PLAY);

  state.session_open = false;
  state.screen = APP_SCREEN_HOME;
  state.result = APP_RESULT_LOSE;
  AppActionOutcome finished = app_apply_action(&game, &state, action, 4.0);
  assert(finished.changed && finished.terminal);
  assert(state.session_open && state.screen == APP_SCREEN_RESULT);
  assert(finished.result == APP_RESULT_LOSE);
}

static void test_hint_verify_and_restart(void) {
  Game game;
  game_new(&game, 314159);
  AppState state = playing_state(MODE_CLASSIC);
  int index = first_playable(&game);
  assert(index >= 0);

  AppAction hint = {
      .kind = APP_ACTION_HINT,
      .row = index / 9,
      .column = index % 9,
  };
  AppActionOutcome hinted = app_apply_action(&game, &state, hint, 1.0);
  assert(hinted.changed && hinted.revealed);
  assert(game.hinted[index]);

  AppAction verify = {.kind = APP_ACTION_VERIFY};
  AppActionOutcome verified = app_apply_action(&game, &state, verify, 1.0);
  assert(verified.revealed && !verified.no_effect);
  assert(verified.detail == game_conflict_count(&game));

  AppAction restart = {.kind = APP_ACTION_RESTART};
  AppActionOutcome restarted = app_apply_action(&game, &state, restart, 1.0);
  assert(restarted.changed);
  assert(state.result == APP_RESULT_NONE && state.strikes == 0);
  assert(game.puzzle[index] == game.initial[index]);
}

static int session_save_calls = 0;
static int preferences_save_calls = 0;

static bool fake_session_save(const char *path, const SessionState *state) {
  ++session_save_calls;
  (void)path;
  (void)state;
  return false;
}

static bool fake_preferences_save(const char *path,
                                  const Preferences *preferences) {
  ++preferences_save_calls;
  return path && preferences;
}

static void test_storage_is_substitutable(void) {
  StorageOps ops = {
      .save_session = fake_session_save,
      .save_preferences = fake_preferences_save,
  };
  SessionState session;
  Preferences preferences;
  memset(&session, 0, sizeof(session));
  memset(&preferences, 0, sizeof(preferences));

  assert(!storage_save_session(&ops, "session.dat", &session));
  assert(storage_save_preferences(&ops, "preferences.dat", &preferences));
  assert(session_save_calls == 1);
  assert(preferences_save_calls == 1);

  StorageOps missing = {0};
  assert(!storage_save_session(&missing, "session.dat", &session));
  assert(!storage_save_preferences(&missing, "preferences.dat", &preferences));
}

int main(void) {
  test_navigation_contract();
  test_no_change_and_strikes();
  test_grouped_events_stop_at_loss();
  test_win_is_terminal();
  test_time_limit_preempts_input();
  test_continue_action();
  test_hint_verify_and_restart();
  test_storage_is_substitutable();
  puts("application action sequencing, terminality, navigation, and storage substitution passed");
  return 0;
}
