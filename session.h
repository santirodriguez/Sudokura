#ifndef SUDOKURA_SESSION_H
#define SUDOKURA_SESSION_H

#include "game.h"
#include "i18n.h"

#include <stdbool.h>
#include <stdint.h>

#define SUDOKURA_SAVE_FORMAT_VERSION 1u

typedef enum {
  STORE_OK = 0,
  STORE_NOT_FOUND,
  STORE_CORRUPT,
  STORE_INCOMPATIBLE,
  STORE_IO_ERROR
} StoreStatus;

typedef enum {
  SESSION_ACTIVE = 0,
  SESSION_WON,
  SESSION_LOST
} SessionStatus;

typedef struct {
  Game game;
  GameMode mode;
  int selected_row;
  int selected_column;
  bool notes_mode;
  bool strict_mode;
  bool manual_paused;
  SessionStatus status;
  int mistakes;
  int strikes;
  uint64_t elapsed_ms;
  bool is_daily;
  int daily_year;
  int daily_month;
  int daily_day;
} SessionState;

typedef struct {
  Language language;
  bool dark_theme;
  bool strict_mode;
  GameMode mode;
  GameDifficulty difficulty;
} Preferences;

void preferences_defaults(Preferences *preferences);
bool preferences_validate(const Preferences *preferences);
bool session_validate(const SessionState *session);

bool preferences_save_file(const char *path, const Preferences *preferences);
StoreStatus preferences_load_file(const char *path, Preferences *preferences);
bool session_save_file(const char *path, const SessionState *session);
StoreStatus session_load_file(const char *path, SessionState *session);

bool store_quarantine_corrupt(const char *path);

#endif
