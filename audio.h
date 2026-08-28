#ifndef SUDOKURA_AUDIO_H
#define SUDOKURA_AUDIO_H

#include <stdbool.h>

typedef enum {
  AUDIO_CONTEXT_MAIN = 0,
  AUDIO_CONTEXT_FAIL = 1
} AudioContext;

typedef enum {
  AUDIO_RESULT_WIN = 0,
  AUDIO_RESULT_FAIL = 1
} AudioResultCue;

bool audio_init(void);
void audio_shutdown(void);
bool audio_is_available(void);
bool audio_is_enabled(void);
void audio_set_enabled(bool enabled);
void audio_set_context(AudioContext context);
void audio_play_result(AudioResultCue cue);
void audio_cancel_result(void);
void audio_set_focus_paused(bool paused);
void audio_update(void);

#endif
