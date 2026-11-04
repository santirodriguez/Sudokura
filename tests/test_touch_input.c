#include "touch_input.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static TouchSample sample(int64_t touch, int64_t finger, float x, float y) {
  return (TouchSample){touch, finger, 3u, x, y};
}

static TouchInputResult send(TouchInput *in, TouchInputPhase phase,
                             TouchSample s) {
  return touch_input_event(in, phase, s, 3u, 400, 800, 8);
}

int main(void) {
  TouchInput in = {0};
  TouchInputResult r = send(&in, TOUCH_INPUT_DOWN, sample(4, 5, .5f, .5f));
  assert(r.accepted && r.tap && r.x == 200 && r.y == 400 && in.active);
  assert(!send(&in, TOUCH_INPUT_DOWN, sample(4, 6, .4f, .4f)).accepted);
  assert(!send(&in, TOUCH_INPUT_UP, sample(4, 6, .4f, .4f)).accepted);
  assert(send(&in, TOUCH_INPUT_UP, sample(4, 5, .5f, .5f)).tap);
  assert(!in.active);
  assert(!send(&in, TOUCH_INPUT_UP, sample(4, 5, .5f, .5f)).accepted);
  assert(send(&in, TOUCH_INPUT_DOWN, sample(4, 5, .5f, .5f)).accepted);
  r = send(&in, TOUCH_INPUT_MOTION, sample(4, 5, .5025f, .5f));
  assert(r.accepted && r.tap && !r.slop_exceeded);
  r = send(&in, TOUCH_INPUT_MOTION, sample(4, 5, .55f, .5f));
  assert(r.slop_exceeded && !r.tap && r.delta_x > 0);
  r = send(&in, TOUCH_INPUT_UP, sample(4, 5, .5f, .5f));
  assert(r.accepted && !r.tap && !in.active);
  assert(!send(&in, TOUCH_INPUT_DOWN, sample(7, 8, NAN, .5f)).accepted);
  assert(!send(&in, TOUCH_INPUT_DOWN, sample(7, 8, 1.0f, .5f)).accepted);
  assert(!send(&in, TOUCH_INPUT_DOWN, sample(7, 8, -.1f, .5f)).accepted);
  assert(send(&in, TOUCH_INPUT_DOWN, sample(7, 8, .99f, .99f)).accepted);
  assert(!send(&in, TOUCH_INPUT_MOTION, sample(7, 8, 1.2f, .5f)).valid);
  assert(!in.active && !send(&in, TOUCH_INPUT_UP, sample(7, 8, .99f, .99f)).accepted);
  assert(!touch_input_event(&in, TOUCH_INPUT_DOWN, sample(7, 8, .5f, .5f),
                            2u, 400, 800, 8).accepted);

  TouchScroll scroll = {0};
  touch_scroll_begin(&scroll, 30, 300);
  assert(!touch_scroll_motion(&scroll, 31, 296, 8).scrolling);
  assert(!touch_scroll_motion(&scroll, 48, 300, 8).scrolling);
  TouchScrollResult step = touch_scroll_motion(&scroll, 48, 260, 8);
  assert(step.scrolling && step.delta_y == -40);
  assert(touch_scroll_rows(&scroll, step.delta_y, 64) == 0);
  step = touch_scroll_motion(&scroll, 48, 210, 8);
  assert(step.delta_y == -50 && touch_scroll_rows(&scroll, step.delta_y, 64) == -1);
  step = touch_scroll_motion(&scroll, 48, 275, 8);
  assert(touch_scroll_rows(&scroll, step.delta_y, 64) == 0);
  step = touch_scroll_motion(&scroll, 48, 355, 8);
  assert(touch_scroll_rows(&scroll, step.delta_y, 64) == 1);
  touch_scroll_cancel(&scroll);
  assert(!touch_scroll_motion(&scroll, 48, 400, 8).scrolling);
  puts("touch single owner, dedup, slop and bounded row gesture passed");
  return 0;
}
