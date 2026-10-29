#ifndef SUDOKURA_ANDROID_VIEWPORT_H
#define SUDOKURA_ANDROID_VIEWPORT_H

#include <stdbool.h>

/* Pure density policy. Java owns system insets; the SDL surface is already
   the usable rectangle, so native rendering must not subtract them again. */
typedef struct {
  int width_dp, height_dp;
  int touch_slop_dp;
  int surface_width_px, surface_height_px;
  float density;
} AndroidViewport;

bool android_viewport_compute(int surface_width_px, int surface_height_px,
                              float density, int touch_slop_px,
                              AndroidViewport *viewport);

#endif
