#include "profile.h"
#include "store_io.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *profile_path = ".sudokura-profile-test.dat";
static const char *profile_backup_path = ".sudokura-profile-test.dat.bak";
static const char *legacy_session_path = ".sudokura-profile-session-v12.dat";
static const char *legacy_preferences_path = ".sudokura-profile-preferences-v12.dat";
static const char *legacy_audio_path = ".sudokura-profile-audio-v12.dat";
static const char *legacy_session_backup = ".sudokura-profile-session-v12.dat.v1.2.bak";
static const char *legacy_preferences_backup = ".sudokura-profile-preferences-v12.dat.v1.2.bak";
static const char *legacy_audio_backup = ".sudokura-profile-audio-v12.dat.v1.2.bak";

enum {
  LEGACY_HEADER_SIZE = 18,
  LEGACY_SESSION_PAYLOAD_SIZE = 365
};

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

static void cleanup(void) {
  const char *paths[] = {
      profile_path, profile_backup_path, legacy_session_path,
      legacy_preferences_path, legacy_audio_path, legacy_session_backup,
      legacy_preferences_backup, legacy_audio_backup};
  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
    (void)store_remove_file(paths[i]);
}

static int first_playable(const Game *game) {
  for (int i = 0; i < 81; ++i)
    if (!game->fixed[i]) return i;
  return -1;
}

static SessionState normal_state(void) {
  SessionState state;
  memset(&state, 0, sizeof(state));
  game_new_difficulty(&state.game, UINT64_C(0x123456789abcdef0),
                      DIFFICULTY_HARD);
  state.mode = MODE_STRIKES;
  state.selected_row = 4;
  state.selected_column = 5;
  state.notes_mode = true;
  state.status = SESSION_ACTIVE;
  state.mistakes = 1;
  state.strikes = 1;
  state.elapsed_ms = UINT64_C(543210);
  int cell = first_playable(&state.game);
  assert(cell >= 0);
  assert(game_hint(&state.game, cell / 9, cell % 9));
  assert(session_validate(&state));
  return state;
}

static SessionState daily_state(void) {
  SessionState state;
  memset(&state, 0, sizeof(state));
  assert(game_new_daily(&state.game, 2026, 9, 18));
  state.mode = MODE_CLASSIC;
  state.selected_row = 3;
  state.selected_column = 6;
  state.status = SESSION_ACTIVE;
  state.is_daily = true;
  state.daily_year = 2026;
  state.daily_month = 9;
  state.daily_day = 18;
  state.elapsed_ms = UINT64_C(12345);
  assert(session_validate(&state));
  return state;
}

static void test_profile_roundtrip(void) {
  ProfileData profile;
  profile_defaults(&profile);
  profile.preferences.language = LANG_CA;
  profile.preferences.dark_theme = false;
  profile.preferences.strict_mode = true;
  profile.preferences.mode = MODE_TIME;
  profile.preferences.difficulty = DIFFICULTY_HARD;
  profile.preferences.audio_enabled = false;
  profile.preferences.music_volume = 31;
  profile.preferences.fx_volume = 72;
  profile.preferences.reduced_motion = true;
  profile.preferences.window_x = 40;
  profile.preferences.window_y = 50;
  profile.preferences.window_width = 1280;
  profile.preferences.window_height = 800;
  profile.preferences.window_maximized = true;

  SessionState normal = normal_state();
  SessionState daily = daily_state();
  assert(profile_slot_set(&profile.normal, &normal, true));
  assert(profile_slot_set(&profile.daily, &daily, false));

  profile.normal.value.undo_count = 1;
  profile.normal.value.undo[0] = (ProfileEdit){
      .row = 1,
      .column = 2,
      .before_value = 0,
      .after_value = 4,
      .before_notes = 0,
      .after_notes = 0,
      .before_hinted = 0,
      .after_hinted = 0,
  };

  profile.result_count = 1;
  profile.results[0] = (ProfileResult){
      .seed = normal.game.seed,
      .generator_revision = normal.game.generator_revision,
      .difficulty = normal.game.difficulty,
      .mode = MODE_STRIKES,
      .status = SESSION_LOST,
      .assisted = true,
      .is_daily = false,
      .elapsed_ms = UINT64_C(550000),
      .mistakes = 3,
      .strikes = 3,
  };

  assert(profile_validate(&profile));
  assert(profile_save_file(profile_path, profile_backup_path, &profile) ==
         STORE_OK);

  ProfileData loaded;
  assert(profile_load_file(profile_path, &loaded) == STORE_OK);
  assert(profile_validate(&loaded));
  assert(loaded.preferences.language == LANG_CA);
  assert(loaded.preferences.music_volume == 31);
  assert(loaded.preferences.fx_volume == 72);
  assert(loaded.preferences.reduced_motion);
  assert(loaded.preferences.window_width == 1280);
  assert(loaded.normal.present && loaded.daily.present);
  assert(!memcmp(&loaded.normal.value.session.game, &normal.game, sizeof(Game)));
  assert(!memcmp(&loaded.daily.value.session.game, &daily.game, sizeof(Game)));
  assert(loaded.normal.value.assisted);
  assert(loaded.normal.value.undo_count == 1);
  assert(loaded.normal.value.undo[0].after_value == 4);
  assert(loaded.result_count == 1);
  assert(loaded.results[0].status == SESSION_LOST);
}

