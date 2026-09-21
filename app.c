#include "app.h"

#include <string.h>

static AppActionOutcome outcome_none(const AppState *state) {
  AppActionOutcome outcome;
  memset(&outcome, 0, sizeof(outcome));
  outcome.no_effect = true;
  outcome.input = GAME_INPUT_NO_CHANGE;
  outcome.result = state ? state->result : APP_RESULT_NONE;
  return outcome;
}

static void history_push(GameEdit history[SUDOKURA_HISTORY_LIMIT],
                         uint16_t *count, const GameEdit *edit) {
  if (!history || !count || !edit) return;
  if (*count == SUDOKURA_HISTORY_LIMIT) {
    memmove(history, history + 1,
            (SUDOKURA_HISTORY_LIMIT - 1u) * sizeof(history[0]));
    --*count;
  }
  history[(*count)++] = *edit;
}

static void history_record(AppState *state, const GameEdit *edit) {
  history_push(state->undo, &state->undo_count, edit);
  state->redo_count = 0;
}

void app_state_init(AppState *state) {
  if (!state) return;
  memset(state, 0, sizeof(*state));
  state->sel_r = 4;
  state->sel_c = 4;
  state->language = LANG_EN;
  state->dark_theme = true;
  state->reduced_motion = false;
  state->mode = MODE_CLASSIC;
  state->strikes_max = 3;
  state->screen = APP_SCREEN_HOME;
  state->prev_screen = APP_SCREEN_HOME;
  state->result = APP_RESULT_NONE;
}

bool app_screen_is_auxiliary(AppScreen screen) {
  return screen == APP_SCREEN_HELP || screen == APP_SCREEN_SETTINGS ||
         screen == APP_SCREEN_ABOUT;
}

bool app_pause_hides_play(const AppState *state) {
  return state && (state->pause_reasons & APP_PAUSE_USER_VISIBLE) != 0;
}

bool app_open_aux(AppState *state, AppScreen screen) {
  if (!state || !app_screen_is_auxiliary(screen) || state->screen == screen)
    return false;
  if (!app_screen_is_auxiliary(state->screen))
    state->prev_screen = state->screen;
  state->screen = screen;
  return true;
}

bool app_return_aux(AppState *state) {
  if (!state || !app_screen_is_auxiliary(state->screen)) return false;
  AppScreen target = state->prev_screen;
  if (app_screen_is_auxiliary(target)) target = APP_SCREEN_HOME;
  state->screen = target;
  return true;
}

bool app_navigate(AppState *state, AppScreen target) {
  if (!state || target < APP_SCREEN_HOME || target > APP_SCREEN_SETTINGS)
    return false;
  if (app_screen_is_auxiliary(target)) return app_open_aux(state, target);
  if (state->screen == target) return false;
  state->prev_screen = state->screen;
  state->screen = target;
  return true;
}

static void mark_terminal(Game *game, AppState *state, double elapsed_s,
                          AppActionOutcome *outcome) {
  bool lost =
      game_mode_lost(state->mode, state->strikes, state->strikes_max,
                     elapsed_s, state->time_limit_s);
  bool solved = game_is_solved(game);
  if (!lost && !solved) return;

  state->result = lost ? APP_RESULT_LOSE : APP_RESULT_WIN;
  outcome->terminal = true;
  outcome->no_effect = false;
  outcome->result = state->result;
}

AppActionOutcome app_check_terminal(Game *game, AppState *state,
                                    double elapsed_s) {
  AppActionOutcome outcome = outcome_none(state);
  if (!game || !state) return outcome;

  if (state->result != APP_RESULT_NONE ||
      state->screen == APP_SCREEN_RESULT) {
    outcome.terminal = true;
    outcome.result = state->result;
    return outcome;
  }

  mark_terminal(game, state, elapsed_s, &outcome);
  return outcome;
}

static GameInputResult apply_cell_action(Game *game, AppState *state,
                                         AppAction action, GameEdit *edit) {
  switch (action.kind) {
    case APP_ACTION_PLACE:
      return game_apply_input_recorded(
          game, action.row, action.column, action.value, false,
          state->strict_mode, state->auto_remove_peer_notes, edit);
    case APP_ACTION_NOTE:
      return game_apply_input_recorded(
          game, action.row, action.column, action.value, true,
          state->strict_mode, false, edit);
    case APP_ACTION_CLEAR:
      return game_apply_input_recorded(
          game, action.row, action.column, 0, false,
          state->strict_mode, false, edit);
    default:
      return GAME_INPUT_NO_CHANGE;
  }
}

