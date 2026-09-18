#include "game.h"
#include "human.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define QUALITY_CORPUS_PER_DIFFICULTY 1000
#define FULL_MASK UINT16_C(0x03fe)

typedef struct {
  int board[81];
  uint16_t rows[9], columns[9], boxes[9];
  int count;
  int first_solution[81];
} IndependentSolver;

typedef struct {
  const int *solution;
  uint64_t eliminated_candidates;
  uint64_t placements;
} OracleContext;

typedef struct {
  unsigned attempts;
} AttemptCounter;

static int box_index(int row, int column) {
  return (row / 3) * 3 + column / 3;
}

static int bit_count(uint16_t mask) {
  int count = 0;
  while (mask) {
    mask &= (uint16_t)(mask - 1u);
    ++count;
  }
  return count;
}

static bool independent_init(IndependentSolver *solver, const int puzzle[81]) {
  memset(solver, 0, sizeof(*solver));
  for (int cell = 0; cell < 81; ++cell) {
    int value = puzzle[cell];
    if (value < 0 || value > 9) return false;
    solver->board[cell] = value;
    if (!value) continue;
    int row = cell / 9, column = cell % 9, box = box_index(row, column);
    uint16_t bit = (uint16_t)(1u << value);
    if ((solver->rows[row] | solver->columns[column] | solver->boxes[box]) &
        bit)
      return false;
    solver->rows[row] |= bit;
    solver->columns[column] |= bit;
    solver->boxes[box] |= bit;
  }
  return true;
}

static uint16_t independent_candidates(const IndependentSolver *solver,
                                       int cell) {
  int row = cell / 9, column = cell % 9, box = box_index(row, column);
  return (uint16_t)(FULL_MASK &
                    (uint16_t)~(solver->rows[row] |
                                solver->columns[column] |
                                solver->boxes[box]));
}

static void independent_search(IndependentSolver *solver) {
  if (solver->count >= 2) return;
  int best_cell = -1, best_count = 10;
  uint16_t best_mask = 0;
  for (int cell = 0; cell < 81; ++cell) {
    if (solver->board[cell]) continue;
    uint16_t mask = independent_candidates(solver, cell);
    int count = bit_count(mask);
    if (count == 0) return;
    if (count < best_count) {
      best_cell = cell;
      best_count = count;
      best_mask = mask;
      if (count == 1) break;
    }
  }
  if (best_cell < 0) {
    if (solver->count == 0)
      memcpy(solver->first_solution, solver->board,
             sizeof(solver->first_solution));
    ++solver->count;
    return;
  }

  int row = best_cell / 9, column = best_cell % 9;
  int box = box_index(row, column);
  for (int value = 1; value <= 9 && solver->count < 2; ++value) {
    uint16_t bit = (uint16_t)(1u << value);
    if (!(best_mask & bit)) continue;
    solver->board[best_cell] = value;
    solver->rows[row] |= bit;
    solver->columns[column] |= bit;
    solver->boxes[box] |= bit;
    independent_search(solver);
    solver->rows[row] &= (uint16_t)~bit;
    solver->columns[column] &= (uint16_t)~bit;
    solver->boxes[box] &= (uint16_t)~bit;
    solver->board[best_cell] = 0;
  }
}

static int independent_solution_count(const int puzzle[81],
                                      int solution_out[81]) {
  IndependentSolver solver;
  if (!independent_init(&solver, puzzle)) return 0;
  independent_search(&solver);
  if (solution_out && solver.count > 0)
    memcpy(solution_out, solver.first_solution, sizeof(solver.first_solution));
  return solver.count;
}

static bool oracle_step(const HumanStep *step, void *userdata) {
  OracleContext *oracle = (OracleContext *)userdata;
  assert(step && oracle && oracle->solution);
  if (step->placement_cell >= 0) {
    assert(step->placement_cell < 81);
    assert(step->placement_value ==
           oracle->solution[step->placement_cell]);
    ++oracle->placements;
    return true;
  }

  for (int i = 0; i < step->affected_count; ++i) {
    int cell = step->affected_cells[i];
    uint16_t removed = step->affected_masks[i];
    assert(cell >= 0 && cell < 81);
    assert(removed != 0);
    assert((removed & (uint16_t)(1u << oracle->solution[cell])) == 0);
    oracle->eliminated_candidates += (uint64_t)bit_count(removed);
  }
  return true;
}

static bool count_attempt(void *userdata, unsigned attempt,
                          unsigned max_attempts) {
  AttemptCounter *counter = (AttemptCounter *)userdata;
  assert(counter);
  assert(attempt < max_attempts);
  unsigned used = attempt + 1u;
  if (used > counter->attempts) counter->attempts = used;
  return true;
}

