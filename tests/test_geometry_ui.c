#include "geometry.h"

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
      assert(geometry_compute(width, height, (GeometryMode)mode, &g));
      assert_inside(g.about_study, g.about_study_link);
      assert(g.about_fact.h >= 48);
      assert(g.about_study.h >= 48);
      assert(GEOMETRY_HOME_SECONDARY_COUNT == 5);
      assert(GEOMETRY_TITLE_BUTTON_COUNT == 8);
      assert_rect(g.home_new_label,width,height);
      assert(g.home_new_label.h >= 20);
      assert(g.home_continue_label.h >= 20);
      assert(g.home_mode_label.h >= 20);
      assert(g.home_difficulty_label.h >= 20);
      assert_rect(g.home_continue_label,width,height);
      assert(g.home_new_label.y < g.home_mode_label.y);
      assert(g.home_mode[0].y < g.home_difficulty_label.y);
      assert(g.home_primary[0].w == g.home_logo.w);
      assert(g.home_primary[0].y > g.home_difficulty[0].y);
      assert(g.home_primary[1].y > g.home_continue_label.y);
      if (portrait) {
        assert(g.home_primary[2].y > g.home_primary[1].y);
      } else {
        assert(g.home_primary[1].y == g.home_primary[2].y);
        assert(g.home_primary[1].w <= g.home_primary[2].w);
        assert(g.home_primary[2].w - g.home_primary[1].w <= 1);
      }
      for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i) {
        assert_rect(g.home_secondary[i],width,height);
        if (i > 0) {
          if (i == 1)
            assert(g.home_secondary[i].y > g.home_secondary[0].y);
          else if (portrait && i == 3)
            assert(g.home_secondary[i].y > g.home_secondary[i-1].y);
          else
            assert(g.home_secondary[i].y == g.home_secondary[i-1].y);
        }
      }
      assert(g.home_secondary[0].w == g.home_logo.w);
      assert(g.home_secondary[0].y > g.home_primary[2].y);
      assert_rect(g.board, width, height);
      assert_rect(g.sidebar, width, height);
      for (int action = 0; action < PLAY_ACTION_COUNT; ++action)
        assert_rect(g.actions[action], width, height);
      for (int number = 0; number < GEOMETRY_PALETTE_COUNT; ++number)
        assert_rect(g.palette[number], width, height);

      AudioControlGeometry play_audio =
          geometry_audio_control(width, height, &g, true);
      AudioControlGeometry screen_audio =
          geometry_audio_control(width, height, &g, false);
      assert_rect(play_audio.button, width, height);
      assert_rect(play_audio.popup, width, height);
      assert_inside(play_audio.popup, play_audio.music);
      assert_inside(play_audio.popup, play_audio.fx);
      assert_rect(screen_audio.button, width, height);
      assert_rect(screen_audio.popup, width, height);
      assert_inside(screen_audio.popup, screen_audio.music);
      assert_inside(screen_audio.popup, screen_audio.fx);
      assert_inside(g.actions[PLAY_ACTION_AUDIO],play_audio.button);
      assert(play_audio.button.x - 4 >
             g.actions[PLAY_ACTION_AUDIO].x + 48);
      assert(screen_audio.button.x >=
             g.screen_language.x + g.screen_language.w);

      assert(g.actions[PLAY_ACTION_MENU].x == g.sidebar.x);
      assert(g.actions[PLAY_ACTION_MENU].w == g.sidebar.w);
      assert(g.actions[PLAY_ACTION_ABOUT].x == g.sidebar.x);
      assert(g.actions[PLAY_ACTION_ABOUT].w == g.sidebar.w);
      assert(g.actions[PLAY_ACTION_PAUSE].y <
             g.actions[PLAY_ACTION_RESTART].y);
      assert(g.actions[PLAY_ACTION_PAUSE].w == g.sidebar.w);
      assert(g.actions[PLAY_ACTION_RESTART].w == g.sidebar.w);
      assert(g.actions[PLAY_ACTION_RESTART].y <
             g.actions[PLAY_ACTION_HINT].y);
      assert(g.actions[PLAY_ACTION_HINT].y == g.actions[PLAY_ACTION_NOTES].y);
      assert(g.actions[PLAY_ACTION_HINT].y == g.actions[PLAY_ACTION_VERIFY].y);
      assert(g.actions[PLAY_ACTION_AUDIO].y == g.actions[PLAY_ACTION_HELP].y);
      assert(g.actions[PLAY_ACTION_MENU].y < g.actions[PLAY_ACTION_PAUSE].y);
      assert(g.actions[PLAY_ACTION_PAUSE].y < g.actions[PLAY_ACTION_HINT].y);
      assert(g.actions[PLAY_ACTION_HINT].y < g.actions[PLAY_ACTION_AUDIO].y);
      assert(g.actions[PLAY_ACTION_HELP].y + g.actions[PLAY_ACTION_HELP].h <=
             g.palette_label.y);
      assert(g.palette_label.y < g.progress.y);
      assert(g.progress.y + g.progress.h <= g.status.y);
      assert(g.actions[PLAY_ACTION_ABOUT].x == g.status.x);
      assert(g.actions[PLAY_ACTION_ABOUT].y == g.status.y);
      assert(g.actions[PLAY_ACTION_ABOUT].w == g.status.w);
      assert(g.actions[PLAY_ACTION_ABOUT].h == g.status.h);

      if (portrait) {
        assert(g.board.w >= 225);
        for (int i = 1; i < GEOMETRY_ABOUT_LINK_COUNT; ++i)
          assert(g.about_links[i].y > g.about_links[i - 1].y);
      } else {
        assert(g.about_links[0].y == g.about_links[1].y);
        assert(g.about_links[2].y == g.about_links[3].y);
        assert(g.about_links[2].y > g.about_links[0].y);
        assert(g.play_title.h >= 38);
        assert(g.actions[PLAY_ACTION_PAUSE].h >= 28);
        assert(g.actions[PLAY_ACTION_RESTART].h >= 28);
        assert(g.play_language.y - g.sidebar.y <= 36);
      }
    }
  }
  puts("UI geometry tests passed for play controls, global audio geometry and About screen");
  return 0;
}
