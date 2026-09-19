#include "desktop.h"

#include <limits.h>
#include <stdint.h>

bool desktop_metrics_compute(int logical_width, int logical_height,
                             int output_width, int output_height,
                             DesktopMetrics *out) {
  if (!out || logical_width <= 0 || logical_height <= 0 ||
      output_width <= 0 || output_height <= 0)
    return false;
  int64_t sx = ((int64_t)output_width * 1000 + logical_width / 2) /
               logical_width;
  int64_t sy = ((int64_t)output_height * 1000 + logical_height / 2) /
               logical_height;
  int64_t scale = sx > sy ? sx : sy;
  if (scale < 500) scale = 500;
  if (scale > 8000) scale = 8000;
  *out = (DesktopMetrics){
      .logical_width = logical_width,
      .logical_height = logical_height,
      .output_width = output_width,
      .output_height = output_height,
      .scale_milli = (int)scale,
  };
  return true;
}

int desktop_scale_value(int logical_value, int scale_milli) {
  if (logical_value <= 0) return logical_value;
  if (scale_milli <= 0) scale_milli = 1000;
  int64_t scaled = ((int64_t)logical_value * scale_milli + 500) / 1000;
  if (scaled < 1) scaled = 1;
  if (scaled > INT_MAX) scaled = INT_MAX;
  return (int)scaled;
}

int desktop_unscale_value(int physical_value, int scale_milli) {
  if (physical_value <= 0) return physical_value;
  if (scale_milli <= 0) scale_milli = 1000;
  int64_t logical = ((int64_t)physical_value * 1000 + scale_milli / 2) /
                    scale_milli;
  if (logical < 1) logical = 1;
  if (logical > INT_MAX) logical = INT_MAX;
  return (int)logical;
}

bool desktop_rect_has_visible_area(DesktopRect window, DesktopRect display,
                                   int minimum_visible) {
  if (window.w <= 0 || window.h <= 0 || display.w <= 0 || display.h <= 0)
    return false;
  if (minimum_visible < 1) minimum_visible = 1;
  int64_t left = window.x > display.x ? window.x : display.x;
  int64_t top = window.y > display.y ? window.y : display.y;
  int64_t right_a = (int64_t)window.x + window.w;
  int64_t right_b = (int64_t)display.x + display.w;
  int64_t bottom_a = (int64_t)window.y + window.h;
  int64_t bottom_b = (int64_t)display.y + display.h;
  int64_t right = right_a < right_b ? right_a : right_b;
  int64_t bottom = bottom_a < bottom_b ? bottom_a : bottom_b;
  return right - left >= minimum_visible &&
         bottom - top >= minimum_visible;
}
