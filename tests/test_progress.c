#include "progress.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SessionState fresh_state(void) {
  SessionState state;
  memset(&state, 0, sizeof(state));
  game_new_difficulty(&state.game, UINT64_C(0x123456789abcdef0),
                      DIFFICULTY_MEDIUM);
  state.mode = MODE_CLASSIC;
  state.selected_row = 4;
  state.selected_column = 4;
  state.status = SESSION_ACTIVE;
  assert(session_validate(&state));
  return state;
}

static int first_playable(const Game *game) {
  for (int i = 0; i < 81; ++i)
    if (!game->fixed[i]) return i;
  return -1;
}

int main(void) {
  SessionState state = fresh_state();
  state.elapsed_ms = UINT64_C(987654);
  assert(session_can_continue(&state));
  assert(!session_has_meaningful_progress(&state));
  assert(!game_has_meaningful_progress(&state.game, 0, 0));

  int cell = first_playable(&state.game);
  assert(cell >= 0);
  assert(game_apply_input(&state.game, cell / 9, cell % 9, 3, true, false) ==
         GAME_INPUT_NOTE_ADDED);
  assert(session_has_meaningful_progress(&state));

  state = fresh_state();
  cell = first_playable(&state.game);
  assert(game_apply_input(&state.game, cell / 9, cell % 9,
                          state.game.solution[cell], false, false) ==
         GAME_INPUT_CORRECT);
  assert(session_has_meaningful_progress(&state));

  state = fresh_state();
  cell = first_playable(&state.game);
  assert(game_hint(&state.game, cell / 9, cell % 9));
  assert(session_has_meaningful_progress(&state));

  state = fresh_state();
  assert(game_has_meaningful_progress(&state.game, 1, 0));
  assert(game_has_meaningful_progress(&state.game, 0, 1));

  state = fresh_state();
  for (int i = 0; i < 81; ++i)
    if (!state.game.fixed[i]) state.game.puzzle[i] = state.game.solution[i];
  state.status = SESSION_WON;
  assert(session_validate(&state));
  assert(!session_can_continue(&state));
  assert(!session_has_meaningful_progress(&state));

  puts("meaningful-progress tests passed for pristine, edited, hinted and completed sessions");
  return 0;
}
