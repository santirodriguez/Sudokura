#include "audio.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
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
  assert(audio_music_volume() == AUDIO_DEFAULT_MUSIC_VOLUME);
  assert(audio_fx_volume() == AUDIO_DEFAULT_FX_VOLUME);

  audio_set_music_volume(35);
  audio_set_fx_volume(80);
  assert(audio_music_volume() == 35);
  assert(audio_fx_volume() == 80);
  audio_set_music_volume(-5);
  audio_set_fx_volume(130);
  assert(audio_music_volume() == 0);
  assert(audio_fx_volume() == 100);
  audio_set_music_volume(AUDIO_DEFAULT_MUSIC_VOLUME);
  audio_set_fx_volume(AUDIO_DEFAULT_FX_VOLUME);

  audio_set_context(AUDIO_CONTEXT_MAIN);
  audio_update();
  assert(Mix_PlayingMusic());
  for (int effect = 0; effect < AUDIO_EFFECT_COUNT; ++effect)
    audio_play_effect((AudioEffect)effect);

  audio_set_context(AUDIO_CONTEXT_FAIL);
  audio_update();
  assert(Mix_PlayingMusic());
  audio_play_result(AUDIO_RESULT_FAIL);
  assert(!Mix_PlayingMusic());
  audio_cancel_result();
  assert(Mix_PlayingMusic());

  audio_set_context(AUDIO_CONTEXT_MAIN);
  audio_update();
  assert(Mix_PlayingMusic());

  audio_play_result(AUDIO_RESULT_WIN);
  assert(!Mix_PlayingMusic());
  audio_cancel_result();
  assert(Mix_PlayingMusic());

  audio_set_focus_paused(true);
  assert(Mix_PausedMusic());
  audio_set_focus_paused(false);
  audio_update();
  assert(Mix_PlayingMusic());

  audio_set_enabled(false);
  assert(!audio_is_enabled());
  assert(!Mix_PlayingMusic());
  audio_set_enabled(true);
  assert(audio_is_enabled());
  audio_update();
  assert(Mix_PlayingMusic());

  audio_shutdown();
  SDL_Quit();
  puts("SDL_mixer audio tests passed for volumes, result cues, contexts, pause and mute recovery");
  return 0;
}
