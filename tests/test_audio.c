#include "audio.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#ifdef main
#undef main
#endif

#include <assert.h>
#include <stdio.h>

static void init_dummy_audio(unsigned missing_assets) {
  audio_test_set_missing_assets(missing_assets);
  assert(audio_init());
  assert(audio_device_available());
  assert(audio_is_available());
  assert(audio_is_enabled());
}

int main(void) {
  assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
  assert(SDL_Init(0) == 0);

  init_dummy_audio(0);
  assert(audio_music_available());
  assert(audio_fx_available());
  assert(audio_music_volume() == AUDIO_DEFAULT_MUSIC_VOLUME);
  assert(audio_fx_volume() == AUDIO_DEFAULT_FX_VOLUME);
  assert(!audio_music_muted());
  assert(!audio_fx_muted());

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

  audio_set_music_volume(35);
  audio_set_music_muted(true);
  assert(audio_music_muted());
  assert(audio_music_volume() == 35);
  assert(!Mix_PlayingMusic());
  audio_set_music_muted(false);
  audio_update();
  assert(!audio_music_muted());
  assert(audio_music_volume() == 35);
  assert(Mix_PlayingMusic());

  audio_set_fx_volume(80);
  audio_set_fx_muted(true);
  assert(audio_fx_muted());
  assert(audio_fx_volume() == 80);
  Mix_HaltChannel(-1);
  audio_play_effect(AUDIO_EFFECT_CLICK);
  assert(Mix_Playing(-1) == 0);
  audio_set_fx_muted(false);
  assert(!audio_fx_muted());
  assert(audio_fx_volume() == 80);
  audio_play_effect(AUDIO_EFFECT_CLICK);
  assert(Mix_Playing(-1) > 0);
  Mix_HaltChannel(-1);
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

  audio_set_music_volume(37);
  audio_set_fx_volume(73);
  audio_set_music_muted(true);
  audio_set_fx_muted(true);
  audio_set_enabled(false);
  audio_test_force_device_loss();
  assert(!audio_device_available());
  assert(!audio_is_available());
  assert(!audio_is_enabled());
  audio_test_retry_now();
  audio_update();
  assert(audio_device_available());
  assert(audio_is_available());
  assert(!audio_is_enabled());
  assert(audio_music_volume() == 37);
  assert(audio_fx_volume() == 73);
  assert(audio_music_muted());
  assert(audio_fx_muted());
  audio_set_enabled(true);
  audio_update();
  assert(!Mix_PlayingMusic());
  audio_set_music_muted(false);
  audio_set_fx_muted(false);
  audio_update();
  assert(Mix_PlayingMusic());
  audio_shutdown();

  init_dummy_audio(AUDIO_TEST_MISSING_MAIN_MUSIC);
  assert(audio_music_available());
  assert(audio_fx_available());
  audio_set_context(AUDIO_CONTEXT_MAIN);
  audio_update();
  assert(!Mix_PlayingMusic());
  audio_play_effect(AUDIO_EFFECT_CLICK);
  audio_set_context(AUDIO_CONTEXT_FAIL);
  audio_update();
  assert(Mix_PlayingMusic());
  audio_shutdown();

  init_dummy_audio(AUDIO_TEST_MISSING_ALL_OGG);
  assert(!audio_music_available());
  assert(audio_fx_available());
  audio_set_context(AUDIO_CONTEXT_MAIN);
  audio_update();
  assert(!Mix_PlayingMusic());
  audio_play_effect(AUDIO_EFFECT_CLICK);
  audio_play_result(AUDIO_RESULT_WIN);
  audio_cancel_result();
  assert(audio_is_available());
  audio_shutdown();

  audio_test_set_missing_assets(0);
  SDL_Quit();
  puts("SDL_mixer audio tests passed for master/channel mute, levels, device recovery and optional resources");
  return 0;
}
