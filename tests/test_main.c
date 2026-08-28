#include "game.h"
#include "geometry.h"
#include "i18n.h"
#include "version.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool rects_overlap(GeoRect a, GeoRect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}

static void puzzle_text(const Game *game, char output[82]) {
  for (int i = 0; i < 81; ++i)
    output[i] = game->initial[i] ? (char)('0' + game->initial[i]) : '.';
  output[81] = '\0';
}

static int first_playable(const Game *game) {
  for (int i = 0; i < 81; ++i)
    if (!game->fixed[i]) return i;
  return -1;
}

static void test_generation(void) {
  for (uint64_t seed = 1; seed <= 24; ++seed) {
    int previous_clues = 82, previous_score = -1;
    for (int difficulty = DIFFICULTY_EASY; difficulty < DIFFICULTY_COUNT; ++difficulty) {
      Game game, duplicate;
      game_new_difficulty(&game, seed, (GameDifficulty)difficulty);
      game_new_difficulty(&duplicate, seed, (GameDifficulty)difficulty);
      assert(!memcmp(&game, &duplicate, sizeof(game)));
      assert(game.seed == seed);
      assert(game.generator_revision == SUDOKURA_GENERATOR_REVISION);
      assert(game.difficulty == (GameDifficulty)difficulty);
      assert(game_board_valid(game.solution));
      assert(game_solution_count(game.initial, 2) == 1);
      assert(game.difficulty_score == game_difficulty_score(game.initial));
      assert(game.difficulty_score > previous_score);
      int clues = game_clue_count(&game);
      assert(clues < previous_clues);
      if (difficulty == DIFFICULTY_EASY) assert(clues >= 42 && clues <= 46);
      else if (difficulty == DIFFICULTY_MEDIUM) assert(clues >= 34 && clues <= 38);
      else assert(clues >= 28 && clues <= 32);
      for (int i = 0; i < 81; ++i) {
        assert((game.fixed[i] != 0) == (game.initial[i] != 0));
        assert(game.puzzle[i] == game.initial[i]);
        assert(!game.hinted[i]);
        assert(!game.notes[i]);
      }
      previous_clues = clues;
      previous_score = game.difficulty_score;
    }
  }
}

static void test_generator_golden(void) {
  static const char *expected[DIFFICULTY_COUNT] = {
      "4..2..631.....1.9.16348.25...87.5.6.316...725.2..6...96..89..7..94..231..72316948",
      "..8..4...65.7...93.3456.8...651.73....368..41...9...86.......7.5..8..13.7.64..952",
      "...2.6319..9578..2....1.....4..3.75.3......6..78.....146.3.1..59...57.....5..4...",
  };
  for (int difficulty = DIFFICULTY_EASY; difficulty < DIFFICULTY_COUNT; ++difficulty) {
    Game game; char actual[82];
    game_new_difficulty(&game, 42, (GameDifficulty)difficulty);
    puzzle_text(&game, actual);
    assert(!strcmp(actual, expected[difficulty]));
  }
}

static void test_daily(void) {
  uint64_t seed_a = 0, seed_b = 0, seed_next = 0;
  assert(game_daily_seed(2026, 8, 28, &seed_a));
  assert(game_daily_seed(2026, 8, 28, &seed_b));
  assert(game_daily_seed(2026, 8, 29, &seed_next));
  assert(seed_a == UINT64_C(17236981323489437412));
  assert(seed_a == seed_b && seed_a != seed_next);
  assert(!game_daily_seed(2026, 2, 29, &seed_a));
  assert(game_daily_seed(2024, 2, 29, &seed_a));
  assert(!game_daily_seed(2026, 13, 1, &seed_a));
  assert(!game_daily_seed(2026, 8, 28, NULL));
  Game first, second;
  assert(game_new_daily(&first, 2026, 8, 28));
  assert(game_new_daily(&second, 2026, 8, 28));
  assert(!memcmp(&first, &second, sizeof(first)));
  assert(first.difficulty == DIFFICULTY_MEDIUM);
  assert(!game_new_daily(&first, 2026, 2, 29));
}

