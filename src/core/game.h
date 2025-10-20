#ifndef SUDOKURA_CORE_GAME_H
#define SUDOKURA_CORE_GAME_H

#include <stdbool.h>
#include <stdint.h>

#define SUDOKU_SIZE 9
#define SUDOKU_CELLS (SUDOKU_SIZE * SUDOKU_SIZE)
#define GAME_IDX(r, c) ((r) * SUDOKU_SIZE + (c))

typedef struct {
  int puzzle[SUDOKU_CELLS];
  int solution[SUDOKU_CELLS];
  unsigned char fixed[SUDOKU_CELLS];
  uint16_t notes[SUDOKU_CELLS];
} Game;

void game_new(Game *game, unsigned seed);
bool game_place(Game *game, int row, int col, int value, bool strict);
bool game_give_hint(Game *game, int row, int col);
bool game_is_solved(const Game *game);
bool game_has_conflict(const Game *game, int row, int col, int value);
int game_count_conflicts(const Game *game);

#endif /* SUDOKURA_CORE_GAME_H */
