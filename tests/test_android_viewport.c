#include "android_viewport.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

static void size(int w, int h, float density, int px_slop,
                 int expected_w, int expected_h, int expected_slop) {
  AndroidViewport viewport = {0};
  assert(android_viewport_compute(w, h, density, px_slop, &viewport));
  assert(viewport.width_dp == expected_w);
  assert(viewport.height_dp == expected_h);
  assert(viewport.touch_slop_dp == expected_slop);
  assert(viewport.surface_width_px == w && viewport.surface_height_px == h);
}

int main(void) {
  size(360, 600, 1.0f, 8, 360, 600, 8);
  size(720, 1200, 2.0f, 16, 360, 600, 8);
  size(1080, 1800, 3.0f, 24, 360, 600, 8);
  size(1440, 2400, 4.0f, 32, 360, 600, 8);
  size(1080, 2400, 3.0f, 25, 360, 800, 9);
  size(1280, 800, 2.0f, 18, 640, 400, 9); /* Landscape needs a policy. */
  size(1080, 1800, 3.0f, 0, 360, 600, 8);
  size(1080, 1800, 3.0f, 1, 360, 600, 1);
  AndroidViewport v = {0};
  assert(!android_viewport_compute(0, 1800, 3.0f, 8, &v));
  assert(!android_viewport_compute(1080, 0, 3.0f, 8, &v));
  assert(!android_viewport_compute(1080, 1800, 0.0f, 8, &v));
  assert(!android_viewport_compute(1080, 1800, NAN, 8, &v));
  assert(!android_viewport_compute(1080, 1800, INFINITY, 8, &v));
  assert(!android_viewport_compute(INT_MAX, INT_MAX, 0.001f, 8, &v));
  assert(!android_viewport_compute(1080, 1800, 3.0f, 8, NULL));
  puts("Android surface density, bounds and slop policy passed");
  return 0;
}
