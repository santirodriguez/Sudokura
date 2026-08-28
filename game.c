#include "game.h"

#include <string.h>

#define IDX(r, c) ((r) * 9 + (c))

typedef struct {
  uint64_t state;
} GameRng;

typedef struct {
  int decisions;
  int backtracks;
  int max_depth;
} SolveMetrics;

static bool valid_cell(int row, int column) {
  return row >= 0 && row < 9 && column >= 0 && column < 9;
}

static bool valid_difficulty(GameDifficulty difficulty) {
  return difficulty >= DIFFICULTY_EASY && difficulty < DIFFICULTY_COUNT;
}

static bool allowed(const int *board, int row, int column, int value) {
  for (int i = 0; i < 9; ++i) {
    if ((i != column && board[IDX(row, i)] == value) ||
        (i != row && board[IDX(i, column)] == value)) {
      return false;
    }
  }

  int box_row = row - row % 3;
  int box_column = column - column % 3;
  for (int y = box_row; y < box_row + 3; ++y) {
    for (int x = box_column; x < box_column + 3; ++x) {
      if ((y != row || x != column) && board[IDX(y, x)] == value) {
        return false;
      }
    }
  }
  return true;
}

static uint64_t mix64(uint64_t value) {
  value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31);
}

static uint64_t rng_next(GameRng *rng) {
  rng->state += UINT64_C(0x9e3779b97f4a7c15);
  return mix64(rng->state);
}

static uint32_t rng_bounded(GameRng *rng, uint32_t bound) {
  if (bound < 2) {
    return 0;
  }
  uint64_t threshold = (uint64_t)(0 - (uint64_t)bound) % bound;
  for (;;) {
    uint64_t value = rng_next(rng);
    if (value >= threshold) {
      return (uint32_t)(value % bound);
    }
  }
}

static void shuffle(GameRng *rng, int *values, int count) {
  for (int i = count - 1; i > 0; --i) {
    int j = (int)rng_bounded(rng, (uint32_t)(i + 1));
    int temporary = values[i];
    values[i] = values[j];
    values[j] = temporary;
  }
}

bool game_board_valid(const int board[81]) {
  if (!board) {
    return false;
  }
  for (int i = 0; i < 81; ++i) {
    if (board[i] < 1 || board[i] > 9 ||
        !allowed(board, i / 9, i % 9, board[i])) {
      return false;
    }
  }
  return true;
}

static int mrv(const int *board, int *candidate_count) {
  int best = -1;
  int minimum = 10;
  for (int i = 0; i < 81; ++i) {
    if (board[i]) {
      continue;
    }
    int count = 0;
    for (int value = 1; value <= 9; ++value) {
      count += allowed(board, i / 9, i % 9, value);
    }
    if (count < minimum) {
      best = i;
      minimum = count;
      if (count == 0) {
        break;
      }
    }
  }
  if (candidate_count) {
    *candidate_count = best < 0 ? 0 : minimum;
  }
  return best;
}

static int solve_count(int *board, int limit) {
  int candidates = 0;
  int index = mrv(board, &candidates);
  if (index < 0) {
    return 1;
  }
  if (candidates == 0) {
    return 0;
  }

  int count = 0;
  for (int value = 1; value <= 9 && count < limit; ++value) {
    if (!allowed(board, index / 9, index % 9, value)) {
      continue;
    }
    board[index] = value;
    count += solve_count(board, limit - count);
    board[index] = 0;
  }
  return count;
}

int game_solution_count(const int puzzle[81], int limit) {
  if (!puzzle || limit <= 0) {
    return 0;
  }

  int board[81];
  for (int i = 0; i < 81; ++i) {
    if (puzzle[i] < 0 || puzzle[i] > 9) {
      return 0;
    }
    if (puzzle[i] && !allowed(puzzle, i / 9, i % 9, puzzle[i])) {
      return 0;
    }
  }
  memcpy(board, puzzle, sizeof(board));
  return solve_count(board, limit);
}

static bool solve_metrics(int *board, int depth, SolveMetrics *metrics) {
  int candidates = 0;
  int index = mrv(board, &candidates);
  if (index < 0) {
    return true;
  }
  if (candidates == 0) {
    return false;
  }

  int next_depth = depth;
  if (candidates > 1) {
    ++metrics->decisions;
    ++next_depth;
    if (next_depth > metrics->max_depth) {
      metrics->max_depth = next_depth;
    }
  }

  for (int value = 1; value <= 9; ++value) {
    if (!allowed(board, index / 9, index % 9, value)) {
      continue;
    }
    board[index] = value;
    if (solve_metrics(board, next_depth, metrics)) {
      return true;
    }
    board[index] = 0;
    ++metrics->backtracks;
  }
  return false;
}

