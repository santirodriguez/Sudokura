#include "core/game.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NN SUDOKU_CELLS

static inline bool row_has(const int *board, int row, int value) {
  for (int c = 0; c < SUDOKU_SIZE; ++c) {
    if (board[GAME_IDX(row, c)] == value) {
      return true;
    }
  }
  return false;
}

static inline bool col_has(const int *board, int col, int value) {
  for (int r = 0; r < SUDOKU_SIZE; ++r) {
    if (board[GAME_IDX(r, col)] == value) {
      return true;
    }
  }
  return false;
}

static inline bool box_has(const int *board, int row, int col, int value) {
  int br = (row / 3) * 3;
  int bc = (col / 3) * 3;
  for (int rr = 0; rr < 3; ++rr) {
    for (int cc = 0; cc < 3; ++cc) {
      if (board[GAME_IDX(br + rr, bc + cc)] == value) {
        return true;
      }
    }
  }
  return false;
}

static inline bool can_place_local(const int *board, int row, int col, int value) {
  return !row_has(board, row, value) && !col_has(board, col, value) &&
         !box_has(board, row, col, value);
}

static void shuffle(int *a, int n) {
  for (int i = n - 1; i > 0; --i) {
    int j = rand() % (i + 1);
    int t = a[i];
    a[i] = a[j];
    a[j] = t;
  }
}

static int find_mrv(const int *board) {
  int best = -1;
  int best_count = 10;
  for (int i = 0; i < NN; ++i) {
    if (board[i]) {
      continue;
    }
    int row = i / SUDOKU_SIZE;
    int col = i % SUDOKU_SIZE;
    int count = 0;
    for (int value = 1; value <= SUDOKU_SIZE; ++value) {
      if (can_place_local(board, row, col, value)) {
        ++count;
      }
    }
    if (count < best_count) {
      best_count = count;
      best = i;
      if (count == 1) {
        break;
      }
    }
  }
  return best;
}

static bool rec_first(int *board, int *out) {
  int idx = find_mrv(board);
  if (idx < 0) {
    memcpy(out, board, NN * sizeof(int));
    return true;
  }
  int row = idx / SUDOKU_SIZE;
  int col = idx % SUDOKU_SIZE;
  int options[9];
  int count = 0;
  for (int value = 1; value <= SUDOKU_SIZE; ++value) {
    if (can_place_local(board, row, col, value)) {
      options[count++] = value;
    }
  }
  for (int k = 0; k < count; ++k) {
    board[idx] = options[k];
    if (rec_first(board, out)) {
      return true;
    }
    board[idx] = 0;
  }
  return false;
}

static int count_limit(int *board, int limit) {
  int idx = find_mrv(board);
  if (idx < 0) {
    return 1;
  }
  int row = idx / SUDOKU_SIZE;
  int col = idx % SUDOKU_SIZE;
  int options[9];
  int count = 0;
  for (int value = 1; value <= SUDOKU_SIZE; ++value) {
    if (can_place_local(board, row, col, value)) {
      options[count++] = value;
    }
  }
  int total = 0;
  for (int k = 0; k < count; ++k) {
    board[idx] = options[k];
    int got = count_limit(board, limit - total);
    total += got;
    if (total >= limit) {
      board[idx] = 0;
      return total;
    }
    board[idx] = 0;
  }
  return total;
}

static bool unique_solution(const int *puzzle, int *out_solution) {
  int tmp[NN];
  memcpy(tmp, puzzle, sizeof(tmp));
  if (!rec_first(tmp, out_solution)) {
    return false;
  }
  memcpy(tmp, puzzle, sizeof(tmp));
  return count_limit(tmp, 2) == 1;
}

