#include "android_viewport.h"

#include <limits.h>
#include <math.h>

bool android_viewport_compute(int width_px, int height_px, float density,
                              int touch_slop_px, AndroidViewport *out) {
  if (!out || width_px <= 0 || height_px <= 0 || !isfinite(density) ||
      density <= 0.0f)
    return false;
  const double width_dp = (double)width_px / density;
  const double height_dp = (double)height_px / density;
  if (!isfinite(width_dp) || !isfinite(height_dp) ||
      width_dp < 0.5 || height_dp < 0.5 ||
      width_dp >= (double)INT_MAX - 1 || height_dp >= (double)INT_MAX - 1)
    return false;
  const int w = (int)(width_dp + 0.5);
  const int h = (int)(height_dp + 0.5);
  if (w < 1 || h < 1) return false;

  int slop = 8; /* Desktop-compatible fallback if ViewConfiguration is absent. */
  if (touch_slop_px > 0) {
    const double logical_slop = ceil((double)touch_slop_px / density);
    if (!isfinite(logical_slop) || logical_slop >= (double)INT_MAX)
      return false;
    slop = (int)logical_slop;
    if (slop < 1) slop = 1;
  }

  *out = (AndroidViewport){.width_dp = w, .height_dp = h,
                           .touch_slop_dp = slop,
                           .surface_width_px = width_px,
                           .surface_height_px = height_px,
                           .density = density};
  return true;
}
