#include "audio.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { AUDIO_PATH_CAPACITY = 4096 };

typedef struct {
  bool initialized;
  bool subsystem_initialized;
  bool mixer_open;
  bool available;
  bool enabled;
  bool focus_paused;
  AudioContext context;
  AudioContext playing_context;
  bool playing_context_valid;
  int pending_channel;
  Mix_Music *main_loop;
  Mix_Music *fail_loop;
  Mix_Chunk *win_jingle;
  Mix_Chunk *fail_jingle;
} AudioState;

static AudioState audio_state = {.enabled = true, .pending_channel = -1};

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

static void audio_free_assets(void) {
  if (audio_state.main_loop) Mix_FreeMusic(audio_state.main_loop);
  if (audio_state.fail_loop) Mix_FreeMusic(audio_state.fail_loop);
  if (audio_state.win_jingle) Mix_FreeChunk(audio_state.win_jingle);
  if (audio_state.fail_jingle) Mix_FreeChunk(audio_state.fail_jingle);
  audio_state.main_loop = NULL;
  audio_state.fail_loop = NULL;
  audio_state.win_jingle = NULL;
  audio_state.fail_jingle = NULL;
}

static Mix_Music *audio_context_music(AudioContext context) {
  return context == AUDIO_CONTEXT_FAIL ? audio_state.fail_loop
                                       : audio_state.main_loop;
}

static void audio_start_music(void) {
  if (!audio_state.available || !audio_state.enabled ||
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
  } else {
    fprintf(stderr, "audio music: %s\n", Mix_GetError());
    audio_state.playing_context_valid = false;
  }
}

bool audio_init(void) {
  if (audio_state.initialized) return audio_state.available;
  audio_state.initialized = true;
  audio_state.enabled = true;
  audio_state.context = AUDIO_CONTEXT_MAIN;
  audio_state.pending_channel = -1;

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "audio disabled: %s\n", SDL_GetError());
    return false;
  }
  audio_state.subsystem_initialized = true;

  if ((Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG) == 0) {
    fprintf(stderr, "audio OGG support unavailable: %s\n", Mix_GetError());
    return false;
  }
  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
    fprintf(stderr, "audio disabled: %s\n", Mix_GetError());
    return false;
  }
  audio_state.mixer_open = true;

  char main_path[AUDIO_PATH_CAPACITY], fail_path[AUDIO_PATH_CAPACITY];
  char win_path[AUDIO_PATH_CAPACITY], fail_jingle_path[AUDIO_PATH_CAPACITY];
  if (!audio_resolve(main_path, "music-main.ogg") ||
      !audio_resolve(fail_path, "music-fail.ogg") ||
      !audio_resolve(win_path, "jingle-win.ogg") ||
      !audio_resolve(fail_jingle_path, "jingle-fail.ogg")) {
    fprintf(stderr, "audio assets are missing; continuing without audio\n");
    return false;
  }

  audio_state.main_loop = Mix_LoadMUS(main_path);
  audio_state.fail_loop = Mix_LoadMUS(fail_path);
  audio_state.win_jingle = Mix_LoadWAV(win_path);
  audio_state.fail_jingle = Mix_LoadWAV(fail_jingle_path);
  if (!audio_state.main_loop || !audio_state.fail_loop ||
      !audio_state.win_jingle || !audio_state.fail_jingle) {
    fprintf(stderr, "audio asset decode failed: %s\n", Mix_GetError());
    audio_free_assets();
    return false;
  }

  Mix_VolumeMusic(MIX_MAX_VOLUME * 30 / 100);
  Mix_VolumeChunk(audio_state.win_jingle, MIX_MAX_VOLUME * 65 / 100);
  Mix_VolumeChunk(audio_state.fail_jingle, MIX_MAX_VOLUME * 65 / 100);
  audio_state.available = true;
  return true;
}

void audio_shutdown(void) {
  if (!audio_state.initialized && !audio_state.subsystem_initialized) return;
  if (audio_state.mixer_open) {
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
  }
  audio_free_assets();
  if (audio_state.mixer_open) Mix_CloseAudio();
  Mix_Quit();
  if (audio_state.subsystem_initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
  memset(&audio_state, 0, sizeof(audio_state));
  audio_state.enabled = true;
  audio_state.pending_channel = -1;
}

bool audio_is_available(void) { return audio_state.available; }

bool audio_is_enabled(void) { return audio_state.enabled; }

void audio_set_enabled(bool enabled) {
  if (audio_state.enabled == enabled) return;
  audio_state.enabled = enabled;
  if (!audio_state.available) return;
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
  if (!audio_state.available || !audio_state.enabled) return;
  Mix_Chunk *chunk = cue == AUDIO_RESULT_FAIL ? audio_state.fail_jingle
                                              : audio_state.win_jingle;
  audio_state.context =
      cue == AUDIO_RESULT_FAIL ? AUDIO_CONTEXT_FAIL : AUDIO_CONTEXT_MAIN;
  Mix_HaltChannel(-1);
  Mix_HaltMusic();
  audio_state.playing_context_valid = false;
  audio_state.pending_channel = Mix_PlayChannel(-1, chunk, 0);
  if (audio_state.pending_channel < 0) {
    fprintf(stderr, "audio jingle: %s\n", Mix_GetError());
    audio_start_music();
  }
}

void audio_cancel_result(void) {
  if (!audio_state.available) return;
  if (audio_state.pending_channel >= 0)
    Mix_HaltChannel(audio_state.pending_channel);
  audio_state.pending_channel = -1;
  audio_start_music();
}

void audio_set_focus_paused(bool paused) {
  if (audio_state.focus_paused == paused) return;
  audio_state.focus_paused = paused;
  if (!audio_state.available || !audio_state.enabled) return;
  if (paused) {
    Mix_PauseMusic();
    Mix_Pause(-1);
  } else {
    Mix_ResumeMusic();
    Mix_Resume(-1);
    audio_start_music();
  }
}

void audio_update(void) {
  if (!audio_state.available || !audio_state.enabled) return;
  if (audio_state.pending_channel >= 0 &&
      !Mix_Playing(audio_state.pending_channel)) {
    audio_state.pending_channel = -1;
    audio_start_music();
  } else if (audio_state.pending_channel < 0) {
    audio_start_music();
  }
}
