#include "touch_input.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

void touch_input_cancel(TouchInput *input) {
  if (input) *input = (TouchInput){0};
}

static bool touch_position(TouchSample sample, int width, int height,
                           int *x, int *y) {
  if (width <= 0 || height <= 0 || !isfinite(sample.x) || !isfinite(sample.y) ||
      sample.x < 0.0f || sample.x >= 1.0f || sample.y < 0.0f ||
      sample.y >= 1.0f)
    return false;
  double px = (double)sample.x * width;
  double py = (double)sample.y * height;
  if (px < 0.0 || py < 0.0 || px >= width || py >= height) return false;
  *x = (int)px;
  *y = (int)py;
  return true;
}

TouchInputResult touch_input_event(TouchInput *input, TouchInputPhase phase,
                                   TouchSample sample, uint32_t window_id,
                                   int width, int height, int slop) {
  TouchInputResult out = {0};
  if (!input || sample.window_id != window_id || window_id == 0) return out;
  int x = 0, y = 0;
  bool valid = touch_position(sample, width, height, &x, &y);
  if (phase == TOUCH_INPUT_DOWN) {
    if (input->active || !valid) return out;
    *input = (TouchInput){.active = true,
                          .touch_id = sample.touch_id,
                          .finger_id = sample.finger_id,
                          .window_id = sample.window_id,
                          .start_x = x, .start_y = y,
                          .last_x = x, .last_y = y};
    out.accepted = out.valid = true;
    out.x = x;
    out.y = y;
    out.tap = true;
    return out;
  }
  if (!input->active || input->window_id != sample.window_id ||
      input->touch_id != sample.touch_id ||
      input->finger_id != sample.finger_id)
    return out;
  out.accepted = true;
  out.valid = valid;
  if (!valid) {
    touch_input_cancel(input);
    return out;
  }
  out.x = x;
  out.y = y;
  out.delta_x = x - input->last_x;
  out.delta_y = y - input->last_y;
  int64_t dx = (int64_t)x - input->start_x;
  int64_t dy = (int64_t)y - input->start_y;
  int64_t threshold = slop > 0 ? slop : 1;
  /* Only square bounded components; enormous logical sizes cannot overflow. */
  if (llabs(dx) > threshold || llabs(dy) > threshold ||
      dx * dx + dy * dy > threshold * threshold)
    input->moved = true;
  out.slop_exceeded = input->moved;
  out.tap = !input->moved;
  input->last_x = x;
  input->last_y = y;
  if (phase == TOUCH_INPUT_UP) touch_input_cancel(input);
  return out;
}

void touch_scroll_begin(TouchScroll *scroll, int x, int y) {
  if (scroll) *scroll = (TouchScroll){.active = true, .start_x = x,
                                      .start_y = y, .last_y = y};
}

void touch_scroll_cancel(TouchScroll *scroll) {
  if (scroll) *scroll = (TouchScroll){0};
}

TouchScrollResult touch_scroll_motion(TouchScroll *scroll, int x, int y,
                                      int slop) {
  TouchScrollResult out = {0};
  if (!scroll || !scroll->active) return out;
  if (!scroll->dragging) {
    int64_t dx = (int64_t)x - scroll->start_x;
    int64_t dy = (int64_t)y - scroll->start_y;
    int64_t limit = slop > 0 ? slop : 1;
    if ((llabs(dx) <= limit && llabs(dy) <= limit &&
         dx * dx + dy * dy <= limit * limit) ||
        llabs(dy) <= llabs(dx))
      return out;
    scroll->dragging = true;
  }
  out.scrolling = true;
  out.delta_y = y - scroll->last_y;
  scroll->last_y = y;
  return out;
}

int touch_scroll_rows(TouchScroll *scroll, int delta_y, int row_height) {
  if (!scroll || !scroll->active || !scroll->dragging || row_height <= 0)
    return 0;
  int64_t total = (int64_t)scroll->remainder + delta_y;
  int64_t steps = total / row_height;
  scroll->remainder = (int)(total % row_height);
  if (steps > INT_MAX) return INT_MAX;
  if (steps < INT_MIN) return INT_MIN;
  return (int)steps;
}
