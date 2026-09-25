#include "app_clock.h"

#include <limits.h>

bool app_clock_running(const AppState *state) {
  return state && state->session_open && state->screen == APP_SCREEN_PLAY &&
         state->pause_reasons == 0;
}

uint64_t app_clock_elapsed_ms(const AppState *state, uint64_t now_ms) {
  if (!state) return 0;
  uint64_t elapsed = state->elapsed_ms;
  if (!app_clock_running(state) || now_ms <= state->running_since_ms)
    return elapsed;

  uint64_t delta = now_ms - state->running_since_ms;
  if (UINT64_MAX - elapsed < delta) return UINT64_MAX;
  return elapsed + delta;
}

double app_clock_elapsed_s(const AppState *state, uint64_t now_ms) {
  return (double)app_clock_elapsed_ms(state, now_ms) / 1000.0;
}

bool app_clock_pause(AppState *state, unsigned reason, uint64_t now_ms) {
  if (!state || !reason || (state->pause_reasons & reason) != 0) return false;
  if (app_clock_running(state)) {
    state->elapsed_ms = app_clock_elapsed_ms(state, now_ms);
    state->running_since_ms = now_ms;
  }
  state->pause_reasons |= reason;
  return true;
}

bool app_clock_resume(AppState *state, unsigned reason, uint64_t now_ms) {
  if (!state || !reason || (state->pause_reasons & reason) == 0) return false;
  state->pause_reasons &= ~reason;
  if (app_clock_running(state)) state->running_since_ms = now_ms;
  return true;
}

void app_clock_reset(AppState *state, uint64_t now_ms) {
  if (!state) return;
  state->elapsed_ms = 0;
  state->running_since_ms = now_ms;
  state->pause_reasons = 0;
}

void app_clock_restore(AppState *state, uint64_t elapsed_ms, uint64_t now_ms) {
  if (!state) return;
  state->elapsed_ms = elapsed_ms;
  state->running_since_ms = now_ms;
}
