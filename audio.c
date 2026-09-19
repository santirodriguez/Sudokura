#include "audio.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  AUDIO_PATH_CAPACITY = 4096,
  AUDIO_SAMPLE_RATE = 44100,
  AUDIO_REOPEN_DELAY_MS = 2000,
  AUDIO_DEVICE_EVENT_REOPEN_MS = 250
};

typedef struct {
  bool initialized;
  bool subsystem_initialized;
  bool mixer_initialized;
  bool mixer_open;
  bool assets_loaded;
  bool enabled;
  bool focus_paused;
  bool subsystem_error_logged;
  bool device_error_logged;
  bool ogg_error_logged;
  bool music_error_logged;
  bool effect_error_logged;
  Uint64 reopen_at_ms;
  AudioContext context;
  AudioContext playing_context;
  bool playing_context_valid;
  int pending_channel;
  int music_volume;
  int fx_volume;
  Mix_Music *main_loop;
  Mix_Music *fail_loop;
  Mix_Chunk *win_jingle;
  Mix_Chunk *fail_jingle;
  Mix_Chunk *effects[AUDIO_EFFECT_COUNT];
  Uint8 *effect_buffers[AUDIO_EFFECT_COUNT];
} AudioState;

static AudioState audio_state = {
    .enabled = true,
    .pending_channel = -1,
    .music_volume = AUDIO_DEFAULT_MUSIC_VOLUME,
    .fx_volume = AUDIO_DEFAULT_FX_VOLUME,
};

#ifdef SUDOKURA_AUDIO_TESTING
static unsigned audio_test_missing_assets = 0;
#endif