static void make_solved(int *out) {
  int rows[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  int cols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  int nums[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  int band[3] = {0, 1, 2};
  shuffle(band, 3);
  int rin[3][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
  int p = 0;
  for (int b0 = 0; b0 < 3; ++b0) {
    int bi = band[b0];
    shuffle(rin[bi], 3);
    for (int i = 0; i < 3; ++i) {
      rows[p++] = rin[bi][i];
    }
  }

  int stack_[3] = {0, 1, 2};
  shuffle(stack_, 3);
  int cin[3][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
  p = 0;
  for (int s0 = 0; s0 < 3; ++s0) {
    int si = stack_[s0];
    shuffle(cin[si], 3);
    for (int i = 0; i < 3; ++i) {
      cols[p++] = cin[si][i];
    }
  }

  shuffle(nums, 9);
  for (int r = 0; r < SUDOKU_SIZE; ++r) {
    for (int c = 0; c < SUDOKU_SIZE; ++c) {
      int r2 = rows[r];
      int c2 = cols[c];
      int base = (r2 * 3 + r2 / 3 + c2) % 9;
      out[GAME_IDX(r, c)] = nums[base];
    }
  }
}

static void remove_to_medium(int *grid) {
  const int min_clues = 32;
  const int max_clues = 38;
  int pos[NN];
  for (int i = 0; i < NN; ++i) {
    pos[i] = i;
  }
  shuffle(pos, NN);
  int clues = NN;
  for (int k = 0; k < NN; ++k) {
    int i = pos[k];
    int r = i / SUDOKU_SIZE;
    int c = i % SUDOKU_SIZE;
    int j = GAME_IDX(SUDOKU_SIZE - 1 - r, SUDOKU_SIZE - 1 - c);
    if (grid[i] == 0 && grid[j] == 0) {
      continue;
    }
    int bi = grid[i];
    int bj = grid[j];
    grid[i] = 0;
    if (j != i) {
      grid[j] = 0;
    }
    int tmp[NN];
    memcpy(tmp, grid, sizeof(tmp));
    int solution[NN];
    bool ok = unique_solution(tmp, solution);
    int delta = (j == i) ? 1 : 2;
    if (!ok || (clues - delta) < min_clues) {
      grid[i] = bi;
      if (j != i) {
        grid[j] = bj;
      }
    } else {
      clues -= delta;
      if (clues <= max_clues) {
        if (rand() % 3 == 0) {
          break;
        }
      }
    }
  }
}

void game_new(Game *game, unsigned seed) {
  if (seed) {
    srand(seed);
  } else {
    srand((unsigned)time(NULL));
  }
  int solved[NN];
  make_solved(solved);
  int puzzle[NN];
  memcpy(puzzle, solved, sizeof(puzzle));
  remove_to_medium(puzzle);
  int final_solution[NN];
  if (!unique_solution(puzzle, final_solution)) {
    int safe[NN] = {
        5, 3, 0, 0, 7, 0, 0, 0, 0,
        6, 0, 0, 1, 9, 5, 0, 0, 0,
        0, 9, 8, 0, 0, 0, 0, 6, 0,
        8, 0, 0, 0, 6, 0, 0, 0, 3,
        4, 0, 0, 8, 0, 3, 0, 0, 1,
        7, 0, 0, 0, 2, 0, 0, 0, 6,
        0, 6, 0, 0, 0, 0, 2, 8, 0,
        0, 0, 0, 4, 1, 9, 0, 0, 5,
        0, 0, 0, 0, 8, 0, 0, 7, 9};
    memcpy(puzzle, safe, sizeof(safe));
    unique_solution(puzzle, final_solution);
  }
  for (int i = 0; i < NN; ++i) {
    game->puzzle[i] = puzzle[i];
    game->solution[i] = final_solution[i];
    game->fixed[i] = (unsigned char)(puzzle[i] != 0);
    game->notes[i] = 0;
  }
}

bool game_is_solved(const Game *game) {
  for (int i = 0; i < NN; ++i) {
    if (game->puzzle[i] == 0) {
      return false;
    }
    if (game->puzzle[i] != game->solution[i]) {
      return false;
    }
  }
  return true;
}

bool game_place(Game *game, int row, int col, int value, bool strict) {
  int idx = GAME_IDX(row, col);
  if (game->fixed[idx]) {
    return false;
  }
  if (value == 0) {
    game->puzzle[idx] = 0;
    game->notes[idx] = 0;
    return true;
  }
  if (strict && !can_place_local(game->puzzle, row, col, value)) {
    return false;
  }
  game->puzzle[idx] = value;
  game->notes[idx] = 0;
  return true;
}

bool game_give_hint(Game *game, int row, int col) {
  int idx = GAME_IDX(row, col);
  if (game->fixed[idx]) {
    return false;
  }
  int correct = game->solution[idx];
  if (game->puzzle[idx] == correct) {
    return false;
  }
  game->puzzle[idx] = correct;
  game->notes[idx] = 0;
  return true;
}

bool game_has_conflict(const Game *game, int row, int col, int value) {
  if (value == 0) {
    return false;
  }
  for (int c = 0; c < SUDOKU_SIZE; ++c) {
    if (c != col && game->puzzle[GAME_IDX(row, c)] == value) {
      return true;
    }
  }
  for (int r = 0; r < SUDOKU_SIZE; ++r) {
    if (r != row && game->puzzle[GAME_IDX(r, col)] == value) {
      return true;
    }
  }
  int br = (row / 3) * 3;
  int bc = (col / 3) * 3;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      int R = br + r;
      int C = bc + c;
      if (R == row && C == col) {
        continue;
      }
      if (game->puzzle[GAME_IDX(R, C)] == value) {
        return true;
      }
    }
  }
  return false;
}

int game_count_conflicts(const Game *game) {
  int cnt = 0;
  for (int r = 0; r < SUDOKU_SIZE; ++r) {
    for (int c = 0; c < SUDOKU_SIZE; ++c) {
      int value = game->puzzle[GAME_IDX(r, c)];
      if (value && game_has_conflict(game, r, c, value)) {
        ++cnt;
      }
    }
  }
  return cnt;
}