static void test_bounds_and_future_container(void) {
  SessionState state = normal_state();
  state.elapsed_ms = SUDOKURA_SESSION_MAX_ELAPSED_MS + UINT64_C(1);
  assert(!session_validate(&state));

  ProfileData profile;
  profile_defaults(&profile);
  profile.preferences.music_volume = 101;
  assert(!profile_validate(&profile));
  profile.preferences.music_volume = 20;
  profile.preferences.window_width = 20000;
  assert(!profile_validate(&profile));

  profile_defaults(&profile);
  assert(profile_save_file(profile_path, profile_backup_path, &profile) ==
         STORE_OK);

  unsigned char bytes[16384];
  size_t size = 0;
  assert(store_read_file(profile_path, bytes, sizeof(bytes), &size) == STORE_OK);
  put_u16_test(bytes + 8, SUDOKURA_PROFILE_CONTAINER_VERSION + 1u);
  assert(store_atomic_write(profile_path, NULL, bytes, size) == STORE_OK);
  assert(profile_load_file(profile_path, &profile) == STORE_INCOMPATIBLE);
}

static void test_corruption_and_truncation(void) {
  ProfileData profile;
  profile_defaults(&profile);
  assert(profile_save_file(profile_path, profile_backup_path, &profile) ==
         STORE_OK);

  unsigned char bytes[16384];
  size_t size = 0;
  assert(store_read_file(profile_path, bytes, sizeof(bytes), &size) == STORE_OK);
  bytes[size - 1] ^= 0x5a;
  assert(store_atomic_write(profile_path, NULL, bytes, size) == STORE_OK);
  assert(profile_load_file(profile_path, &profile) == STORE_CORRUPT);

  assert(profile_save_file(profile_path, profile_backup_path, &profile) ==
         STORE_OK);
  assert(store_read_file(profile_path, bytes, sizeof(bytes), &size) == STORE_OK);
  assert(size > 8);
  assert(store_atomic_write(profile_path, NULL, bytes, size - 7) == STORE_OK);
  assert(profile_load_file(profile_path, &profile) == STORE_CORRUPT);
}

static void write_legacy_audio(void) {
  FILE *file = fopen(legacy_audio_path, "wb");
  assert(file);
  assert(fprintf(file, "SUDOAUDIO1 37 81\n") > 0);
  assert(fclose(file) == 0);
}

static ProfileLegacyPaths legacy_paths(void) {
  ProfileLegacyPaths paths = {
      .session_path = legacy_session_path,
      .preferences_path = legacy_preferences_path,
      .audio_levels_path = legacy_audio_path,
      .session_backup_path = legacy_session_backup,
      .preferences_backup_path = legacy_preferences_backup,
      .audio_levels_backup_path = legacy_audio_backup,
  };
  return paths;
}

