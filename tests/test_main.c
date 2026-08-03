#include "game.h"
#include "geometry.h"
#include "i18n.h"
#include "version.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool rects_overlap(GeoRect a, GeoRect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}

static void test_generation(void) {
  for (unsigned seed = 1; seed <= 12; ++seed) {
    Game game;
    game_new(&game, seed);
    assert(game_board_valid(game.solution));
    assert(game_solution_count(game.puzzle, 2) == 1);
    assert(game_clue_count(&game) >= 32 && game_clue_count(&game) <= 38);
    for (int i = 0; i < 81; ++i)
      assert((game.fixed[i] != 0) == (game.puzzle[i] != 0));
  }
}

static void test_actions(void) {
  Game game;
  game_new(&game, 42);
  int i = 0;
  while (game.fixed[i]) ++i;
  int row = i / 9, column = i % 9, value = game.solution[i];
  assert(game_toggle_note(&game, row, column, value));
  assert(game.notes[i] & (1u << value));
  assert(game_place(&game, row, column, value, false));
  assert(!game.notes[i]);
  assert(game_place(&game, row, column, 0, false));
  assert(game_hint(&game, row, column));
  assert(game.puzzle[i] == value);
  assert(!game_hint(&game, row, column));
}

static void test_bounds(void) {
  Game game;
  game_new(&game, 9);
  assert(!game_place(NULL, 0, 0, 1, false));
  assert(!game_place(&game, -1, 0, 1, false));
  assert(!game_place(&game, 0, 9, 1, false));
  assert(!game_toggle_note(&game, 9, 0, 1));
  assert(!game_toggle_note(&game, 0, -1, 1));
  assert(!game_hint(&game, -1, -1));
  assert(!game_has_conflict(&game, 9, 9));
  assert(!game_has_conflict(NULL, 0, 0));
  assert(game_conflict_count(NULL) == 0);
  assert(game_clue_count(NULL) == 0);
  assert(!game_is_solved(NULL));
  assert(game_solution_count(NULL, 2) == 0);
  int invalid[81] = {0}; invalid[0] = invalid[1] = 1;
  assert(game_solution_count(invalid, 2) == 0);
}

static void test_conflicts_and_end(void) {
  Game game;
  game_new(&game, 77);
  int a = -1, b = -1;
  for (int row = 0; row < 9 && a < 0; ++row)
    for (int column = 0; column < 9; ++column)
      if (!game.fixed[row * 9 + column]) {
        if (a < 0) a = row * 9 + column;
        else if (a / 9 == row) { b = row * 9 + column; break; }
      }
  assert(a >= 0 && b >= 0);
  game.puzzle[a] = game.puzzle[b] = 1;
  assert(game_has_conflict(&game, a / 9, a % 9));
  assert(game_conflict_count(&game) >= 2);
  memcpy(game.puzzle, game.solution, sizeof(game.puzzle));
  assert(game_is_solved(&game));
  assert(!game_mode_lost(MODE_CLASSIC, 99, 3, 999, 10));
  assert(game_mode_lost(MODE_STRIKES, 3, 3, 0, 0));
  assert(game_mode_lost(MODE_TIME, 0, 3, 601, 600));
}

static void assert_screen_geometry(const AppGeometry *g, int width, int height) {
  assert(geometry_rect_in_bounds(g->title_heading, width, height));
  for (int i = 0; i < GEOMETRY_TITLE_BUTTON_COUNT; ++i)
    assert(geometry_rect_in_bounds(g->title_buttons[i], width, height));
  assert(geometry_rect_in_bounds(g->info_heading, width, height));
  assert(geometry_rect_in_bounds(g->info_body, width, height));
  assert(geometry_rect_in_bounds(g->back_button, width, height));
  assert(geometry_rect_in_bounds(g->end_heading, width, height));
  assert(geometry_rect_in_bounds(g->end_summary, width, height));
  for (int i = 0; i < GEOMETRY_END_BUTTON_COUNT; ++i)
    assert(geometry_rect_in_bounds(g->end_buttons[i], width, height));
}

static void test_geometry(void) {
  const int sizes[][2] = {{640,480},{800,600},{1024,720},{1280,720},
                          {1366,768},{1920,1080}};
  for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
    for (int mode = GEOMETRY_MODE_CLASSIC; mode <= GEOMETRY_MODE_TIME; ++mode) {
      AppGeometry g;
      int width = sizes[s][0], height = sizes[s][1];
      assert(geometry_compute(width, height, (GeometryMode)mode, &g));
      assert(geometry_play_valid(&g, width, height));
      assert(g.board.w % 9 == 0 && g.board.w / 9 >= 32);
      assert(g.hud_count == (mode == GEOMETRY_MODE_CLASSIC ? 2 : 3));
      assert(geometry_rect_in_bounds(g.play_title, width, height));
      for (int i = 0; i < g.hud_count; ++i)
        assert(geometry_rect_in_bounds(g.hud[i], width, height));
      for (int i = 0; i < GEOMETRY_ACTION_COUNT; ++i)
        assert(g.actions[i].w >= 110 && g.actions[i].h >= 26);
      assert(geometry_rect_in_bounds(g.palette_label, width, height));
      for (int i = 0; i < GEOMETRY_PALETTE_COUNT; ++i)
        assert(g.palette[i].w >= 70 && g.palette[i].h >= 24);
      assert(geometry_rect_in_bounds(g.progress, width, height));
      assert(geometry_rect_in_bounds(g.language, width, height));
      assert(!rects_overlap(g.language, g.play_title));
      for (int i = 0; i < g.hud_count; ++i)
        assert(!rects_overlap(g.language, g.hud[i]));
      assert_screen_geometry(&g, width, height);
    }
  }
  AppGeometry invalid;
  assert(!geometry_compute(639, 480, GEOMETRY_MODE_CLASSIC, &invalid));
  assert(!geometry_compute(640, 479, GEOMETRY_MODE_CLASSIC, &invalid));
}

static void test_i18n(void) {
  for (int language = 0; language < LANG_COUNT; ++language)
    for (int key = 0; key < T_COUNT; ++key)
      assert(tr((Language)language, (TextKey)key)[0]);
  assert(!strcmp(SUDOKURA_VERSION, "1.1.0"));
}

int main(void) {
  test_generation();
  test_actions();
  test_bounds();
  test_conflicts_and_end();
  test_geometry();
  test_i18n();
  puts("all tests passed (12 seeds; 18 mode/viewports; all screens; API bounds)");
  return 0;
}
