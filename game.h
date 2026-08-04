#ifndef SUDOKURA_GAME_H
#define SUDOKURA_GAME_H
#include <stdbool.h>
#include <stdint.h>
#define SUDOKU_N 9
#define SUDOKU_CELLS 81
typedef struct { int puzzle[81], solution[81]; unsigned char fixed[81]; uint16_t notes[81]; } Game;
typedef enum { MODE_CLASSIC=0, MODE_STRIKES=1, MODE_TIME=2 } GameMode;
void game_new(Game *game, unsigned seed);
bool game_board_valid(const int board[81]);
int game_solution_count(const int puzzle[81], int limit);
int game_clue_count(const Game *game);
bool game_place(Game *game,int row,int col,int value,bool strict);
bool game_toggle_note(Game *game,int row,int col,int value);
bool game_hint(Game *game,int row,int col);
bool game_has_conflict(const Game *game,int row,int col);
int game_conflict_count(const Game *game);
bool game_is_solved(const Game *game);
bool game_mode_lost(GameMode mode,int strikes,int strikes_max,double elapsed,double limit);
#endif
