#include "android_lifecycle.h"

AndroidLifecycleEffects android_lifecycle_transition(
    AndroidLifecycle *state, AndroidLifecycleEvent event) {
  AndroidLifecycleEffects effects = {0};
  if (!state) return effects;
  switch (event) {
    case ANDROID_LIFECYCLE_WILL_BACKGROUND:
    case ANDROID_LIFECYCLE_DID_BACKGROUND:
      if (!state->suspended && !state->terminating) {
        state->suspended = true;
        effects.checkpoint = true;
        effects.cancel_input = true;
        effects.silence_audio = true;
      }
      break;
    case ANDROID_LIFECYCLE_WILL_FOREGROUND: break;
    case ANDROID_LIFECYCLE_DID_FOREGROUND:
      if (state->suspended && !state->terminating) {
        state->suspended = false;
        effects.restore_surface = true;
      }
      break;
    case ANDROID_LIFECYCLE_LOW_MEMORY:
      if (!state->suspended && !state->terminating)
        effects.checkpoint = true;
      break;
    case ANDROID_LIFECYCLE_TERMINATING:
      if (!state->terminating) {
        if (!state->suspended) {
          effects.checkpoint = true;
          effects.cancel_input = true;
          effects.silence_audio = true;
        }
        state->suspended = true;
        state->terminating = true;
      }
      break;
  }
  return effects;
}
