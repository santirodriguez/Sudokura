#include "desktop.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
  DesktopMetrics metrics;
  assert(desktop_metrics_compute(1024, 720, 1024, 720, &metrics));
  assert(metrics.scale_milli == 1000);
  assert(desktop_metrics_compute(1024, 720, 2048, 1440, &metrics));
  assert(metrics.scale_milli == 2000);
  assert(desktop_scale_value(44, metrics.scale_milli) == 88);
  assert(desktop_unscale_value(88, metrics.scale_milli) == 44);
  assert(desktop_metrics_compute(800, 600, 1200, 900, &metrics));
  assert(metrics.scale_milli == 1500);

  DesktopRect display = {0, 0, 1920, 1080};
  assert(desktop_rect_has_visible_area((DesktopRect){100, 100, 1024, 720},
                                       display, 64));
  assert(desktop_rect_has_visible_area((DesktopRect){-960, 100, 1024, 720},
                                       display, 64));
  assert(!desktop_rect_has_visible_area((DesktopRect){-1000, 100, 1024, 720},
                                        display, 64));
  assert(!desktop_rect_has_visible_area((DesktopRect){2500, 100, 1024, 720},
                                        display, 64));
  assert(!desktop_rect_has_visible_area(
      (DesktopRect){INT_MAX - 8, INT_MAX - 8, 1024, 720}, display, 64));
  assert(!desktop_rect_has_visible_area(
      (DesktopRect){INT_MIN + 8, INT_MIN + 8, 1024, 720}, display, 64));

  assert(desktop_tick_at_or_after(100u, 100u));
  assert(desktop_tick_at_or_after(101u, 100u));
  assert(!desktop_tick_at_or_after(99u, 100u));
  assert(desktop_tick_at_or_after(2u, UINT32_MAX - 2u));
  assert(!desktop_tick_at_or_after(UINT32_MAX - 2u, 2u));

  assert(!desktop_window_event_requires_pause(
      DESKTOP_WINDOW_EVENT_FOCUS_LOST, DESKTOP_WINDOW_INPUT_FOCUS));
  assert(desktop_window_event_requires_pause(
      DESKTOP_WINDOW_EVENT_FOCUS_LOST, 0));
  assert(!desktop_window_event_requires_pause(
      DESKTOP_WINDOW_EVENT_MINIMIZED, DESKTOP_WINDOW_INPUT_FOCUS));
  assert(desktop_window_event_requires_pause(
      DESKTOP_WINDOW_EVENT_MINIMIZED, DESKTOP_WINDOW_MINIMIZED));
  assert(!desktop_window_event_requires_pause(
      DESKTOP_WINDOW_EVENT_HIDDEN, DESKTOP_WINDOW_INPUT_FOCUS));
  assert(desktop_window_event_requires_pause(
      DESKTOP_WINDOW_EVENT_HIDDEN, DESKTOP_WINDOW_HIDDEN));
  assert(!desktop_window_event_requires_pause(
      DESKTOP_WINDOW_EVENT_OTHER, 0));
  assert(!desktop_window_event_should_pause(
      99u, 100u, DESKTOP_WINDOW_EVENT_FOCUS_LOST, 0));
  assert(!desktop_window_event_should_pause(
      100u, 100u, DESKTOP_WINDOW_EVENT_FOCUS_LOST,
      DESKTOP_WINDOW_INPUT_FOCUS));
  assert(desktop_window_event_should_pause(
      100u, 100u, DESKTOP_WINDOW_EVENT_FOCUS_LOST, 0));
  assert(desktop_window_event_should_pause(
      2u, UINT32_MAX - 2u, DESKTOP_WINDOW_EVENT_HIDDEN,
      DESKTOP_WINDOW_HIDDEN));

  puts("desktop metrics, visibility and focus-pause policy passed");
  return 0;
}
