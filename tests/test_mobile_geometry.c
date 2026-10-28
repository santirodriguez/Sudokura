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
  assert(g.palette[5].y == g.palette[0].y + 50);
  AudioControlGeometry speaker = geometry_audio_control(w, h, &g, true);
  assert(speaker.button.w >= 48 && speaker.button.h >= 48);
  assert(geometry_rect_in_bounds(speaker.button, w, h));
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
  assert(!geometry_window_size_supported(360, 600));
  assert(!geometry_compute(360, 600, GEOMETRY_MODE_CLASSIC, &g));
  assert(geometry_compute(360, 640, GEOMETRY_MODE_CLASSIC, &g));
  puts("Android 360x600dp layout, 48dp controls and desktop bounds passed");
  return 0;
}
