#include "pointer_capture.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
  PointerCapture press = {0};
  PointerTarget a = {1, 7, 0, 0, 10, 20, 80, 40};
  PointerTarget b = {2, 7, 0, 0, 100, 20, 80, 40};
  assert(!pointer_capture_release(&press, a, 20, 30));
  assert(pointer_capture_begin(&press, a, 20, 30));
  assert(pointer_capture_release(&press, a, 20, 30));
  assert(!press.active);
  assert(!pointer_capture_release(&press, a, 20, 30));
  assert(pointer_capture_begin(&press, a, 20, 30));
  pointer_capture_motion(&press, 100, 30);
  assert(!press.inside);
  assert(!pointer_capture_release(&press, b, 110, 30));
  assert(pointer_capture_begin(&press, a, 20, 30));
  pointer_capture_motion(&press, 0, 0);
  pointer_capture_motion(&press, 20, 30);
  assert(press.inside);
  assert(pointer_capture_release(&press, a, 20, 30));
  assert(pointer_capture_begin(&press, a, 20, 30));
  assert(!pointer_capture_release(&press, a, 90, 30));
  assert(pointer_capture_begin(&press, a, 20, 30));
  assert(!pointer_capture_release(&press, (PointerTarget){0}, 20, 30));
  for (int change = 0; change < 5; ++change) {
    PointerTarget changed = a;
    if (change == 0) changed.generation++;
    if (change == 1) changed.key_hi++;
    if (change == 2) changed.key_lo++;
    if (change == 3) changed.x++;
    if (change == 4) changed.w++;
    assert(pointer_capture_begin(&press, a, 20, 30));
    assert(!pointer_capture_release(&press, changed, 20, 30));
  }
  assert(pointer_capture_begin(&press, a, 20, 30));
  pointer_capture_cancel(&press);
  assert(!pointer_capture_release(&press, a, 20, 30));
  assert(!pointer_capture_begin(&press, (PointerTarget){0}, 20, 30));
  a.x = INT_MIN; a.w = INT_MAX;
  assert(!pointer_target_contains(a, INT_MAX, 30));
  puts("pointer ownership, cancellation, reentry and record identity passed");
  return 0;
}
