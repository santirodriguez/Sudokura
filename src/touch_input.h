#ifndef SUDOKURA_TOUCH_INPUT_H
#define SUDOKURA_TOUCH_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  TOUCH_INPUT_DOWN,
  TOUCH_INPUT_MOTION,
  TOUCH_INPUT_UP
} TouchInputPhase;

typedef struct {
  int64_t touch_id, finger_id;
  uint32_t window_id;
  float x, y; /* SDL renderer-adjusted normalized logical coordinates. */
} TouchSample;

typedef struct {
  bool active, moved;
  int64_t touch_id, finger_id;
  uint32_t window_id;
  int start_x, start_y, last_x, last_y;
} TouchInput;

typedef struct {
  bool accepted, valid, tap, slop_exceeded;
  int x, y, delta_x, delta_y;
} TouchInputResult;

void touch_input_cancel(TouchInput *input);
TouchInputResult touch_input_event(TouchInput *input, TouchInputPhase phase,
                                   TouchSample sample, uint32_t window_id,
                                   int width, int height, int slop);

/* One bounded drag policy is shared by Help and Saved Games, including mice. */
typedef struct {
  bool active, dragging;
  int start_x, start_y, last_y, remainder;
} TouchScroll;

typedef struct {
  bool scrolling;
  int delta_y;
} TouchScrollResult;

void touch_scroll_begin(TouchScroll *scroll, int x, int y);
void touch_scroll_cancel(TouchScroll *scroll);
TouchScrollResult touch_scroll_motion(TouchScroll *scroll, int x, int y,
                                      int slop);
/* Positive drag (down) moves to earlier rows; negative drag moves later. */
int touch_scroll_rows(TouchScroll *scroll, int delta_y, int row_height);

#endif
