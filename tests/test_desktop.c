#include "desktop.h"

#include <assert.h>
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

  puts("desktop HiDPI metrics and window visibility policy passed");
  return 0;
}
