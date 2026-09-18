#ifndef SUDOKURA_APP_H
#define SUDOKURA_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"
#include "i18n.h"

typedef enum {
  APP_SCREEN_HOME = 0,
  APP_SCREEN_PLAY = 1,
  APP_SCREEN_RESULT = 2,
  APP_SCREEN_HELP = 3,
  APP_SCREEN_ABOUT = 4,
  APP_SCREEN_SETTINGS = 5
} AppScreen;

typedef enum {
  APP_RESULT_NONE = 0,
  APP_RESULT_WIN = 1,
  APP_RESULT_LOSE = 2
} AppResult;

enum {
  APP_PAUSE_MANUAL = 1u << 0,
  APP_PAUSE_FOCUS = 1u << 1,
  APP_PAUSE_MODAL = 1u << 2,
  APP_PAUSE_HOME = 1u << 3,
  APP_PAUSE_END = 1u << 4
};

#define APP_PAUSE_USER_VISIBLE (APP_PAUSE_MANUAL | APP_PAUSE_FOCUS)

typedef struct {
  int sel_r, sel_c;
  bool notes_mode, strict_mode, dark_theme;
  Language language;

  int mistakes, strikes, strikes_max;
  uint64_t elapsed_ms, running_since_ms;
  double time_limit_s;
  unsigned pause_reasons;

  bool session_open, has_session, is_daily;
  int daily_year, daily_month, daily_day;

  char toast[96];
  double toast_t0;
  bool toast_on;

  bool generating;
  unsigned generation_attempt;
  unsigned generation_max_attempts;

  AppScreen screen, prev_screen;
  GameMode mode;
  AppResult result;
} AppState;

typedef enum {
  APP_ACTION_PLACE = 0,
  APP_ACTION_NOTE,
  APP_ACTION_CLEAR,
  APP_ACTION_UNDO,
  APP_ACTION_REDO,
  APP_ACTION_VERIFY,
  APP_ACTION_HINT,
  APP_ACTION_RESTART,
  APP_ACTION_CONTINUE,
  APP_ACTION_NAVIGATE
} AppActionKind;

typedef struct {
  AppActionKind kind;
  int row;
  int column;
  int value;
  AppScreen target;
} AppAction;

typedef struct {
  bool changed;
  bool error;
  bool terminal;
  bool blocked;
  bool revealed;
  bool no_effect;
  GameInputResult input;
  AppResult result;
  int detail;
} AppActionOutcome;

void app_state_init(AppState *state);
bool app_screen_is_auxiliary(AppScreen screen);
bool app_pause_hides_play(const AppState *state);
bool app_open_aux(AppState *state, AppScreen screen);
bool app_return_aux(AppState *state);
bool app_navigate(AppState *state, AppScreen target);
AppActionOutcome app_check_terminal(Game *game, AppState *state,
                                    double elapsed_s);
AppActionOutcome app_apply_action(Game *game, AppState *state,
                                  AppAction action, double elapsed_s);

#endif
