#ifndef SUDOKURA_BUTTON_FEEDBACK_H
#define SUDOKURA_BUTTON_FEEDBACK_H

#include <stdbool.h>
#include <stdint.h>

/* Keep a short, static pressed state visible even after a fast click.
   No animation loop or gameplay timer is involved. */
enum { BUTTON_FEEDBACK_HOLD_MS = 130 };

typedef struct {
  bool active;
  bool keyboard;
  int screen;
  int x, y, w, h;
  uint64_t started_ms;
} ButtonFeedback;

static inline void button_feedback_pointer(ButtonFeedback *feedback,
                                           int screen, int x, int y,
                                           uint64_t now_ms) {
  if (!feedback) return;
  *feedback = (ButtonFeedback){true, false, screen, x, y, 0, 0, now_ms};
}

static inline void button_feedback_keyboard(ButtonFeedback *feedback,
                                            int screen, int x, int y,
                                            int width, int height,
                                            uint64_t now_ms) {
  if (!feedback || width <= 0 || height <= 0) return;
  *feedback = (ButtonFeedback){true, true, screen, x, y, width, height,
                               now_ms};
}

static inline int button_feedback_remaining_ms(const ButtonFeedback *feedback,
                                               int screen, uint64_t now_ms) {
  if (!feedback || !feedback->active || feedback->screen != screen ||
      now_ms < feedback->started_ms)
    return 0;
  uint64_t elapsed = now_ms - feedback->started_ms;
  return elapsed < BUTTON_FEEDBACK_HOLD_MS
             ? (int)(BUTTON_FEEDBACK_HOLD_MS - elapsed) : 0;
}

static inline bool button_feedback_matches(const ButtonFeedback *feedback,
                                           int screen, uint64_t now_ms,
                                           int x, int y, int width,
                                           int height) {
  if (width <= 0 || height <= 0 ||
      !button_feedback_remaining_ms(feedback, screen, now_ms))
    return false;
  if (feedback->keyboard)
    return feedback->x == x && feedback->y == y &&
           feedback->w == width && feedback->h == height;
  return feedback->x >= x && feedback->y >= y &&
         feedback->x - x < width && feedback->y - y < height;
}

#endif
