#ifndef SUDOKURA_AUDIO_H
#define SUDOKURA_AUDIO_H

#include <stdbool.h>

#define AUDIO_DEFAULT_MUSIC_VOLUME 20
#define AUDIO_DEFAULT_FX_VOLUME 65

typedef enum {
  AUDIO_CONTEXT_MAIN = 0,
  AUDIO_CONTEXT_FAIL = 1
} AudioContext;

typedef enum {
  AUDIO_RESULT_WIN = 0,
  AUDIO_RESULT_FAIL = 1
} AudioResultCue;

typedef enum {
  AUDIO_EFFECT_CLICK = 0,
  AUDIO_EFFECT_POSITIVE = 1,
  AUDIO_EFFECT_NEGATIVE = 2,
  AUDIO_EFFECT_NEUTRAL = 3,
  AUDIO_EFFECT_BLOCKED = 4,
  AUDIO_EFFECT_START = 5,
  AUDIO_EFFECT_LEAVE = 6,
  AUDIO_EFFECT_COUNT = 7
} AudioEffect;

bool audio_init(void);
void audio_shutdown(void);
bool audio_is_available(void);
bool audio_is_enabled(void);
void audio_set_enabled(bool enabled);
void audio_set_context(AudioContext context);
void audio_play_result(AudioResultCue cue);
void audio_play_effect(AudioEffect effect);
void audio_cancel_result(void);
void audio_set_focus_paused(bool paused);
void audio_update(void);
int audio_music_volume(void);
int audio_fx_volume(void);
void audio_set_music_volume(int percent);
void audio_set_fx_volume(int percent);

#endif