static void test_v12_migration_and_one_time_import(void) {
  SessionState state = normal_state();
  Preferences preferences;
  preferences_defaults(&preferences);
  preferences.language = LANG_ES;
  preferences.dark_theme = false;
  preferences.mode = MODE_TIME;
  preferences.difficulty = DIFFICULTY_EASY;
  preferences.audio_enabled = true;

  assert(session_save_file(legacy_session_path, &state));
  assert(preferences_save_file(legacy_preferences_path, &preferences));
  write_legacy_audio();

  ProfileLegacyPaths paths = legacy_paths();
  ProfileData profile;
  bool migrated = false;
  assert(profile_load_or_migrate_v12(profile_path, profile_backup_path, &paths,
                                     &profile, &migrated) == STORE_OK);
  assert(migrated);
  assert(profile.normal.present);
  assert(!profile.daily.present);
  assert(profile.preferences.language == LANG_ES);
  assert(profile.preferences.music_volume == 37);
  assert(profile.preferences.fx_volume == 81);
  assert(store_file_exists(legacy_session_path));
  assert(store_file_exists(legacy_preferences_path));
  assert(store_file_exists(legacy_audio_path));
  assert(store_file_exists(legacy_session_backup));
  assert(store_file_exists(legacy_preferences_backup));
  assert(store_file_exists(legacy_audio_backup));

  write_legacy_audio();
  migrated = true;
  ProfileData second;
  assert(profile_load_or_migrate_v12(profile_path, profile_backup_path, &paths,
                                     &second, &migrated) == STORE_OK);
  assert(!migrated);
  assert(second.preferences.music_volume == 37);
  assert(second.preferences.fx_volume == 81);
}

static void test_daily_uses_separate_slot(void) {
  cleanup();
  SessionState daily = daily_state();
  assert(session_save_file(legacy_session_path, &daily));
  ProfileLegacyPaths paths = legacy_paths();
  ProfileData profile;
  bool migrated = false;
  assert(profile_load_or_migrate_v12(profile_path, profile_backup_path, &paths,
                                     &profile, &migrated) == STORE_OK);
  assert(migrated);
  assert(!profile.normal.present);
  assert(profile.daily.present);
  assert(profile.daily.value.session.is_daily);
}

static void test_incompatible_legacy_is_preserved(void) {
  cleanup();
  SessionState state = normal_state();
  assert(session_save_file(legacy_session_path, &state));

  unsigned char bytes[LEGACY_HEADER_SIZE + LEGACY_SESSION_PAYLOAD_SIZE];
  size_t size = 0;
  assert(store_read_file(legacy_session_path, bytes, sizeof(bytes), &size) ==
         STORE_OK);
  assert(size == sizeof(bytes));
  put_u32_test(bytes + LEGACY_HEADER_SIZE,
               SUDOKURA_GENERATOR_REVISION + 1u);
  put_u32_test(bytes + 14,
               crc32_bytes_test(bytes + LEGACY_HEADER_SIZE,
                                LEGACY_SESSION_PAYLOAD_SIZE));
  assert(store_atomic_write(legacy_session_path, NULL, bytes, size) == STORE_OK);

  ProfileLegacyPaths paths = legacy_paths();
  ProfileData profile;
  bool migrated = false;
  assert(profile_load_or_migrate_v12(profile_path, profile_backup_path, &paths,
                                     &profile, &migrated) ==
         STORE_INCOMPATIBLE);
  assert(!migrated);
  assert(store_file_exists(legacy_session_path));
  assert(!store_file_exists(profile_path));
}

int main(void) {
  cleanup();
  test_profile_roundtrip();
  cleanup();
  test_bounds_and_future_container();
  cleanup();
  test_corruption_and_truncation();
  cleanup();
  test_v12_migration_and_one_time_import();
  test_daily_uses_separate_slot();
  test_incompatible_legacy_is_preserved();
  cleanup();
  puts("v1.3 profile model, slots, bounds, corruption, and v1.2 migration passed");
  return 0;
}
