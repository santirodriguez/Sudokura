#include "progress.h"

bool game_has_meaningful_progress(const Game *game, int mistakes, int strikes) {
  if (!game) return false;
  if (mistakes > 0 || strikes > 0) return true;
  for (int i = 0; i < 81; ++i) {
    if (game->puzzle[i] != game->initial[i] || game->hinted[i] || game->notes[i])
      return true;
  }
  return false;
}

bool session_has_meaningful_progress(const SessionState *session) {
  if (!session || !session_validate(session) || session->status != SESSION_ACTIVE)
    return false;
  return game_has_meaningful_progress(&session->game, session->mistakes,
                                      session->strikes);
}

bool session_can_continue(const SessionState *session) {
  return session && session_validate(session) && session->status == SESSION_ACTIVE;
}
