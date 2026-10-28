#include "geometry.h"

#include <assert.h>
#include <stdio.h>

static bool overlap(GeoRect a, GeoRect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}

static void check(int w, int h, GeometryMode mode) {
  AppGeometry g;
  assert(geometry_compute_mobile(w, h, mode, &g));
  assert(geometry_play_valid(&g, w, h));
  assert(g.play_compact);
  assert(g.board.w >= 270 && g.board.w == g.board.h);
  assert(g.board.w % 9 == 0);
  for (int i = 0; i < GEOMETRY_PALETTE_COUNT; ++i) {
    assert(g.palette[i].w >= 48 && g.palette[i].h >= 48);
    assert(geometry_rect_in_bounds(g.palette[i], w, h));
    for (int j = 0; j < i; ++j)
      assert(!overlap(g.palette[i], g.palette[j]));
  }
  assert(g.palette[4].y == g.palette[0].y);
  assert(g.palette[5].y == g.palette[0].y + 54);
  assert(g.progress.h >= 24);
  assert(g.hud[0].h >= 22);
  assert(geometry_rect_in_bounds(g.status, w, h));
  assert(g.play_title.h >= 20);
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    assert(g.home_mode[i].h >= 48 && g.home_difficulty[i].h >= 48);
    assert(geometry_rect_in_bounds(g.home_mode[i], w, h));
    assert(geometry_rect_in_bounds(g.home_difficulty[i], w, h));
  }
  for (int i = 0; i < GEOMETRY_HOME_PRIMARY_COUNT; ++i) {
    assert(g.home_primary[i].h >= 48);
    assert(geometry_rect_in_bounds(g.home_primary[i], w, h));
  }
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i) {
    assert(g.home_secondary[i].h >= 48);
    assert(geometry_rect_in_bounds(g.home_secondary[i], w, h));
  }
  AudioControlGeometry home_speaker = geometry_audio_control(w, h, &g, false);
  assert(home_speaker.button.w >= 48 && home_speaker.button.h >= 48);
  assert(geometry_rect_in_bounds(home_speaker.button, w, h));
  assert(home_speaker.music.h >= 48 && home_speaker.fx.h >= 48);
  assert(geometry_rect_in_bounds(home_speaker.popup, w, h));
  assert(!overlap(home_speaker.button, g.screen_language));
  assert(!overlap(g.home_mode[0], g.home_logo));
  assert(!overlap(g.home_secondary[3], g.home_secondary[4]));
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i)
    for (int j = 0; j < i; ++j) {
      assert(!overlap(g.home_mode[i], g.home_mode[j]));
      assert(!overlap(g.home_difficulty[i], g.home_difficulty[j]));
    }
  AudioControlGeometry speaker = geometry_audio_control(w, h, &g, true);
  assert(speaker.button.w >= 48 && speaker.button.h >= 48);
  assert(geometry_rect_in_bounds(speaker.button, w, h));
  assert(speaker.music.h >= 48 && speaker.fx.h >= 48);
  assert(geometry_rect_in_bounds(speaker.popup, w, h));
  for (int i = PLAY_CONTROL_MENU; i <= PLAY_CONTROL_MORE; ++i)
    assert(!overlap(speaker.button, g.play_controls[i]));
  assert(!overlap(speaker.button, g.board));
  for (int i = PLAY_CONTROL_MENU; i <= PLAY_CONTROL_NOTES; ++i) {
    assert(geometry_rect_in_bounds(g.play_controls[i], w, h));
    assert(g.play_controls[i].w >= 48 && g.play_controls[i].h >= 48);
  }
  for (int i = 0; i < GEOMETRY_MORE_COUNT; ++i) {
    assert(geometry_rect_in_bounds(g.more_items[i], w, h));
    assert(g.more_items[i].w >= 48 && g.more_items[i].h >= 48);
    for (int j = 0; j < i; ++j)
      assert(!overlap(g.more_items[i], g.more_items[j]));
  }
}

int main(void) {
  for (int w = 360; w <= 639; w += 27) {
    for (int h = 600; h <= 920; h += 40)
      for (int mode = GEOMETRY_MODE_CLASSIC;
           mode <= GEOMETRY_MODE_TIME; ++mode)
        check(w, h, (GeometryMode)mode);
  }
  check(360, 600, GEOMETRY_MODE_CLASSIC);
  check(390, 844, GEOMETRY_MODE_STRIKES);
  check(412, 915, GEOMETRY_MODE_TIME);
  AppGeometry g;
  assert(!geometry_compute_mobile(359, 600, GEOMETRY_MODE_CLASSIC, &g));
  assert(!geometry_compute_mobile(360, 599, GEOMETRY_MODE_CLASSIC, &g));
  assert(!geometry_compute_mobile(780, 430, GEOMETRY_MODE_CLASSIC, &g));
  assert(!geometry_window_size_supported(360, 600));
  assert(!geometry_compute(360, 600, GEOMETRY_MODE_CLASSIC, &g));
  assert(geometry_compute(360, 640, GEOMETRY_MODE_CLASSIC, &g));
  puts("Android 360x600dp layout, 48dp controls and desktop bounds passed");
  return 0;
}
