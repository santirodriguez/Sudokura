#include "game.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Compare complete original/solution identities, not clue counts or scores.
   This test is compiled against both release trees on the same CI runner. */
static bool emit(const char *kind, uint32_t revision, GameDifficulty difficulty,
                 const Game *game) {
  if (!game || game->generator_revision != revision ||
      game->difficulty != difficulty ||
      !game_board_valid(game->solution) ||
      game_solution_count(game->initial, 2) != 1)
    return false;
  printf("%s|revision=%" PRIu32 "|difficulty=%d|seed=%" PRIu64
         "|score=%d|original=", kind, revision, (int)difficulty, game->seed,
         game->difficulty_score);
  for (int i = 0; i < SUDOKU_CELLS; ++i) {
    if (game->initial[i] < 0 || game->initial[i] > 9 ||
        game->solution[i] < 1 || game->solution[i] > 9 ||
        (game->initial[i] && game->initial[i] != game->solution[i]))
      return false;
    putchar(game->initial[i] ? '0' + game->initial[i] : '.');
  }
  fputs("|solution=", stdout);
  for (int i = 0; i < SUDOKU_CELLS; ++i)
    putchar('0' + game->solution[i]);
  putchar('\n');
  return !ferror(stdout);
}

int main(void) {
  const uint64_t v3_seeds[] = {UINT64_C(1), UINT64_C(3)};
  puts("deterministic-identity-fixtures=1");
  for (int difficulty = DIFFICULTY_EASY;
       difficulty < DIFFICULTY_COUNT; ++difficulty) {
    Game game = {0};
    if (!game_new_difficulty_revision(
            &game, UINT64_C(42), (GameDifficulty)difficulty,
            SUDOKURA_GENERATOR_REVISION_LEGACY) ||
        !emit("normal", SUDOKURA_GENERATOR_REVISION_LEGACY,
              (GameDifficulty)difficulty, &game))
      return 1;
    for (unsigned i = 0; i < sizeof(v3_seeds) / sizeof(v3_seeds[0]); ++i) {
      if (!game_new_difficulty_revision(
              &game, v3_seeds[i], (GameDifficulty)difficulty,
              SUDOKURA_GENERATOR_REVISION) ||
          !emit("normal", SUDOKURA_GENERATOR_REVISION,
                (GameDifficulty)difficulty, &game))
        return 1;
    }
  }
  for (uint32_t revision = SUDOKURA_GENERATOR_REVISION_LEGACY;
       revision <= SUDOKURA_GENERATOR_REVISION; ++revision) {
    Game daily = {0};
    if (!game_new_daily_revision(&daily, 2026, 8, 28, revision) ||
        !emit("daily-2026-08-28", revision, DIFFICULTY_MEDIUM, &daily))
      return 1;
  }
  return ferror(stdout) ? 1 : 0;
}