static void test_actions(void) {
  Game game; game_new(&game, 42);
  int i = first_playable(&game); assert(i >= 0);
  int row = i / 9, column = i % 9, value = game.solution[i];
  assert(game_toggle_note(&game, row, column, value));
  assert(game.notes[i] & (1u << value));
  assert(game_place(&game, row, column, value, false));
  assert(!game.notes[i]);
  assert(game_place(&game, row, column, 0, false));
  assert(game_hint(&game, row, column));
  assert(game.puzzle[i] == value && game.hinted[i]);
  assert(game_cell_locked(&game, row, column));
  assert(!game_place(&game, row, column, 0, false));
  assert(!game_toggle_note(&game, row, column, 1));
  assert(!game_hint(&game, row, column));
}

static void test_player_input(void) {
  Game game; game_new(&game, 99);
  int i = first_playable(&game); assert(i >= 0);
  int row = i / 9, column = i % 9, value = game.solution[i];
  assert(game_apply_input(&game, row, column, value, true, false) == GAME_INPUT_NOTE_ADDED);
  assert(game_apply_input(&game, row, column, value, true, false) == GAME_INPUT_NOTE_REMOVED);
  assert(game_apply_input(&game, row, column, value, false, false) == GAME_INPUT_CORRECT);
  assert(game_apply_input(&game, row, column, 0, false, false) == GAME_INPUT_CLEARED);
  int conflicting = 0;
  for (int x = 0; x < 9 && !conflicting; ++x)
    if (game.puzzle[row * 9 + x]) conflicting = game.puzzle[row * 9 + x];
  assert(conflicting);
  assert(game_apply_input(&game, row, column, conflicting, false, true) == GAME_INPUT_STRICT_REJECTED);
  int wrong = game.solution[i] % 9 + 1; assert(wrong != game.solution[i]);
  assert(game_apply_input(&game, row, column, wrong, false, false) == GAME_INPUT_WRONG);
  assert(game_apply_input(&game, row, column, 0, false, false) == GAME_INPUT_CLEARED);
  assert(game_apply_input(NULL, row, column, value, false, false) == GAME_INPUT_NO_CHANGE);
}

static void test_progress_restart(void) {
  Game game; game_new(&game, 314159); assert(game_progress_percent(&game) == 0);
  int original[81]; memcpy(original, game.initial, sizeof(original));
  uint64_t seed = game.seed; GameDifficulty difficulty = game.difficulty; int difficulty_score = game.difficulty_score;
  int correct_cell = first_playable(&game); assert(correct_cell >= 0);
  assert(game_apply_input(&game, correct_cell / 9, correct_cell % 9, game.solution[correct_cell], false, false) == GAME_INPUT_CORRECT);
  int progress = game_progress_percent(&game); assert(progress > 0);
  int wrong_cell = correct_cell + 1;
  while (wrong_cell < 81 && game.fixed[wrong_cell]) ++wrong_cell;
  if (wrong_cell == 81) { wrong_cell = 0; while (wrong_cell < correct_cell && game.fixed[wrong_cell]) ++wrong_cell; }
  assert(wrong_cell != correct_cell);
  int wrong = game.solution[wrong_cell] % 9 + 1;
  assert(game_apply_input(&game, wrong_cell / 9, wrong_cell % 9, wrong, false, false) == GAME_INPUT_WRONG);
  assert(game_progress_percent(&game) == progress);
  int hint_cell = wrong_cell + 1;
  while (hint_cell < 81 && game.fixed[hint_cell]) ++hint_cell;
  if (hint_cell == 81) { hint_cell = 0; while (hint_cell < 81 && (game.fixed[hint_cell] || hint_cell == correct_cell || hint_cell == wrong_cell)) ++hint_cell; }
  assert(hint_cell < 81 && game_hint(&game, hint_cell / 9, hint_cell % 9));
  assert(game.hinted[hint_cell] && game_progress_percent(&game) > progress);
  assert(game_apply_input(&game, hint_cell / 9, hint_cell % 9, 0, false, false) == GAME_INPUT_LOCKED);
  game_restart(&game);
  assert(game.seed == seed && game.difficulty == difficulty && game.difficulty_score == difficulty_score);
  assert(!memcmp(game.puzzle, original, sizeof(original)) && game_progress_percent(&game) == 0);
  for (int i = 0; i < 81; ++i) assert(!game.hinted[i] && !game.notes[i]);
  for (int i = 0; i < 81; ++i) if (!game.fixed[i]) assert(game_apply_input(&game, i / 9, i % 9, game.solution[i], false, false) == GAME_INPUT_CORRECT);
  assert(game_progress_percent(&game) == 100 && game_is_solved(&game));
}

