#include "game.h"
#include "session.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *session_path = ".sudokura-session-test.dat";
static const char *preferences_path = ".sudokura-preferences-test.dat";

enum {
  STORE_HEADER_SIZE_TEST = 18,
  SESSION_PAYLOAD_SIZE_TEST = 365,
  LEGACY_PREFERENCES_PAYLOAD_SIZE_TEST = 5
};

static int first_playable(const Game *game) {
  for (int i = 0; i < 81; ++i) if (!game->fixed[i]) return i;
  return -1;
}

static int next_playable(const Game *game, int after) {
  for (int offset = 1; offset < 81; ++offset) {
    int i = (after + offset) % 81;
    if (!game->fixed[i]) return i;
  }
  return -1;
}

static uint32_t crc32_bytes_test(const unsigned char *data, size_t size) {
  uint32_t crc = UINT32_C(0xffffffff);
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      uint32_t mask = (uint32_t)(0u - (crc & 1u));
      crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
    }
  }
  return ~crc;
}

static void put_u16_test(unsigned char *data, uint16_t value) {
  data[0] = (unsigned char)(value & 0xffu);
  data[1] = (unsigned char)((value >> 8) & 0xffu);
}

static void put_u32_test(unsigned char *data, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    data[shift / 8] = (unsigned char)((value >> shift) & 0xffu);
}

static SessionState make_state(void) {
  SessionState state;
  memset(&state, 0, sizeof(state));
  game_new_difficulty(&state.game, UINT64_C(0x123456789abcdef0),
                      DIFFICULTY_HARD);
  state.mode = MODE_STRIKES;
  state.selected_row = 4;
  state.selected_column = 5;
  state.notes_mode = true;
  state.strict_mode = false;
  state.manual_paused = true;
  state.status = SESSION_ACTIVE;
  state.mistakes = 2;
  state.strikes = 2;
  state.elapsed_ms = UINT64_C(123456);

  int first = first_playable(&state.game);
  int second = next_playable(&state.game, first);
  int third = next_playable(&state.game, second);
  assert(first >= 0 && second >= 0 && third >= 0);
  assert(game_apply_input(&state.game, first / 9, first % 9,
                          state.game.solution[first], false, false) ==
         GAME_INPUT_CORRECT);
  int wrong = state.game.solution[second] % 9 + 1;
  assert(game_apply_input(&state.game, second / 9, second % 9, wrong,
                          false, false) == GAME_INPUT_WRONG);
  assert(game_apply_input(&state.game, second / 9, second % 9, 0,
                          false, false) == GAME_INPUT_CLEARED);
  assert(game_apply_input(&state.game, second / 9, second % 9, 3,
                          true, false) == GAME_INPUT_NOTE_ADDED);
  assert(game_hint(&state.game, third / 9, third % 9));
  assert(session_validate(&state));
  return state;
}

static void cleanup(void) {
  remove(session_path);
  remove(".sudokura-session-test.dat.tmp");
  remove(".sudokura-session-test.dat.corrupt");
  remove(".sudokura-session-test.dat.corrupt.1");
  remove(preferences_path);
  remove(".sudokura-preferences-test.dat.tmp");
  remove(".sudokura-preferences-test.dat.corrupt");
}

