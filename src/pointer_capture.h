#ifndef SUDOKURA_POINTER_CAPTURE_H
#define SUDOKURA_POINTER_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

/* A target is resolved from the current screen before arming or releasing.
   Record keys distinguish library entries even when they occupy the same row. */
typedef struct {
  int action;
  uint64_t generation, key_hi, key_lo;
  int x, y, w, h;
} PointerTarget;

typedef struct {
  bool active, inside;
  PointerTarget target;
} PointerCapture;

static inline bool pointer_target_contains(PointerTarget t, int x, int y) {
  return t.action != 0 && t.w > 0 && t.h > 0 &&
         (int64_t)x >= t.x && (int64_t)y >= t.y &&
         (int64_t)x - t.x < t.w && (int64_t)y - t.y < t.h;
}

static inline bool pointer_target_equal(PointerTarget a, PointerTarget b) {
  return a.action == b.action && a.generation == b.generation &&
         a.key_hi == b.key_hi && a.key_lo == b.key_lo &&
         a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static inline void pointer_capture_cancel(PointerCapture *press) {
  if (press) *press = (PointerCapture){0};
}

static inline bool pointer_capture_begin(PointerCapture *press,
                                         PointerTarget target, int x, int y) {
  if (!press) return false;
  pointer_capture_cancel(press);
  if (!pointer_target_contains(target, x, y)) return false;
  *press = (PointerCapture){true, true, target};
  return true;
}

static inline void pointer_capture_motion(PointerCapture *press,
                                          int x, int y) {
  if (press && press->active)
    press->inside = pointer_target_contains(press->target, x, y);
}

/* Invalidate before the caller dispatches: dialogs and callbacks may reenter. */
static inline bool pointer_capture_release(PointerCapture *press,
                                           PointerTarget current,
                                           int x, int y) {
  if (!press) return false;
  bool activate = press->active &&
                  pointer_target_equal(press->target, current) &&
                  pointer_target_contains(current, x, y);
  pointer_capture_cancel(press);
  return activate;
}

#endif