int game_difficulty_score(const int puzzle[81]) {
  if (!puzzle || game_solution_count(puzzle, 2) != 1) {
    return -1;
  }

  int board[81];
  int clues = 0;
  memcpy(board, puzzle, sizeof(board));
  for (int i = 0; i < 81; ++i) {
    clues += puzzle[i] != 0;
  }

  SolveMetrics metrics = {0};
  if (!solve_metrics(board, 0, &metrics)) {
    return -1;
  }
  int complexity = metrics.decisions * 10 + metrics.backtracks * 5 +
                   metrics.max_depth * 5;
  if (complexity > 150) {
    complexity = 150;
  }
  return (81 - clues) * 100 + complexity;
}

static void axis_order(GameRng *rng, int output[9]) {
  int groups[3] = {0, 1, 2};
  shuffle(rng, groups, 3);
  int offset = 0;
  for (int group = 0; group < 3; ++group) {
    int within[3] = {0, 1, 2};
    shuffle(rng, within, 3);
    for (int i = 0; i < 3; ++i) {
      output[offset++] = groups[group] * 3 + within[i];
    }
  }
}

static void make_solution(GameRng *rng, int solution[81]) {
  int rows[9];
  int columns[9];
  int numbers[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  axis_order(rng, rows);
  axis_order(rng, columns);
  shuffle(rng, numbers, 9);
  bool transpose = (rng_next(rng) & 1u) != 0;

  for (int row = 0; row < 9; ++row) {
    for (int column = 0; column < 9; ++column) {
      int source_row = transpose ? columns[column] : rows[row];
      int source_column = transpose ? rows[row] : columns[column];
      solution[IDX(row, column)] =
          numbers[(source_row * 3 + source_row / 3 + source_column) % 9];
    }
  }
}

static int target_clues(GameDifficulty difficulty) {
  static const int targets[DIFFICULTY_COUNT] = {44, 36, 30};
  return targets[difficulty];
}

static bool difficulty_clues_ok(GameDifficulty difficulty, int clues) {
  if (difficulty == DIFFICULTY_EASY) {
    return clues >= 42 && clues <= 46;
  }
  if (difficulty == DIFFICULTY_MEDIUM) {
    return clues >= 34 && clues <= 38;
  }
  return clues >= 28 && clues <= 32;
}

static bool generate_candidate(Game *game, uint64_t internal_seed,
                               GameDifficulty difficulty) {
  GameRng rng = {internal_seed ^
                 ((uint64_t)SUDOKURA_GENERATOR_REVISION << 56) ^
                 ((uint64_t)difficulty << 48)};
  make_solution(&rng, game->solution);
  memcpy(game->puzzle, game->solution, sizeof(game->puzzle));

  int positions[81];
  for (int i = 0; i < 81; ++i) {
    positions[i] = i;
  }
  shuffle(&rng, positions, 81);

  int clues = 81;
  int target = target_clues(difficulty);
  for (int i = 0; i < 81 && clues > target; ++i) {
    int index = positions[i];
    int previous = game->puzzle[index];
    game->puzzle[index] = 0;
    if (game_solution_count(game->puzzle, 2) == 1) {
      --clues;
    } else {
      game->puzzle[index] = previous;
    }
  }
  return difficulty_clues_ok(difficulty, clues);
}

void game_new_difficulty(Game *game, uint64_t seed,
                         GameDifficulty difficulty) {
  if (!game) {
    return;
  }
  if (!valid_difficulty(difficulty)) {
    difficulty = DIFFICULTY_MEDIUM;
  }

  memset(game, 0, sizeof(*game));
  game->seed = seed;
  game->generator_revision = SUDOKURA_GENERATOR_REVISION;
  game->difficulty = difficulty;

  bool generated = false;
  for (unsigned attempt = 0; attempt < 64 && !generated; ++attempt) {
    uint64_t attempt_seed =
        seed + UINT64_C(0x9e3779b97f4a7c15) * (uint64_t)attempt;
    generated = generate_candidate(game, attempt_seed, difficulty);
  }

  if (!generated) {
    GameRng rng = {seed};
    make_solution(&rng, game->solution);
    memcpy(game->puzzle, game->solution, sizeof(game->puzzle));
  }

  memcpy(game->initial, game->puzzle, sizeof(game->initial));
  for (int i = 0; i < 81; ++i) {
    game->fixed[i] = (unsigned char)(game->initial[i] != 0);
    game->hinted[i] = 0;
    game->notes[i] = 0;
  }
  game->difficulty_score = game_difficulty_score(game->initial);
}

void game_new(Game *game, uint64_t seed) {
  game_new_difficulty(game, seed, DIFFICULTY_MEDIUM);
}

static bool leap_year(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static bool valid_date(int year, int month, int day) {
  static const int month_days[] = {31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
  if (year < 1 || year > 9999 || month < 1 || month > 12) {
    return false;
  }
  int maximum = month_days[month - 1] + (month == 2 && leap_year(year));
  return day >= 1 && day <= maximum;
}

bool game_daily_seed(int year, int month, int day, uint64_t *seed_out) {
  if (!seed_out || !valid_date(year, month, day)) {
    return false;
  }
  uint64_t date =
      (uint64_t)year * 10000u + (uint64_t)month * 100u + (uint64_t)day;
  *seed_out =
      mix64(date ^ UINT64_C(0x5355444f4b555241) ^
            ((uint64_t)SUDOKURA_GENERATOR_REVISION << 32));
  return true;
}

bool game_new_daily(Game *game, int year, int month, int day) {
  uint64_t seed = 0;
  if (!game || !game_daily_seed(year, month, day, &seed)) {
    return false;
  }
  game_new_difficulty(game, seed, DIFFICULTY_MEDIUM);
  return true;
}

void game_restart(Game *game) {
  if (!game) {
    return;
  }
  memcpy(game->puzzle, game->initial, sizeof(game->puzzle));
  for (int i = 0; i < 81; ++i) {
    game->hinted[i] = 0;
    game->notes[i] = 0;
    game->fixed[i] = (unsigned char)(game->initial[i] != 0);
  }
}

int game_clue_count(const Game *game) {
  if (!game) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < 81; ++i) {
    count += game->initial[i] != 0;
  }
  return count;
}

int game_progress_percent(const Game *game) {
  if (!game) {
    return 0;
  }
  int total = 0;
  int correct = 0;
  for (int i = 0; i < 81; ++i) {
    if (game->initial[i] == 0) {
      ++total;
      correct += game->puzzle[i] == game->solution[i];
    }
  }
  return total ? correct * 100 / total : 100;
}

bool game_cell_locked(const Game *game, int row, int column) {
  return game && valid_cell(row, column) &&
         (game->fixed[IDX(row, column)] || game->hinted[IDX(row, column)]);
}

bool game_place(Game *game, int row, int column, int value, bool strict) {
  if (!game || !valid_cell(row, column) || value < 0 || value > 9) {
    return false;
  }
  int index = IDX(row, column);
  if (game_cell_locked(game, row, column)) {
    return false;
  }
  if (value && strict && !allowed(game->puzzle, row, column, value)) {
    return false;
  }
  game->puzzle[index] = value;
  game->notes[index] = 0;
  return true;
}

bool game_toggle_note(Game *game, int row, int column, int value) {
  if (!game || !valid_cell(row, column) || value < 1 || value > 9) {
    return false;
  }
  int index = IDX(row, column);
  if (game_cell_locked(game, row, column) || game->puzzle[index]) {
    return false;
  }
  game->notes[index] ^= (uint16_t)(1u << value);
  return true;
}

GameInputResult game_apply_input(Game *game, int row, int column, int value,
                                 bool notes_mode, bool strict) {
  if (!game || !valid_cell(row, column) || value < 0 || value > 9) {
    return GAME_INPUT_NO_CHANGE;
  }
  int index = IDX(row, column);
  if (game_cell_locked(game, row, column)) {
    return GAME_INPUT_LOCKED;
  }

  if (notes_mode) {
    if (value < 1 || value > 9 || game->puzzle[index]) {
      return GAME_INPUT_NO_CHANGE;
    }
    bool had_note = (game->notes[index] & (1u << value)) != 0;
    if (!game_toggle_note(game, row, column, value)) {
      return GAME_INPUT_NO_CHANGE;
    }
    return had_note ? GAME_INPUT_NOTE_REMOVED : GAME_INPUT_NOTE_ADDED;
  }

  if (value == 0) {
    if (game->puzzle[index] == 0 && game->notes[index] == 0) {
      return GAME_INPUT_NO_CHANGE;
    }
    return game_place(game, row, column, 0, false) ? GAME_INPUT_CLEARED
                                                   : GAME_INPUT_NO_CHANGE;
  }

  if (strict && !allowed(game->puzzle, row, column, value)) {
    return GAME_INPUT_STRICT_REJECTED;
  }
  if (!game_place(game, row, column, value, false)) {
    return GAME_INPUT_NO_CHANGE;
  }
  return value == game->solution[index] ? GAME_INPUT_CORRECT : GAME_INPUT_WRONG;
}

bool game_hint(Game *game, int row, int column) {
  if (!game || !valid_cell(row, column)) {
    return false;
  }
  int index = IDX(row, column);
  if (game_cell_locked(game, row, column) ||
      game->puzzle[index] == game->solution[index]) {
    return false;
  }
  game->puzzle[index] = game->solution[index];
  game->notes[index] = 0;
  game->hinted[index] = 1;
  return true;
}

bool game_has_conflict(const Game *game, int row, int column) {
  if (!game || !valid_cell(row, column)) {
    return false;
  }
  int value = game->puzzle[IDX(row, column)];
  return value >= 1 && value <= 9 &&
         !allowed(game->puzzle, row, column, value);
}

int game_conflict_count(const Game *game) {
  if (!game) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < 81; ++i) {
    count += game_has_conflict(game, i / 9, i % 9);
  }
  return count;
}

bool game_is_solved(const Game *game) {
  return game && game_board_valid(game->puzzle) &&
         memcmp(game->puzzle, game->solution, sizeof(game->puzzle)) == 0;
}

bool game_mode_lost(GameMode mode, int strikes, int strikes_max,
                    double elapsed, double limit) {
  return (mode == MODE_STRIKES && strikes >= strikes_max) ||
         (mode == MODE_TIME && limit > 0 && elapsed > limit);
}