static AppActionOutcome apply_history_action(Game *game, AppState *state,
                                             bool redo, double elapsed_s) {
  AppActionOutcome outcome = outcome_none(state);
  GameEdit *source = redo ? state->redo : state->undo;
  uint16_t *source_count = redo ? &state->redo_count : &state->undo_count;
  GameEdit *destination = redo ? state->undo : state->redo;
  uint16_t *destination_count =
      redo ? &state->undo_count : &state->redo_count;

  if (*source_count == 0) {
    outcome.blocked = true;
    return outcome;
  }

  GameEdit edit = source[*source_count - 1u];
  if (!game_apply_edit(game, &edit, redo)) {
    outcome.blocked = true;
    return outcome;
  }

  --*source_count;
  history_push(destination, destination_count, &edit);
  state->sel_r = edit.row;
  state->sel_c = edit.column;
  outcome.changed = true;
  outcome.no_effect = false;
  if (redo) mark_terminal(game, state, elapsed_s, &outcome);
  return outcome;
}

AppActionOutcome app_apply_action(Game *game, AppState *state,
                                  AppAction action, double elapsed_s) {
  AppActionOutcome outcome = outcome_none(state);
  if (!state) return outcome;

  if (action.kind == APP_ACTION_NAVIGATE) {
    outcome.changed = app_navigate(state, action.target);
    outcome.no_effect = !outcome.changed;
    outcome.result = state->result;
    return outcome;
  }

  if (action.kind == APP_ACTION_RESTART) {
    if (!game) return outcome;
    game_restart(game);
    state->mistakes = 0;
    state->strikes = 0;
    state->notes_mode = false;
    state->undo_count = 0;
    state->redo_count = 0;
    state->elapsed_ms = 0;
    state->running_since_ms = 0;
    state->pause_reasons = 0;
    state->session_open = true;
    state->result = APP_RESULT_NONE;
    state->screen = APP_SCREEN_PLAY;
    state->prev_screen = APP_SCREEN_PLAY;
    outcome.changed = true;
    outcome.no_effect = false;
    outcome.result = APP_RESULT_NONE;
    return outcome;
  }

  if (action.kind == APP_ACTION_CONTINUE) {
    if (!game || !state->has_session) return outcome;
    state->session_open = true;
    state->screen = state->result == APP_RESULT_NONE ? APP_SCREEN_PLAY
                                                     : APP_SCREEN_RESULT;
    state->prev_screen = state->screen;
    outcome.changed = true;
    outcome.terminal = state->result != APP_RESULT_NONE;
    outcome.no_effect = false;
    outcome.result = state->result;
    return outcome;
  }

  if (!game) return outcome;

  bool play_action =
      action.kind == APP_ACTION_PLACE || action.kind == APP_ACTION_NOTE ||
      action.kind == APP_ACTION_CLEAR || action.kind == APP_ACTION_UNDO ||
      action.kind == APP_ACTION_REDO || action.kind == APP_ACTION_HINT ||
      action.kind == APP_ACTION_VERIFY;
  if (play_action &&
      (!state->session_open || state->screen != APP_SCREEN_PLAY))
    return outcome;

  AppActionOutcome terminal = app_check_terminal(game, state, elapsed_s);
  if (terminal.terminal) return terminal;

  if (action.kind == APP_ACTION_UNDO)
    return apply_history_action(game, state, false, elapsed_s);
  if (action.kind == APP_ACTION_REDO)
    return apply_history_action(game, state, true, elapsed_s);

  if (action.kind == APP_ACTION_PLACE || action.kind == APP_ACTION_NOTE ||
      action.kind == APP_ACTION_CLEAR) {
    GameEdit edit;
    outcome.input = apply_cell_action(game, state, action, &edit);
    outcome.changed =
        outcome.input == GAME_INPUT_CORRECT ||
        outcome.input == GAME_INPUT_WRONG ||
        outcome.input == GAME_INPUT_CLEARED ||
        outcome.input == GAME_INPUT_NOTE_ADDED ||
        outcome.input == GAME_INPUT_NOTE_REMOVED;
    outcome.blocked = outcome.input == GAME_INPUT_LOCKED ||
                      outcome.input == GAME_INPUT_STRICT_REJECTED;
    outcome.error = outcome.input == GAME_INPUT_WRONG;
    outcome.no_effect = !outcome.changed && !outcome.blocked;

    if (outcome.error && game_mode_reveals_correctness(state->mode)) {
      ++state->mistakes;
      if (state->mode == MODE_STRIKES) ++state->strikes;
    }

    if (outcome.changed) {
      history_record(state, &edit);
      mark_terminal(game, state, elapsed_s, &outcome);
    }
    return outcome;
  }

  if (action.kind == APP_ACTION_HINT) {
    GameEdit edit;
    outcome.changed =
        game_hint_recorded(game, action.row, action.column, &edit);
    outcome.revealed = outcome.changed;
    outcome.no_effect = !outcome.changed;
    if (outcome.changed) {
      history_record(state, &edit);
      mark_terminal(game, state, elapsed_s, &outcome);
    }
    return outcome;
  }

  if (action.kind == APP_ACTION_VERIFY) {
    outcome.detail = game_wrong_entry_count(game);
    outcome.revealed = true;
    outcome.no_effect = false;
    return outcome;
  }

  return outcome;
}
