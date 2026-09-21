#ifndef SUDOKURA_APP_CLOCK_H
#define SUDOKURA_APP_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "app.h"

bool app_clock_running(const AppState *state);
uint64_t app_clock_elapsed_ms(const AppState *state, uint64_t now_ms);
double app_clock_elapsed_s(const AppState *state, uint64_t now_ms);
bool app_clock_pause(AppState *state, unsigned reason, uint64_t now_ms);
bool app_clock_resume(AppState *state, unsigned reason, uint64_t now_ms);
void app_clock_reset(AppState *state, uint64_t now_ms);
void app_clock_restore(AppState *state, uint64_t elapsed_ms, uint64_t now_ms);

#endif
