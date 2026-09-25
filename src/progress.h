#ifndef SUDOKURA_PROGRESS_H
#define SUDOKURA_PROGRESS_H

#include "game.h"
#include "session.h"

#include <stdbool.h>

bool game_has_meaningful_progress(const Game *game, int mistakes, int strikes);
bool session_has_meaningful_progress(const SessionState *session);
bool session_can_continue(const SessionState *session);

/* Runtime variants are only for in-memory sessions that already crossed a
   canonical load/generation validation boundary. They still fail closed on
   structurally invalid runtime state, but deliberately avoid regenerating the
   canonical puzzle on every UI interaction. */
bool session_has_meaningful_progress_runtime(const SessionState *session);
bool session_can_continue_runtime(const SessionState *session);

#endif
