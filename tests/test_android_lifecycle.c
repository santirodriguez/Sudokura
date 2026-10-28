#include "android_lifecycle.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  AndroidLifecycle state = {0};
  AndroidLifecycleEffects e;
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_WILL_BACKGROUND);
  assert(state.suspended && !state.terminating);
  assert(e.checkpoint && e.cancel_input && e.silence_audio && !e.restore_surface);
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_DID_BACKGROUND);
  assert(!e.checkpoint && !e.cancel_input && !e.silence_audio);
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_WILL_FOREGROUND);
  assert(state.suspended && !e.restore_surface);
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_DID_FOREGROUND);
  assert(!state.suspended && e.restore_surface && !e.checkpoint);
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_LOW_MEMORY);
  assert(e.checkpoint && !e.silence_audio);
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_DID_FOREGROUND);
  assert(!e.restore_surface);
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_TERMINATING);
  assert(state.suspended && state.terminating && e.checkpoint);
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_DID_FOREGROUND);
  assert(state.suspended && !e.restore_surface);
  e = android_lifecycle_transition(&state, ANDROID_LIFECYCLE_TERMINATING);
  assert(!e.checkpoint);
  e = android_lifecycle_transition(NULL, ANDROID_LIFECYCLE_LOW_MEMORY);
  assert(!e.checkpoint);
  puts("Android lifecycle transitions passed");
  return 0;
}
