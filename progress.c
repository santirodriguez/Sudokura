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

static bool validated_session_has_meaningful_progress(
    const SessionState *session) {
  return session && session->status == SESSION_ACTIVE &&
         game_has_meaningful_progress(&session->game, session->mistakes,
                                      session->strikes);
}

bool session_has_meaningful_progress(const SessionState *session) {
  if (!session || !session_validate(session)) return false;
  return validated_session_has_meaningful_progress(session);
}

bool session_has_meaningful_progress_runtime(const SessionState *session) {
  if (!session || !session_validate_runtime(session)) return false;
  return validated_session_has_meaningful_progress(session);
}

bool session_can_continue(const SessionState *session) {
  return session && session_validate(session) &&
         session->status == SESSION_ACTIVE;
}

bool session_can_continue_runtime(const SessionState *session) {
  return session && session_validate_runtime(session) &&
         session->status == SESSION_ACTIVE;
}
