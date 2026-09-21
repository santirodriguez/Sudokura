#ifndef SUDOKURA_DESKTOP_H
#define SUDOKURA_DESKTOP_H

#include <stdbool.h>
#include <stdint.h>

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

typedef enum {
  DESKTOP_WINDOW_EVENT_OTHER = 0,
  DESKTOP_WINDOW_EVENT_FOCUS_LOST,
  DESKTOP_WINDOW_EVENT_MINIMIZED,
  DESKTOP_WINDOW_EVENT_HIDDEN
} DesktopWindowEvent;

enum {
  DESKTOP_WINDOW_INPUT_FOCUS = 1u << 0,
  DESKTOP_WINDOW_MINIMIZED = 1u << 1,
  DESKTOP_WINDOW_HIDDEN = 1u << 2
};

bool desktop_metrics_compute(int logical_width, int logical_height,
                             int output_width, int output_height,
                             DesktopMetrics *out);
int desktop_scale_value(int logical_value, int scale_milli);
int desktop_unscale_value(int physical_value, int scale_milli);
bool desktop_rect_has_visible_area(DesktopRect window, DesktopRect display,
                                   int minimum_visible);
bool desktop_tick_at_or_after(uint32_t event_tick, uint32_t floor_tick);
bool desktop_window_event_requires_pause(DesktopWindowEvent event,
                                         unsigned window_state);
bool desktop_window_event_should_pause(uint32_t event_tick,
                                       uint32_t floor_tick,
                                       DesktopWindowEvent event,
                                       unsigned window_state);

#endif