static void test_bounds(void) {
  Game game; game_new(&game, 9);
  assert(!game_place(NULL, 0, 0, 1, false)); assert(!game_place(&game, -1, 0, 1, false)); assert(!game_place(&game, 0, 9, 1, false));
  assert(!game_toggle_note(&game, 9, 0, 1)); assert(!game_toggle_note(&game, 0, -1, 1)); assert(!game_hint(&game, -1, -1));
  assert(!game_cell_locked(&game, 9, 9)); assert(!game_has_conflict(&game, 9, 9)); assert(!game_has_conflict(NULL, 0, 0));
  assert(game_conflict_count(NULL) == 0 && game_clue_count(NULL) == 0 && game_progress_percent(NULL) == 0 && game_difficulty_score(NULL) == -1 && !game_is_solved(NULL));
  assert(game_solution_count(NULL, 2) == 0); int invalid[81] = {0}; invalid[0] = invalid[1] = 1; assert(game_solution_count(invalid, 2) == 0); assert(game_difficulty_score(invalid) == -1);
}

static void test_conflicts_and_end(void) {
  Game game; game_new(&game, 77); int a = -1, b = -1;
  for (int row = 0; row < 9 && a < 0; ++row) for (int column = 0; column < 9; ++column) if (!game.fixed[row * 9 + column]) { if (a < 0) a = row * 9 + column; else if (a / 9 == row) { b = row * 9 + column; break; } }
  assert(a >= 0 && b >= 0); game.puzzle[a] = game.puzzle[b] = 1; assert(game_has_conflict(&game, a / 9, a % 9)); assert(game_conflict_count(&game) >= 2);
  memcpy(game.puzzle, game.solution, sizeof(game.puzzle)); assert(game_is_solved(&game));
  assert(!game_mode_lost(MODE_CLASSIC, 99, 3, 999, 10)); assert(game_mode_lost(MODE_STRIKES, 3, 3, 0, 0)); assert(game_mode_lost(MODE_TIME, 0, 3, 601, 600));
}

static void assert_screen_geometry(const AppGeometry *g, int width, int height) {
  assert(geometry_rect_in_bounds(g->screen_language, width, height));
  assert(geometry_rect_in_bounds(g->home_logo, width, height));
  assert(geometry_rect_in_bounds(g->home_mode_label, width, height));
  assert(geometry_rect_in_bounds(g->home_difficulty_label, width, height));
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    assert(geometry_rect_in_bounds(g->home_mode[i], width, height));
    assert(geometry_rect_in_bounds(g->home_difficulty[i], width, height));
  }
  for (int i = 0; i < GEOMETRY_HOME_PRIMARY_COUNT; ++i) assert(geometry_rect_in_bounds(g->home_primary[i], width, height));
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i) assert(geometry_rect_in_bounds(g->home_secondary[i], width, height));
  assert(geometry_rect_in_bounds(g->info_heading, width, height)); assert(geometry_rect_in_bounds(g->info_body, width, height)); assert(geometry_rect_in_bounds(g->back_button, width, height));
  assert(geometry_rect_in_bounds(g->end_logo, width, height)); assert(geometry_rect_in_bounds(g->end_heading, width, height)); assert(geometry_rect_in_bounds(g->end_summary, width, height));
  for (int i = 0; i < GEOMETRY_END_BUTTON_COUNT; ++i) assert(geometry_rect_in_bounds(g->end_buttons[i], width, height));
  assert(geometry_rect_in_bounds(g->pause_logo, width, height)); assert(geometry_rect_in_bounds(g->pause_heading, width, height));
  for (int i = 0; i < GEOMETRY_PAUSE_BUTTON_COUNT; ++i) assert(geometry_rect_in_bounds(g->pause_buttons[i], width, height));
}

