#ifndef SUDOKURA_SESSION_H
#define SUDOKURA_SESSION_H

#include "game.h"
#include "i18n.h"
#include "store_status.h"

#include <stdbool.h>
#include <stdint.h>

#define SUDOKURA_SAVE_FORMAT_VERSION 1u
#define SUDOKURA_SESSION_MAX_ELAPSED_MS UINT64_C(31536000000)
#define SUDOKURA_DEFAULT_WINDOW_WIDTH 1024
#define SUDOKURA_DEFAULT_WINDOW_HEIGHT 720

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
  bool audio_enabled;
  uint8_t music_volume;
  uint8_t fx_volume;
  bool reduced_motion;
  bool auto_remove_peer_notes;
  int32_t window_x;
  int32_t window_y;
  uint32_t window_width;
  uint32_t window_height;
  bool window_maximized;
} Preferences;

void preferences_defaults(Preferences *preferences);
bool preferences_validate(const Preferences *preferences);
bool session_validate(const SessionState *session);
bool session_validate_runtime(const SessionState *session);

bool preferences_save_file(const char *path, const Preferences *preferences);
StoreStatus preferences_load_file(const char *path, Preferences *preferences);
bool session_save_file(const char *path, const SessionState *session);
StoreStatus session_load_file(const char *path, SessionState *session);

bool store_quarantine_corrupt(const char *path);

#endif
