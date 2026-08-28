#include "geometry.h"

#include <assert.h>
#include <stdio.h>

static void assert_inside(GeoRect outer, GeoRect inner) {
  assert(inner.x >= outer.x);
  assert(inner.y >= outer.y);
  assert(inner.x + inner.w <= outer.x + outer.w);
  assert(inner.y + inner.h <= outer.y + outer.h);
}

int main(void) {
  const int sizes[][2] = {
      {640, 480}, {800, 600}, {1024, 720}, {1366, 768}, {1920, 1080},
      {2560, 1440}, {3440, 1440}, {360, 640}, {390, 844}, {412, 915},
  };

  assert(GEOMETRY_ABOUT_LINK_COUNT == 4);
  for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
    int width = sizes[s][0], height = sizes[s][1];
    bool portrait = width < 640;
    for (int mode = GEOMETRY_MODE_CLASSIC; mode <= GEOMETRY_MODE_TIME; ++mode) {
      AppGeometry g;
      assert(geometry_compute(width, height, (GeometryMode)mode, &g));
      assert(geometry_play_valid(&g, width, height));
      assert_inside(g.about_study, g.about_study_link);
      assert(g.about_fact.h >= 48);
      assert(g.about_study.h >= 48);
      if (portrait) {
        for (int i = 1; i < GEOMETRY_ABOUT_LINK_COUNT; ++i)
          assert(g.about_links[i].y > g.about_links[i - 1].y);
      } else {
        assert(g.about_links[0].y == g.about_links[1].y);
        assert(g.about_links[2].y == g.about_links[3].y);
        assert(g.about_links[2].y > g.about_links[0].y);
        assert(g.play_title.h >= 52);
        assert(g.play_language.y - g.sidebar.y <= 36);
        assert(g.progress.y + g.progress.h <= g.sidebar.y + g.sidebar.h);
      }
    }
  }
  puts("UI geometry tests passed for four-link About and balanced desktop sidebar");
  return 0;
}