static uint64_t corpus_seed(GameDifficulty difficulty, int index) {
  return UINT64_C(0x6a09e667f3bcc909) ^
         ((uint64_t)(difficulty + 1) << 60) ^
         (UINT64_C(0x9e3779b97f4a7c15) * (uint64_t)(index + 1));
}

static uint64_t fnv_byte(uint64_t hash, unsigned char value) {
  hash ^= value;
  return hash * UINT64_C(1099511628211);
}

static uint64_t fnv_u64(uint64_t hash, uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8)
    hash = fnv_byte(hash, (unsigned char)((value >> shift) & UINT64_C(0xff)));
  return hash;
}

static uint64_t hash_game(uint64_t hash, const Game *game,
                          const HumanEvaluation *evaluation) {
  hash = fnv_u64(hash, game->seed);
  hash = fnv_u64(hash, game->generator_revision);
  hash = fnv_u64(hash, (uint64_t)game->difficulty);
  for (int i = 0; i < 81; ++i)
    hash = fnv_byte(hash, (unsigned char)game->initial[i]);
  hash = fnv_u64(hash, (uint64_t)evaluation->rating);
  hash = fnv_u64(hash, (uint64_t)evaluation->max_technique);
  hash = fnv_u64(hash, (uint64_t)evaluation->total_steps);
  return hash;
}

static int compare_double(const void *left, const void *right) {
  double a = *(const double *)left, b = *(const double *)right;
  return a < b ? -1 : a > b ? 1 : 0;
}

static int compare_unsigned(const void *left, const void *right) {
  unsigned a = *(const unsigned *)left, b = *(const unsigned *)right;
  return a < b ? -1 : a > b ? 1 : 0;
}

static double elapsed_ms(clock_t start, clock_t finish) {
  return (double)(finish - start) * 1000.0 / (double)CLOCKS_PER_SEC;
}

static HumanRating expected_rating(GameDifficulty difficulty) {
  if (difficulty == DIFFICULTY_EASY) return HUMAN_RATING_EASY;
  if (difficulty == DIFFICULTY_MEDIUM) return HUMAN_RATING_MEDIUM;
  return HUMAN_RATING_HARD;
}

static void assert_rating_rule(GameDifficulty difficulty,
                               const HumanEvaluation *evaluation) {
  assert(evaluation->rating == expected_rating(difficulty));
  if (difficulty == DIFFICULTY_EASY) {
    assert(evaluation->max_technique <= HUMAN_TECHNIQUE_HIDDEN_SINGLE);
  } else if (difficulty == DIFFICULTY_MEDIUM) {
    assert(evaluation->max_technique == HUMAN_TECHNIQUE_LOCKED_CANDIDATE);
    assert(evaluation->technique_steps[HUMAN_TECHNIQUE_LOCKED_CANDIDATE] > 0);
  } else {
    assert(evaluation->max_technique >= HUMAN_TECHNIQUE_NAKED_PAIR);
    assert(evaluation->technique_steps[HUMAN_TECHNIQUE_NAKED_PAIR] +
               evaluation->technique_steps[HUMAN_TECHNIQUE_NAKED_TRIPLE] +
               evaluation->technique_steps[HUMAN_TECHNIQUE_X_WING] >
           0);
  }
}

static uint64_t run_v3_corpus(GameDifficulty difficulty) {
  double timings[QUALITY_CORPUS_PER_DIFFICULTY];
  unsigned attempts[QUALITY_CORPUS_PER_DIFFICULTY];
  uint64_t hash = UINT64_C(1469598103934665603);
  uint64_t total_eliminations = 0, total_placements = 0;

  for (int index = 0; index < QUALITY_CORPUS_PER_DIFFICULTY; ++index) {
    uint64_t seed = corpus_seed(difficulty, index);
    AttemptCounter counter = {0};
    GameGenerationControl control = {count_attempt, &counter};
    Game game;
    clock_t start = clock();
    GameGenerationResult result = game_generate_difficulty(
        &game, seed, difficulty, SUDOKURA_GENERATOR_REVISION, &control);
    clock_t finish = clock();
    assert(result == GAME_GENERATION_OK);
    assert(counter.attempts >= 1 &&
           counter.attempts <= game_generation_attempt_budget(difficulty));
    timings[index] = elapsed_ms(start, finish);
    attempts[index] = counter.attempts;

    int independent_solution[81];
    assert(independent_solution_count(game.initial, independent_solution) == 1);
    assert(!memcmp(independent_solution, game.solution,
                   sizeof(independent_solution)));

    OracleContext oracle = {independent_solution, 0, 0};
    HumanEvaluation evaluation;
    assert(human_evaluate_trace(game.initial, &evaluation, oracle_step,
                                &oracle));
    assert(evaluation.valid && evaluation.solved && !evaluation.stalled);
    assert_rating_rule(difficulty, &evaluation);
    total_eliminations += oracle.eliminated_candidates;
    total_placements += oracle.placements;
    hash = hash_game(hash, &game, &evaluation);

    Game duplicate;
    assert(game_new_difficulty_revision(
        &duplicate, seed, difficulty, SUDOKURA_GENERATOR_REVISION));
    assert(!memcmp(&game, &duplicate, sizeof(game)));
  }

  qsort(timings, QUALITY_CORPUS_PER_DIFFICULTY, sizeof(timings[0]),
        compare_double);
  qsort(attempts, QUALITY_CORPUS_PER_DIFFICULTY, sizeof(attempts[0]),
        compare_unsigned);
  int p95 = (QUALITY_CORPUS_PER_DIFFICULTY * 95 + 99) / 100 - 1;
  printf("quality-v3 difficulty=%d corpus=%d digest=%016" PRIx64
         " cpu_ms_median=%.3f cpu_ms_p95=%.3f cpu_ms_max=%.3f"
         " attempts_median=%u attempts_p95=%u attempts_max=%u"
         " oracle_eliminations=%" PRIu64 " oracle_placements=%" PRIu64 "\n",
         (int)difficulty, QUALITY_CORPUS_PER_DIFFICULTY, hash,
         timings[QUALITY_CORPUS_PER_DIFFICULTY / 2], timings[p95],
         timings[QUALITY_CORPUS_PER_DIFFICULTY - 1],
         attempts[QUALITY_CORPUS_PER_DIFFICULTY / 2], attempts[p95],
         attempts[QUALITY_CORPUS_PER_DIFFICULTY - 1],
         total_eliminations, total_placements);
  return hash;
}