static int audio_clamp_percent(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

static void audio_apply_volumes(void) {
  if (!audio_state.mixer_open) return;
  Mix_VolumeMusic(MIX_MAX_VOLUME * audio_state.music_volume / 100);
  Mix_Volume(-1, MIX_MAX_VOLUME * audio_state.fx_volume / 100);
}

static bool audio_file_exists(const char *path) {
  SDL_RWops *file = SDL_RWFromFile(path, "rb");
  if (!file) return false;
  SDL_RWclose(file);
  return true;
}

static bool audio_join(char *out, size_t out_size, const char *root,
                       const char *leaf) {
  if (!out || out_size == 0 || !root || !leaf) return false;
  size_t root_length = strlen(root);
  const char *separator =
      root_length && (root[root_length - 1] == '/' || root[root_length - 1] == '\\')
          ? ""
          : "/";
  int written = snprintf(out, out_size, "%s%s%s", root, separator, leaf);
  return written >= 0 && (size_t)written < out_size;
}

static bool audio_candidate(char *out, size_t out_size, const char *root,
                            const char *leaf) {
  return audio_join(out, out_size, root, leaf) && audio_file_exists(out);
}

static bool audio_resolve(char out[AUDIO_PATH_CAPACITY], const char *filename) {
  const char *override = getenv("SUDOKURA_AUDIO_DIR");
  if (override && override[0] &&
      audio_candidate(out, AUDIO_PATH_CAPACITY, override, filename))
    return true;

  char *base = SDL_GetBasePath();
  if (base) {
    char root[AUDIO_PATH_CAPACITY];
    if (audio_join(root, sizeof(root), base, "audio") &&
        audio_candidate(out, AUDIO_PATH_CAPACITY, root, filename)) {
      SDL_free(base);
      return true;
    }
    if (audio_join(root, sizeof(root), base, "assets/audio") &&
        audio_candidate(out, AUDIO_PATH_CAPACITY, root, filename)) {
      SDL_free(base);
      return true;
    }
    if (audio_join(root, sizeof(root), base, "../Resources/audio") &&
        audio_candidate(out, AUDIO_PATH_CAPACITY, root, filename)) {
      SDL_free(base);
      return true;
    }
    SDL_free(base);
  }

  return audio_candidate(out, AUDIO_PATH_CAPACITY, "assets/audio", filename);
}

static void audio_free_effects(void) {
  for (int i = 0; i < AUDIO_EFFECT_COUNT; ++i) {
    if (audio_state.effects[i]) Mix_FreeChunk(audio_state.effects[i]);
    if (audio_state.effect_buffers[i]) SDL_free(audio_state.effect_buffers[i]);
    audio_state.effects[i] = NULL;
    audio_state.effect_buffers[i] = NULL;
  }
}

static void audio_free_assets(void) {
  if (audio_state.main_loop) Mix_FreeMusic(audio_state.main_loop);
  if (audio_state.fail_loop) Mix_FreeMusic(audio_state.fail_loop);
  if (audio_state.win_jingle) Mix_FreeChunk(audio_state.win_jingle);
  if (audio_state.fail_jingle) Mix_FreeChunk(audio_state.fail_jingle);
  audio_state.main_loop = NULL;
  audio_state.fail_loop = NULL;
  audio_state.win_jingle = NULL;
  audio_state.fail_jingle = NULL;
  audio_free_effects();
  audio_state.assets_loaded = false;
}

static bool audio_make_effect(AudioEffect effect, int start_hz, int end_hz,
                              int duration_ms, int relative_volume) {
  if (effect < 0 || effect >= AUDIO_EFFECT_COUNT || duration_ms <= 0) return false;
  int frames = AUDIO_SAMPLE_RATE * duration_ms / 1000;
  if (frames < 2) frames = 2;
  size_t samples_count = (size_t)frames * 2u;
  size_t bytes = samples_count * sizeof(Sint16);
  if (bytes > UINT32_MAX) return false;
  Sint16 *samples = (Sint16 *)SDL_malloc(bytes);
  if (!samples) return false;

  double phase = 0.0;
  const double tau = 6.28318530717958647692;
  for (int frame = 0; frame < frames; ++frame) {
    double position = (double)frame / (double)(frames - 1);
    double attack = position < 0.12 ? position / 0.12 : 1.0;
    double release = 1.0 - position;
    double envelope = attack * release * release;
    double frequency = start_hz + (end_hz - start_hz) * position;
    phase += tau * frequency / AUDIO_SAMPLE_RATE;
    Sint16 sample = (Sint16)(sin(phase) * envelope * 12000.0);
    samples[frame * 2] = sample;
    samples[frame * 2 + 1] = sample;
  }

  Mix_Chunk *chunk = Mix_QuickLoad_RAW((Uint8 *)samples, (Uint32)bytes);
  if (!chunk) {
    SDL_free(samples);
    return false;
  }
  Mix_VolumeChunk(chunk,
                  MIX_MAX_VOLUME * audio_clamp_percent(relative_volume) / 100);
  audio_state.effects[effect] = chunk;
  audio_state.effect_buffers[effect] = (Uint8 *)samples;
  return true;
}

static void audio_build_effects(void) {
  const struct {
    AudioEffect effect;
    int start_hz, end_hz, duration_ms, relative_volume;
  } specs[] = {
      {AUDIO_EFFECT_CLICK, 560, 480, 35, 85},
      {AUDIO_EFFECT_POSITIVE, 660, 880, 70, 95},
      {AUDIO_EFFECT_NEGATIVE, 330, 220, 80, 95},
      {AUDIO_EFFECT_NEUTRAL, 440, 460, 45, 80},
      {AUDIO_EFFECT_BLOCKED, 190, 150, 55, 85},
      {AUDIO_EFFECT_START, 520, 820, 95, 90},
      {AUDIO_EFFECT_LEAVE, 520, 300, 85, 80},
  };
  for (unsigned i = 0; i < sizeof(specs) / sizeof(specs[0]); ++i) {
    if (audio_state.effects[specs[i].effect]) continue;
    if (!audio_make_effect(specs[i].effect, specs[i].start_hz, specs[i].end_hz,
                           specs[i].duration_ms, specs[i].relative_volume))
      fprintf(stderr, "audio effect generation failed: %s\n", Mix_GetError());
  }
}

static bool audio_test_asset_missing(unsigned bit) {
#ifdef SUDOKURA_AUDIO_TESTING
  return (audio_test_missing_assets & bit) != 0;
#else
  (void)bit;
  return false;
#endif
}

static Mix_Music *audio_load_music_asset(const char *filename,
                                         unsigned test_missing_bit) {
  if (audio_test_asset_missing(test_missing_bit)) return NULL;
  char path[AUDIO_PATH_CAPACITY];
  if (!audio_resolve(path, filename)) {
    fprintf(stderr, "audio music asset missing: %s\n", filename);
    return NULL;
  }
  Mix_Music *music = Mix_LoadMUS(path);
  if (!music)
    fprintf(stderr, "audio music asset unavailable (%s): %s\n", filename,
            Mix_GetError());
  return music;
}

static Mix_Chunk *audio_load_chunk_asset(const char *filename,
                                         unsigned test_missing_bit) {
  if (audio_test_asset_missing(test_missing_bit)) return NULL;
  char path[AUDIO_PATH_CAPACITY];
  if (!audio_resolve(path, filename)) {
    fprintf(stderr, "audio jingle asset missing: %s\n", filename);
    return NULL;
  }
  Mix_Chunk *chunk = Mix_LoadWAV(path);
  if (!chunk)
    fprintf(stderr, "audio jingle asset unavailable (%s): %s\n", filename,
            Mix_GetError());
  return chunk;
}

static void audio_load_assets(void) {
  if (audio_state.assets_loaded || !audio_state.mixer_open) return;
  audio_state.assets_loaded = true;

  int codecs = Mix_Init(MIX_INIT_OGG);
  if (!audio_state.mixer_initialized) audio_state.mixer_initialized = true;
  bool ogg_ready = (codecs & MIX_INIT_OGG) != 0;
  if (!ogg_ready && !audio_state.ogg_error_logged) {
    fprintf(stderr,
            "audio OGG resources unavailable; generated effects remain enabled: %s\n",
            Mix_GetError());
    audio_state.ogg_error_logged = true;
  }

  if (ogg_ready) {
    audio_state.main_loop =
        audio_load_music_asset("music-main.ogg", 1u << 0);
    audio_state.fail_loop =
        audio_load_music_asset("music-fail.ogg", 1u << 1);
    audio_state.win_jingle =
        audio_load_chunk_asset("jingle-win.ogg", 1u << 2);
    audio_state.fail_jingle =
        audio_load_chunk_asset("jingle-fail.ogg", 1u << 3);
  }

  if (audio_state.win_jingle)
    Mix_VolumeChunk(audio_state.win_jingle, MIX_MAX_VOLUME);
  if (audio_state.fail_jingle)
    Mix_VolumeChunk(audio_state.fail_jingle, MIX_MAX_VOLUME);
  audio_build_effects();
}

static bool audio_has_music_resource(void) {
  return audio_state.main_loop != NULL || audio_state.fail_loop != NULL;
}

static bool audio_has_fx_resource(void) {
  if (audio_state.win_jingle || audio_state.fail_jingle) return true;
  for (int i = 0; i < AUDIO_EFFECT_COUNT; ++i)
    if (audio_state.effects[i]) return true;
  return false;
}

static bool audio_has_any_resource(void) {
  return audio_has_music_resource() || audio_has_fx_resource();
}

static void audio_schedule_reopen(Uint64 delay_ms) {
  audio_state.reopen_at_ms = SDL_GetTicks64() + delay_ms;
}

static void audio_close_device(void) {
  if (!audio_state.mixer_open) return;
  Mix_HaltChannel(-1);
  Mix_HaltMusic();
  Mix_CloseAudio();
  audio_state.mixer_open = false;
  audio_state.pending_channel = -1;
  audio_state.playing_context_valid = false;
}

static bool audio_prepare_subsystem(void) {
  if (audio_state.subsystem_initialized) return true;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    if (!audio_state.subsystem_error_logged) {
      fprintf(stderr, "audio device unavailable; continuing silently: %s\n",
              SDL_GetError());
      audio_state.subsystem_error_logged = true;
    }
    audio_schedule_reopen(AUDIO_REOPEN_DELAY_MS);
    return false;
  }
  audio_state.subsystem_initialized = true;
  audio_state.subsystem_error_logged = false;
  return true;
}

