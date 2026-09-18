#include "app_clock.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static AppState playing_state(uint64_t now_ms) {
  AppState state;
  app_state_init(&state);
  state.session_open = true;
  state.screen = APP_SCREEN_PLAY;
  state.prev_screen = APP_SCREEN_PLAY;
  app_clock_reset(&state, now_ms);
  return state;
}

static void test_monotonic_64_bit_elapsed(void) {
  AppState state = playing_state(0);
  uint64_t beyond_32_bit = UINT64_C(5000000000);
  assert(app_clock_elapsed_ms(&state, beyond_32_bit) == beyond_32_bit);
  assert(app_clock_elapsed_s(&state, beyond_32_bit) == 5000000.0);
}

static void test_pause_resume_is_idempotent(void) {
  AppState state = playing_state(1000);
  assert(app_clock_elapsed_ms(&state, 2000) == 1000);

  assert(app_clock_pause(&state, APP_PAUSE_MANUAL, 2000));
  assert(state.elapsed_ms == 1000);
  assert(!app_clock_pause(&state, APP_PAUSE_MANUAL, 2500));
  assert(app_clock_elapsed_ms(&state, 3000) == 1000);

  assert(app_clock_pause(&state, APP_PAUSE_FOCUS, 3000));
  assert(!app_clock_resume(&state, APP_PAUSE_MODAL, 3500));
  assert(state.pause_reasons ==
         (APP_PAUSE_MANUAL | APP_PAUSE_FOCUS));
  assert(state.elapsed_ms == 1000);

  assert(app_clock_resume(&state, APP_PAUSE_MANUAL, 4000));
  assert(state.pause_reasons == APP_PAUSE_FOCUS);
  assert(app_clock_elapsed_ms(&state, 4500) == 1000);

  assert(app_clock_resume(&state, APP_PAUSE_FOCUS, 5000));
  assert(state.pause_reasons == 0);
  assert(state.running_since_ms == 5000);
  assert(app_clock_elapsed_ms(&state, 5500) == 1500);
}

static void test_nested_modal_preserves_manual_pause(void) {
  AppState state = playing_state(0);
  assert(app_clock_pause(&state, APP_PAUSE_MANUAL, 1000));
  assert(app_clock_pause(&state, APP_PAUSE_MODAL, 1200));
  assert(app_clock_resume(&state, APP_PAUSE_MODAL, 2000));
  assert(state.pause_reasons == APP_PAUSE_MANUAL);
  assert(app_clock_elapsed_ms(&state, 3000) == 1000);
  assert(app_clock_resume(&state, APP_PAUSE_MANUAL, 4000));
  assert(app_clock_elapsed_ms(&state, 4500) == 1500);
}

static void test_restore_and_reset(void) {
  AppState state = playing_state(0);
  app_clock_restore(&state, UINT64_C(123456), UINT64_C(7000));
  assert(app_clock_elapsed_ms(&state, UINT64_C(8000)) == UINT64_C(124456));

  assert(app_clock_pause(&state, APP_PAUSE_FOCUS, UINT64_C(8000)));
  app_clock_reset(&state, UINT64_C(9000));
  assert(state.elapsed_ms == 0);
  assert(state.pause_reasons == 0);
  assert(state.running_since_ms == UINT64_C(9000));
}

int main(void) {
  test_monotonic_64_bit_elapsed();
  test_pause_resume_is_idempotent();
  test_nested_modal_preserves_manual_pause();
  test_restore_and_reset();
  puts("64-bit application clock and idempotent pause tests passed");
  return 0;
}
