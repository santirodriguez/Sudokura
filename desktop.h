#ifndef SUDOKURA_DESKTOP_H
#define SUDOKURA_DESKTOP_H

#include <stdbool.h>

typedef struct {
  int logical_width;
  int logical_height;
  int output_width;
  int output_height;
  int scale_milli;
} DesktopMetrics;

typedef struct {
  int x;
  int y;
  int w;
  int h;
} DesktopRect;

bool desktop_metrics_compute(int logical_width, int logical_height,
                             int output_width, int output_height,
                             DesktopMetrics *out);
int desktop_scale_value(int logical_value, int scale_milli);
int desktop_unscale_value(int physical_value, int scale_milli);
bool desktop_rect_has_visible_area(DesktopRect window, DesktopRect display,
                                   int minimum_visible);

#endif
