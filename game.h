#ifndef SUDOKURA_GAME_H
#define SUDOKURA_GAME_H
#include <stdbool.h>
#include <stdint.h>
#define SUDOKU_N 9
#define SUDOKU_CELLS 81
#define SUDOKURA_GENERATOR_REVISION 1u

typedef enum { MODE_CLASSIC=0, MODE_STRIKES=1, MODE_TIME=2 } GameMode;
typedef enum { DIFFICULTY_EASY=0, DIFFICULTY_MEDIUM=1, DIFFICULTY_HARD=2, DIFFICULTY_COUNT=3 } GameDifficulty;
typedef enum {
  GAME_INPUT_NO_CHANGE=0,
  GAME_INPUT_CORRECT,
  GAME_INPUT_WRONG,
  GAME_INPUT_STRICT_REJECTED,
  GAME_INPUT_CLEARED,
  GAME_INPUT_NOTE_ADDED,
  GAME_INPUT_NOTE_REMOVED,
  GAME_INPUT_LOCKED
} GameInputResult;

typedef struct {
  int puzzle[81], solution[81], initial[81];
  unsigned char fixed[81], hinted[81];
  uint16_t notes[81];
  uint64_t seed;
  uint32_t generator_revision;
  GameDifficulty difficulty;
  int difficulty_score;
} Game;

void game_new(Game *game, uint64_t seed);
void game_new_difficulty(Game *game, uint64_t seed, GameDifficulty difficulty);
bool game_new_daily(Game *game, int year, int month, int day);
bool game_daily_seed(int year, int month, int day, uint64_t *seed_out);
void game_restart(Game *game);
bool game_board_valid(const int board[81]);
int game_solution_count(const int puzzle[81], int limit);
int game_clue_count(const Game *game);
int game_difficulty_score(const int puzzle[81]);
int game_progress_percent(const Game *game);
bool game_cell_locked(const Game *game,int row,int col);
GameInputResult game_apply_input(Game *game,int row,int col,int value,bool notes_mode,bool strict);
bool game_place(Game *game,int row,int col,int value,bool strict);
bool game_toggle_note(Game *game,int row,int col,int value);
bool game_hint(Game *game,int row,int col);
bool game_has_conflict(const Game *game,int row,int col);
int game_conflict_count(const Game *game);
bool game_is_solved(const Game *game);
bool game_mode_lost(GameMode mode,int strikes,int strikes_max,double elapsed,double limit);
#endif
