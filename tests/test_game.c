#include <stdio.h>
#include <string.h>

#include "core/game.h"

static int check_unit(const int *values) {
  int seen[10] = {0};
  for (int i = 0; i < 9; ++i) {
    int v = values[i];
    if (v < 1 || v > 9) {
      return 0;
    }
    seen[v] = 1;
  }
  for (int v = 1; v <= 9; ++v) {
    if (!seen[v]) {
      return 0;
    }
  }
  return 1;
}

static int test_solution_valid(void) {
  Game game;
  game_new(&game, 1234);

  int row[9];
  for (int r = 0; r < 9; ++r) {
    for (int c = 0; c < 9; ++c) {
      row[c] = game.solution[GAME_IDX(r, c)];
    }
    if (!check_unit(row)) {
      return 0;
    }
  }

  int col[9];
  for (int c = 0; c < 9; ++c) {
    for (int r = 0; r < 9; ++r) {
      col[r] = game.solution[GAME_IDX(r, c)];
    }
    if (!check_unit(col)) {
      return 0;
    }
  }

  int box[9];
  for (int br = 0; br < 3; ++br) {
    for (int bc = 0; bc < 3; ++bc) {
      int idx = 0;
      for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
          int rr = br * 3 + r;
          int cc = bc * 3 + c;
          box[idx++] = game.solution[GAME_IDX(rr, cc)];
        }
      }
      if (!check_unit(box)) {
        return 0;
      }
    }
  }

  for (int i = 0; i < SUDOKU_CELLS; ++i) {
    if (game.puzzle[i] != 0 && game.puzzle[i] != game.solution[i]) {
      return 0;
    }
    if (game.fixed[i] && game.puzzle[i] == 0) {
      return 0;
    }
    if (!game.fixed[i] && game.puzzle[i] != 0) {
      return 0;
    }
  }
  return 1;
}

static int test_place_and_hint(void) {
  Game game;
  game_new(&game, 4321);

  int row = -1;
  int col = -1;
  for (int r = 0; r < 9 && row < 0; ++r) {
    for (int c = 0; c < 9; ++c) {
      if (!game.fixed[GAME_IDX(r, c)]) {
        row = r;
        col = c;
        break;
      }
    }
  }
  if (row < 0) {
    return 0;
  }
  int idx = GAME_IDX(row, col);
  int correct = game.solution[idx];
  int wrong = (correct % 9) + 1;
  if (wrong == correct) {
    wrong = ((correct + 1) % 9) + 1;
  }
  if (game_place(&game, row, col, wrong, true)) {
    return 0; /* strict mode should reject conflicts */
  }
  if (!game_place(&game, row, col, wrong, false)) {
    return 0;
  }
  if (game.puzzle[idx] != wrong) {
    return 0;
  }
  if (!game_give_hint(&game, row, col)) {
    return 0;
  }
  if (game.puzzle[idx] != correct) {
    return 0;
  }
  return 1;
}

int main(void) {
  int passed = 1;
  passed &= test_solution_valid();
  passed &= test_place_and_hint();
  if (passed) {
    printf("All game tests passed.\n");
    return 0;
  }
  printf("Game tests failed.\n");
  return 1;
}
