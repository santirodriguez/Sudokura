#include "geometry.h"
#include "src/sudokura_sdl/ui_geometry.inc"

#include <assert.h>
#include <stdio.h>

static void assert_inside(GeoRect outer, GeoRect inner) {
  assert(inner.x >= outer.x);
  assert(inner.y >= outer.y);
  assert(inner.x + inner.w <= outer.x + outer.w);
  assert(inner.y + inner.h <= outer.y + outer.h);
}

static void assert_rect(GeoRect rect, int width, int height) {
  assert(geometry_rect_in_bounds(rect, width, height));
}

int main(void) {
  const int sizes[][2] = {
      {640, 480}, {800, 600}, {1024, 720}, {1366, 768}, {1920, 1080},
      {2560, 1440}, {3440, 1440}, {360, 640}, {390, 844}, {412, 915},
  };

  assert(GEOMETRY_ABOUT_LINK_COUNT == 4);
  assert(PLAY_ACTION_COUNT == GEOMETRY_ACTION_COUNT);
  for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
    int width = sizes[s][0], height = sizes[s][1];
    bool portrait = width < 640;
    for (int mode = GEOMETRY_MODE_CLASSIC; mode <= GEOMETRY_MODE_TIME; ++mode) {
      AppGeometry g;
      assert(ui_geometry_compute(width, height, (GeometryMode)mode, &g));
      assert_inside(g.about_study, g.about_study_link);
      assert(g.about_fact.h >= 48);
      assert(g.about_study.h >= 48);
      assert_rect(g.board, width, height);
      assert_rect(g.sidebar, width, height);
      for (int action = 0; action < PLAY_ACTION_COUNT; ++action)
        assert_rect(g.actions[action], width, height);
      for (int number = 0; number < GEOMETRY_PALETTE_COUNT; ++number)
        assert_rect(g.palette[number], width, height);

      assert(g.actions[PLAY_ACTION_MENU].x == g.sidebar.x);
      assert(g.actions[PLAY_ACTION_MENU].w == g.sidebar.w);
      assert(g.actions[PLAY_ACTION_ABOUT].x == g.sidebar.x);
      assert(g.actions[PLAY_ACTION_ABOUT].w == g.sidebar.w);
      assert(g.actions[PLAY_ACTION_PAUSE].y == g.actions[PLAY_ACTION_RESTART].y);
      assert(g.actions[PLAY_ACTION_HINT].y == g.actions[PLAY_ACTION_NOTES].y);
      assert(g.actions[PLAY_ACTION_HINT].y == g.actions[PLAY_ACTION_VERIFY].y);
      assert(g.actions[PLAY_ACTION_AUDIO].y == g.actions[PLAY_ACTION_HELP].y);
      assert(g.actions[PLAY_ACTION_MENU].y < g.actions[PLAY_ACTION_PAUSE].y);
      assert(g.actions[PLAY_ACTION_PAUSE].y < g.actions[PLAY_ACTION_HINT].y);
      assert(g.actions[PLAY_ACTION_HINT].y < g.actions[PLAY_ACTION_AUDIO].y);
      assert(g.actions[PLAY_ACTION_HELP].y + g.actions[PLAY_ACTION_HELP].h <=
             g.palette_label.y);
      assert(g.palette_label.y < g.progress.y);
      assert(g.progress.y + g.progress.h <= g.actions[PLAY_ACTION_ABOUT].y);

      if (portrait) {
        assert(g.board.w >= 225);
        for (int i = 1; i < GEOMETRY_ABOUT_LINK_COUNT; ++i)
          assert(g.about_links[i].y > g.about_links[i - 1].y);
      } else {
        assert(g.about_links[0].y == g.about_links[1].y);
        assert(g.about_links[2].y == g.about_links[3].y);
        assert(g.about_links[2].y > g.about_links[0].y);
        assert(g.play_title.h >= 52);
        assert(g.play_language.y - g.sidebar.y <= 36);
      }
    }
  }
  puts("UI geometry tests passed for hierarchical play controls, footer About and four-link About screen");
  return 0;
}