static uint64_t legacy_digest(void) {
  static const uint64_t seeds[] = {
      UINT64_C(0), UINT64_C(1), UINT64_C(42), UINT64_C(0x7fffffffffffffff),
      UINT64_C(0x8000000000000000), UINT64_MAX,
      UINT64_C(0x123456789abcdef0)};
  uint64_t hash = UINT64_C(1469598103934665603);
  for (unsigned s = 0; s < sizeof(seeds) / sizeof(seeds[0]); ++s) {
    for (int difficulty = DIFFICULTY_EASY; difficulty < DIFFICULTY_COUNT;
         ++difficulty) {
      Game game;
      assert(game_new_difficulty_revision(
          &game, seeds[s], (GameDifficulty)difficulty,
          SUDOKURA_GENERATOR_REVISION_LEGACY));
      hash = fnv_u64(hash, seeds[s]);
      hash = fnv_u64(hash, (uint64_t)difficulty);
      for (int i = 0; i < 81; ++i)
        hash = fnv_byte(hash, (unsigned char)game.initial[i]);
      int initial[81];
      memcpy(initial, game.initial, sizeof(initial));
      int playable = -1;
      for (int i = 0; i < 81; ++i)
        if (!game.fixed[i]) {
          playable = i;
          break;
        }
      assert(playable >= 0);
      assert(game_place(&game, playable / 9, playable % 9,
                        game.solution[playable], false));
      game_restart(&game);
      assert(!memcmp(initial, game.puzzle, sizeof(initial)));
    }
  }

  const int dates[][3] = {
      {2024, 2, 29}, {2026, 1, 1}, {2026, 8, 28}, {2026, 12, 31}};
  for (unsigned d = 0; d < sizeof(dates) / sizeof(dates[0]); ++d) {
    Game daily;
    assert(game_new_daily_revision(&daily, dates[d][0], dates[d][1],
                                   dates[d][2],
                                   SUDOKURA_GENERATOR_REVISION_LEGACY));
    hash = fnv_u64(hash, (uint64_t)dates[d][0]);
    hash = fnv_u64(hash, (uint64_t)dates[d][1]);
    hash = fnv_u64(hash, (uint64_t)dates[d][2]);
    for (int i = 0; i < 81; ++i)
      hash = fnv_byte(hash, (unsigned char)daily.initial[i]);
  }
  printf("quality-v2 legacy_digest=%016" PRIx64 "\n", hash);
  return hash;
}

int main(void) {
  uint64_t legacy = legacy_digest();
  uint64_t easy = run_v3_corpus(DIFFICULTY_EASY);
  uint64_t medium = run_v3_corpus(DIFFICULTY_MEDIUM);
  uint64_t hard = run_v3_corpus(DIFFICULTY_HARD);

  /* Locked after the first cross-platform corpus run. */
  const uint64_t expected_legacy = UINT64_C(0);
  const uint64_t expected_easy = UINT64_C(0);
  const uint64_t expected_medium = UINT64_C(0);
  const uint64_t expected_hard = UINT64_C(0);
  if (expected_legacy) assert(legacy == expected_legacy);
  if (expected_easy) assert(easy == expected_easy);
  if (expected_medium) assert(medium == expected_medium);
  if (expected_hard) assert(hard == expected_hard);

  puts("generator quality corpus passed independent uniqueness and elimination oracle checks");
  return 0;
}
