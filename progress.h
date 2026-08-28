#ifndef SUDOKURA_PROGRESS_H
#define SUDOKURA_PROGRESS_H

#include "game.h"
#include "session.h"

#include <stdbool.h>

bool game_has_meaningful_progress(const Game *game, int mistakes, int strikes);
bool session_has_meaningful_progress(const SessionState *session);
bool session_can_continue(const SessionState *session);

#endif
