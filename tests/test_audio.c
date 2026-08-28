#include "audio.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#ifdef main
#undef main
#endif

#include <assert.h>
#include <stdio.h>

int main(void) {
  assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
  assert(SDL_Init(0) == 0);

  assert(audio_init());
  assert(audio_is_available());
  assert(audio_is_enabled());

  audio_set_context(AUDIO_CONTEXT_MAIN);
  for (int effect = 0; effect < AUDIO_EFFECT_COUNT; ++effect)
    audio_play_effect((AudioEffect)effect);
  audio_play_result(AUDIO_RESULT_WIN);
  audio_update();

  audio_set_enabled(false);
  assert(!audio_is_enabled());
  audio_play_effect(AUDIO_EFFECT_CLICK);
  audio_set_enabled(true);
  assert(audio_is_enabled());

  audio_set_context(AUDIO_CONTEXT_FAIL);
  audio_play_effect(AUDIO_EFFECT_NEGATIVE);
  audio_play_result(AUDIO_RESULT_FAIL);
  audio_cancel_result();
  audio_set_focus_paused(true);
  audio_play_effect(AUDIO_EFFECT_CLICK);
  audio_set_focus_paused(false);
  audio_play_effect(AUDIO_EFFECT_POSITIVE);
  audio_update();

  audio_shutdown();
  SDL_Quit();
  puts("SDL_mixer audio tests passed for music, jingles and generated UI effects");
  return 0;
}