static void write_legacy_preferences(void) {
  unsigned char bytes[STORE_HEADER_SIZE_TEST + LEGACY_PREFERENCES_PAYLOAD_SIZE_TEST];
  memset(bytes, 0, sizeof(bytes));
  memcpy(bytes, "SUDOPREF", 8);
  put_u16_test(bytes + 8, (uint16_t)SUDOKURA_SAVE_FORMAT_VERSION);
  put_u32_test(bytes + 10, LEGACY_PREFERENCES_PAYLOAD_SIZE_TEST);

  unsigned char *payload = bytes + STORE_HEADER_SIZE_TEST;
  payload[0] = (unsigned char)LANG_ES;
  payload[1] = 0;
  payload[2] = 1;
  payload[3] = (unsigned char)MODE_TIME;
  payload[4] = (unsigned char)DIFFICULTY_HARD;
  put_u32_test(bytes + 14,
               crc32_bytes_test(payload, LEGACY_PREFERENCES_PAYLOAD_SIZE_TEST));

  FILE *file = fopen(preferences_path, "wb");
  assert(file);
  assert(fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
  assert(fclose(file) == 0);
}

static void test_preferences(void) {
  Preferences defaults;
  preferences_defaults(&defaults);
  assert(defaults.language == LANG_EN);
  assert(defaults.dark_theme);
  assert(!defaults.strict_mode);
  assert(defaults.mode == MODE_CLASSIC);
  assert(defaults.difficulty == DIFFICULTY_MEDIUM);
  assert(defaults.audio_enabled);
  assert(preferences_validate(&defaults));

  Preferences custom = {
      LANG_CA, false, true, MODE_TIME, DIFFICULTY_HARD, false};
  assert(preferences_save_file(preferences_path, &custom));
  Preferences loaded;
  preferences_defaults(&loaded);
  assert(preferences_load_file(preferences_path, &loaded) == STORE_OK);
  assert(custom.language == loaded.language);
  assert(custom.dark_theme == loaded.dark_theme);
  assert(custom.strict_mode == loaded.strict_mode);
  assert(custom.mode == loaded.mode);
  assert(custom.difficulty == loaded.difficulty);
  assert(custom.audio_enabled == loaded.audio_enabled);

  write_legacy_preferences();
  preferences_defaults(&loaded);
  loaded.audio_enabled = false;
  assert(preferences_load_file(preferences_path, &loaded) == STORE_OK);
  assert(loaded.language == LANG_ES);
  assert(!loaded.dark_theme);
  assert(loaded.strict_mode);
  assert(loaded.mode == MODE_TIME);
  assert(loaded.difficulty == DIFFICULTY_HARD);
  assert(loaded.audio_enabled);

  custom.language = (Language)99;
  assert(!preferences_validate(&custom));
  assert(!preferences_save_file(preferences_path, &custom));
}

static void test_session_roundtrip(void) {
  SessionState state = make_state();
  assert(session_save_file(session_path, &state));
  SessionState loaded;
  memset(&loaded, 0xa5, sizeof(loaded));
  assert(session_load_file(session_path, &loaded) == STORE_OK);
  assert(!memcmp(&state.game, &loaded.game, sizeof(Game)));
  assert(state.mode == loaded.mode);
  assert(state.selected_row == loaded.selected_row);
  assert(state.selected_column == loaded.selected_column);
  assert(state.notes_mode == loaded.notes_mode);
  assert(state.strict_mode == loaded.strict_mode);
  assert(state.manual_paused == loaded.manual_paused);
  assert(state.status == loaded.status);
  assert(state.mistakes == loaded.mistakes);
  assert(state.strikes == loaded.strikes);
  assert(state.elapsed_ms == loaded.elapsed_ms);
  assert(state.is_daily == loaded.is_daily);

  state.selected_row = 8;
  state.selected_column = 8;
  state.elapsed_ms += 999;
  assert(session_save_file(session_path, &state));
  assert(session_load_file(session_path, &loaded) == STORE_OK);
  assert(loaded.selected_row == 8 && loaded.selected_column == 8);
  assert(loaded.elapsed_ms == state.elapsed_ms);
}

static void test_daily_and_results(void) {
  SessionState daily;
  memset(&daily, 0, sizeof(daily));
  assert(game_new_daily(&daily.game, 2026, 8, 28));
  daily.mode = MODE_CLASSIC;
  daily.selected_row = 0;
  daily.selected_column = 0;
  daily.status = SESSION_ACTIVE;
  daily.is_daily = true;
  daily.daily_year = 2026;
  daily.daily_month = 8;
  daily.daily_day = 28;
  assert(session_validate(&daily));
  daily.daily_day = 29;
  assert(!session_validate(&daily));
  daily.daily_day = 28;

  for (int i = 0; i < 81; ++i) {
    if (!daily.game.fixed[i]) daily.game.puzzle[i] = daily.game.solution[i];
  }
  daily.status = SESSION_WON;
  assert(session_validate(&daily));
  daily.status = SESSION_ACTIVE;
  assert(!session_validate(&daily));

  SessionState lost = make_state();
  lost.status = SESSION_LOST;
  lost.strikes = 3;
  assert(session_validate(&lost));
  lost.strikes = 2;
  assert(!session_validate(&lost));
}

static void test_semantic_rejections(void) {
  SessionState state = make_state();
  int first = first_playable(&state.game);
  state.game.solution[first] = state.game.solution[first] % 9 + 1;
  assert(!session_validate(&state));
  assert(!session_save_file(session_path, &state));

  state = make_state();
  first = first_playable(&state.game);
  state.game.fixed[first] = 1;
  assert(!session_validate(&state));

  state = make_state();
  first = first_playable(&state.game);
  state.game.hinted[first] = 1;
  state.game.puzzle[first] = 0;
  assert(!session_validate(&state));

  state = make_state();
  first = first_playable(&state.game);
  state.game.notes[first] = UINT16_C(0x8000);
  assert(!session_validate(&state));
}

static void test_incompatible_generator_revision(void) {
  SessionState state = make_state();
  assert(session_save_file(session_path, &state));

  unsigned char bytes[STORE_HEADER_SIZE_TEST + SESSION_PAYLOAD_SIZE_TEST];
  FILE *file = fopen(session_path, "rb");
  assert(file);
  assert(fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
  assert(fclose(file) == 0);

  put_u32_test(bytes + STORE_HEADER_SIZE_TEST,
               SUDOKURA_GENERATOR_REVISION - 1u);
  uint32_t crc = crc32_bytes_test(bytes + STORE_HEADER_SIZE_TEST,
                                  SESSION_PAYLOAD_SIZE_TEST);
  put_u32_test(bytes + 14, crc);

  file = fopen(session_path, "wb");
  assert(file);
  assert(fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
  assert(fclose(file) == 0);

  SessionState loaded;
  assert(session_load_file(session_path, &loaded) == STORE_INCOMPATIBLE);
  file = fopen(session_path, "rb");
  assert(file);
  assert(fclose(file) == 0);
}

static void test_corruption_and_quarantine(void) {
  SessionState state = make_state();
  assert(session_save_file(session_path, &state));
  FILE *file = fopen(session_path, "r+b");
  assert(file);
  assert(fseek(file, -1, SEEK_END) == 0);
  int value = fgetc(file);
  assert(value != EOF);
  assert(fseek(file, -1, SEEK_END) == 0);
  assert(fputc(value ^ 0x5a, file) != EOF);
  assert(fclose(file) == 0);
  SessionState loaded;
  assert(session_load_file(session_path, &loaded) == STORE_CORRUPT);
  assert(store_quarantine_corrupt(session_path));
  assert(session_load_file(session_path, &loaded) == STORE_NOT_FOUND);

  file = fopen(session_path, "wb");
  assert(file);
  assert(fwrite("short", 1, 5, file) == 5);
  assert(fclose(file) == 0);
  assert(session_load_file(session_path, &loaded) == STORE_CORRUPT);

  assert(session_save_file(session_path, &state));
  file = fopen(session_path, "r+b");
  assert(file);
  assert(fseek(file, 8, SEEK_SET) == 0);
  assert(fputc(99, file) != EOF);
  assert(fclose(file) == 0);
  assert(session_load_file(session_path, &loaded) == STORE_CORRUPT);
}

int main(void) {
  cleanup();
  SessionState missing;
  assert(session_load_file(session_path, &missing) == STORE_NOT_FOUND);
  test_preferences();
  test_session_roundtrip();
  test_daily_and_results();
  test_semantic_rejections();
  test_incompatible_generator_revision();
  test_corruption_and_quarantine();
  cleanup();
  puts("session persistence tests passed");
  return 0;
}