static void test_geometry(void) {
  const int sizes[][2] = {{640,480},{800,600},{1024,720},{1280,720},{1366,768},{1920,1080},{2560,1440},{3440,1440},{360,640},{390,844},{412,915}};
  for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
    for (int mode = GEOMETRY_MODE_CLASSIC; mode <= GEOMETRY_MODE_TIME; ++mode) {
      AppGeometry g; int width = sizes[s][0], height = sizes[s][1]; bool portrait = width < 640;
      assert(geometry_compute(width, height, (GeometryMode)mode, &g)); assert(geometry_play_valid(&g, width, height));
      assert(g.board.w % 9 == 0 && g.board.w / 9 >= 25); assert(g.hud_count == (mode == GEOMETRY_MODE_CLASSIC ? 5 : 6));
      assert(geometry_rect_in_bounds(g.play_title, width, height)); assert(geometry_rect_in_bounds(g.play_language, width, height)); assert(!rects_overlap(g.play_language, g.play_title));
      for (int i = 0; i < g.hud_count; ++i) assert(geometry_rect_in_bounds(g.hud[i], width, height));
      for (int i = 0; i < GEOMETRY_ACTION_COUNT; ++i) assert(g.actions[i].w >= 70 && g.actions[i].h >= 40);
      assert(geometry_rect_in_bounds(g.palette_label, width, height));
      for (int i = 0; i < GEOMETRY_PALETTE_COUNT; ++i) assert(g.palette[i].w >= (portrait ? 32 : 70) && g.palette[i].h >= 28);
      if (portrait) for (int i = 1; i < GEOMETRY_PALETTE_COUNT; ++i) assert(g.palette[i].y == g.palette[0].y);
      assert(geometry_rect_in_bounds(g.progress, width, height) && g.progress.h >= 18 && g.palette_label.h >= 16);
      assert_screen_geometry(&g, width, height);
    }
  }
  AppGeometry invalid; assert(!geometry_compute(359, 640, GEOMETRY_MODE_CLASSIC, &invalid)); assert(!geometry_compute(640, 479, GEOMETRY_MODE_CLASSIC, &invalid));
}

static void test_window_size_normalization(void) {
  const int cases[][4] = {{640,480,640,480},{800,600,800,600},{360,640,360,640},{390,844,390,844},{412,915,412,915},{500,500,640,500}};
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    int width = 0, height = 0; bool changed = geometry_normalize_window_size(cases[i][0], cases[i][1], &width, &height);
    assert(width == cases[i][2] && height == cases[i][3]); assert(changed == (width != cases[i][0] || height != cases[i][1])); assert(geometry_window_size_supported(width, height));
    for (int mode = GEOMETRY_MODE_CLASSIC; mode <= GEOMETRY_MODE_TIME; ++mode) { AppGeometry geometry; assert(geometry_compute(width, height, (GeometryMode)mode, &geometry)); }
    int second_width = 0, second_height = 0; assert(!geometry_normalize_window_size(width, height, &second_width, &second_height)); assert(second_width == width && second_height == height);
  }
}

static void test_i18n(void) {
  for (int language = 0; language < LANG_COUNT; ++language)
    for (int key = 0; key < T_COUNT; ++key) assert(tr((Language)language, (TextKey)key)[0]);
  assert(!strcmp(SUDOKURA_VERSION, "1.1.0"));
}

int main(void) {
  test_generation(); test_generator_golden(); test_daily(); test_actions(); test_player_input(); test_progress_restart(); test_bounds(); test_conflicts_and_end(); test_geometry(); test_window_size_normalization(); test_i18n();
  puts("all tests passed (deterministic generation, Daily, restart/progress/hints, modern responsive geometry, all screens, API bounds)");
  return 0;
}
