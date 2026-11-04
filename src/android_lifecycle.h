#ifndef SUDOKURA_ANDROID_LIFECYCLE_H
#define SUDOKURA_ANDROID_LIFECYCLE_H
#include <stdbool.h>

typedef enum {
  ANDROID_LIFECYCLE_WILL_BACKGROUND,
  ANDROID_LIFECYCLE_DID_BACKGROUND,
  ANDROID_LIFECYCLE_WILL_FOREGROUND,
  ANDROID_LIFECYCLE_DID_FOREGROUND,
  ANDROID_LIFECYCLE_LOW_MEMORY,
  ANDROID_LIFECYCLE_TERMINATING
} AndroidLifecycleEvent;
typedef struct { bool suspended, terminating; } AndroidLifecycle;
typedef struct {
  bool checkpoint, cancel_input, silence_audio, restore_surface;
} AndroidLifecycleEffects;
AndroidLifecycleEffects android_lifecycle_transition(
    AndroidLifecycle *state, AndroidLifecycleEvent event);
#endif