static bool audio_open_device(void) {
  if (audio_state.mixer_open) return true;
  if (!audio_prepare_subsystem()) return false;
  if (Mix_OpenAudio(AUDIO_SAMPLE_RATE, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
    if (!audio_state.device_error_logged) {
      fprintf(stderr, "audio device unavailable; continuing silently: %s\n",
              Mix_GetError());
      audio_state.device_error_logged = true;
    }
    audio_schedule_reopen(AUDIO_REOPEN_DELAY_MS);
    return false;
  }
  audio_state.mixer_open = true;
  audio_state.device_error_logged = false;
  audio_load_assets();
  audio_apply_volumes();
  return true;
}

static Mix_Music *audio_context_music(AudioContext context) {
  return context == AUDIO_CONTEXT_FAIL ? audio_state.fail_loop
                                       : audio_state.main_loop;
}

static void audio_start_music(void) {
  if (!audio_state.mixer_open || !audio_state.enabled ||
      audio_state.focus_paused || audio_state.pending_channel >= 0)
    return;

  if (audio_state.playing_context_valid &&
      audio_state.playing_context == audio_state.context && Mix_PlayingMusic())
    return;

  Mix_Music *music = audio_context_music(audio_state.context);
  if (!music) return;
  Mix_HaltMusic();
  if (Mix_PlayMusic(music, -1) == 0) {
    audio_state.playing_context = audio_state.context;
    audio_state.playing_context_valid = true;
    audio_state.music_error_logged = false;
  } else {
    if (!audio_state.music_error_logged) {
      fprintf(stderr, "audio music playback unavailable: %s\n", Mix_GetError());
      audio_state.music_error_logged = true;
    }
    audio_state.playing_context_valid = false;
  }
}

bool audio_init(void) {
  if (audio_state.initialized) return audio_is_available();
  audio_state.initialized = true;
  audio_state.enabled = true;
  audio_state.context = AUDIO_CONTEXT_MAIN;
  audio_state.pending_channel = -1;
  audio_state.music_volume = AUDIO_DEFAULT_MUSIC_VOLUME;
  audio_state.fx_volume = AUDIO_DEFAULT_FX_VOLUME;
  audio_state.reopen_at_ms = 0;
  (void)audio_open_device();
  return audio_is_available();
}

void audio_shutdown(void) {
  if (!audio_state.initialized && !audio_state.subsystem_initialized) return;
  if (audio_state.mixer_open) {
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
  }
  audio_free_assets();
  if (audio_state.mixer_open) Mix_CloseAudio();
  if (audio_state.mixer_initialized) Mix_Quit();
  if (audio_state.subsystem_initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
  memset(&audio_state, 0, sizeof(audio_state));
  audio_state.enabled = true;
  audio_state.pending_channel = -1;
  audio_state.music_volume = AUDIO_DEFAULT_MUSIC_VOLUME;
  audio_state.fx_volume = AUDIO_DEFAULT_FX_VOLUME;
}

bool audio_is_available(void) {
  return audio_state.mixer_open && audio_has_any_resource();
}

bool audio_device_available(void) { return audio_state.mixer_open; }

bool audio_music_available(void) {
  return audio_state.mixer_open && audio_has_music_resource();
}

bool audio_fx_available(void) {
  return audio_state.mixer_open && audio_has_fx_resource();
}

bool audio_is_enabled(void) { return audio_state.enabled; }

int audio_music_volume(void) { return audio_state.music_volume; }

int audio_fx_volume(void) { return audio_state.fx_volume; }

void audio_set_music_volume(int percent) {
  audio_state.music_volume = audio_clamp_percent(percent);
  audio_apply_volumes();
}

void audio_set_fx_volume(int percent) {
  audio_state.fx_volume = audio_clamp_percent(percent);
  audio_apply_volumes();
}

void audio_set_enabled(bool enabled) {
  if (audio_state.enabled == enabled) return;
  audio_state.enabled = enabled;
  if (!audio_state.mixer_open) return;
  if (!enabled) {
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    audio_state.pending_channel = -1;
    audio_state.playing_context_valid = false;
    return;
  }
  audio_start_music();
}

void audio_set_context(AudioContext context) {
  if (context != AUDIO_CONTEXT_MAIN && context != AUDIO_CONTEXT_FAIL)
    context = AUDIO_CONTEXT_MAIN;
  if (audio_state.context == context) {
    audio_start_music();
    return;
  }
  audio_state.context = context;
  audio_state.playing_context_valid = false;
  if (audio_state.pending_channel < 0) audio_start_music();
}

void audio_play_result(AudioResultCue cue) {
  if (!audio_state.mixer_open || !audio_state.enabled) return;
  Mix_Chunk *chunk = cue == AUDIO_RESULT_FAIL ? audio_state.fail_jingle
                                              : audio_state.win_jingle;
  audio_state.context =
      cue == AUDIO_RESULT_FAIL ? AUDIO_CONTEXT_FAIL : AUDIO_CONTEXT_MAIN;
  audio_state.playing_context_valid = false;
  if (!chunk) {
    audio_start_music();
    return;
  }
  Mix_HaltChannel(-1);
  Mix_HaltMusic();
  audio_state.pending_channel = Mix_PlayChannel(-1, chunk, 0);
  if (audio_state.pending_channel < 0) {
    if (!audio_state.effect_error_logged) {
      fprintf(stderr, "audio result playback unavailable: %s\n", Mix_GetError());
      audio_state.effect_error_logged = true;
    }
    audio_start_music();
  } else {
    audio_state.effect_error_logged = false;
  }
}

void audio_play_effect(AudioEffect effect) {
  if (!audio_state.mixer_open || !audio_state.enabled ||
      audio_state.focus_paused || effect < 0 || effect >= AUDIO_EFFECT_COUNT ||
      !audio_state.effects[effect])
    return;
  if (Mix_PlayChannel(-1, audio_state.effects[effect], 0) < 0) {
    if (!audio_state.effect_error_logged) {
      fprintf(stderr, "audio effect playback unavailable: %s\n", Mix_GetError());
      audio_state.effect_error_logged = true;
    }
  } else {
    audio_state.effect_error_logged = false;
  }
}

void audio_cancel_result(void) {
  if (audio_state.mixer_open && audio_state.pending_channel >= 0)
    Mix_HaltChannel(audio_state.pending_channel);
  audio_state.pending_channel = -1;
  audio_start_music();
}

void audio_set_focus_paused(bool paused) {
  if (audio_state.focus_paused == paused) return;
  audio_state.focus_paused = paused;
  if (!audio_state.mixer_open || !audio_state.enabled) return;
  if (paused) {
    Mix_PauseMusic();
    Mix_Pause(-1);
  } else {
    Mix_ResumeMusic();
    Mix_Resume(-1);
    audio_start_music();
  }
}

void audio_notify_device_removed(void) {
  if (!audio_state.initialized) return;
  audio_close_device();
  audio_schedule_reopen(AUDIO_DEVICE_EVENT_REOPEN_MS);
}

void audio_notify_device_added(void) {
  if (!audio_state.initialized || audio_state.mixer_open) return;
  audio_schedule_reopen(0);
}

void audio_update(void) {
  if (!audio_state.initialized) return;
  if (!audio_state.mixer_open) {
    if (SDL_GetTicks64() >= audio_state.reopen_at_ms)
      (void)audio_open_device();
    if (!audio_state.mixer_open) return;
  }
  if (!audio_state.enabled) return;
  if (audio_state.pending_channel >= 0 &&
      !Mix_Playing(audio_state.pending_channel)) {
    audio_state.pending_channel = -1;
    audio_start_music();
  } else if (audio_state.pending_channel < 0) {
    audio_start_music();
  }
}

#ifdef SUDOKURA_AUDIO_TESTING
void audio_test_set_missing_assets(unsigned mask) {
  audio_test_missing_assets = mask;
}

void audio_test_force_device_loss(void) {
  audio_notify_device_removed();
}

void audio_test_retry_now(void) {
  audio_state.reopen_at_ms = 0;
}
#endif
