#define SUDOKURA_STORE_TESTING 1
#include "game_migration.h"
#include "game_library.h"
#include "store_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define test_rmdir _rmdir
#else
#include <unistd.h>
#define test_rmdir rmdir
#endif

static const char *root = ".sudokura-migration-test";
static const char *primary = ".sudokura-migration-test/profile.dat";
static const char *backup = ".sudokura-migration-test/profile.dat.bak";
static const char *source = ".sudokura-migration-test/profile.dat.pre-v1.3.7.bak";
static const char *intent = ".sudokura-migration-test/profile.dat.v1.3.7-import.dat";

static bool delete_visit(const char *name, void *context) {
  const char *dir = (const char *)context;
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  int n = snprintf(path, sizeof(path), "%s/%s", dir, name);
  assert(n >= 0 && (size_t)n < sizeof(path));
  assert(store_remove_file(path));
  return true;
}
static void cleanup(void) {
  const char *games = ".sudokura-migration-test/games";
  StoreStatus status = store_list_directory(games, delete_visit, (void *)games);
  assert(status == STORE_OK || status == STORE_NOT_FOUND);
  (void)test_rmdir(games);
  status = store_list_directory(root, delete_visit, (void *)root);
  assert(status == STORE_OK || status == STORE_NOT_FOUND);
  (void)test_rmdir(root);
}
static void prepare(void) {
  cleanup();
  assert(store_ensure_directory(root) == STORE_OK);
}

static ProfileData legacy_profile(void) {
  ProfileData old;
  profile_defaults(&old);
  old.codec_version = SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY;
  old.preferences.language = LANG_CA;
  old.preferences.music_volume = 39;

  SessionState normal;
  memset(&normal, 0, sizeof(normal));
  assert(game_new_difficulty_revision(
      &normal.game, UINT64_C(0x123456789abcdef0), DIFFICULTY_EASY,
      SUDOKURA_GENERATOR_REVISION_LEGACY));
  normal.mode = MODE_STRIKES;
  normal.selected_row = 4;
  normal.selected_column = 4;
  normal.elapsed_ms = 123456;
  normal.status = SESSION_ACTIVE;
  assert(profile_slot_set(&old.normal, &normal, false));

  SessionState daily;
  memset(&daily, 0, sizeof(daily));
  assert(game_new_daily_revision(&daily.game, 2026, 9, 18,
                                 SUDOKURA_GENERATOR_REVISION_LEGACY));
  daily.mode = MODE_CLASSIC;
  daily.selected_row = 4;
  daily.selected_column = 4;
  daily.is_daily = true;
  daily.daily_year = 2026;
  daily.daily_month = 9;
  daily.daily_day = 18;
  daily.elapsed_ms = 234567;
  daily.status = SESSION_WON;
  for (int i = 0; i < 81; ++i) {
    daily.game.puzzle[i] = daily.game.solution[i];
    daily.game.notes[i] = 0;
  }
  assert(profile_slot_set(&old.daily, &daily, false));
  old.result_count = 2;
  old.results[0] = (ProfileResult){
      .seed = daily.game.seed, .generator_revision = daily.game.generator_revision,
      .difficulty = daily.game.difficulty, .mode = daily.mode,
      .status = SESSION_WON, .is_daily = true, .elapsed_ms = daily.elapsed_ms,
      .daily_year = 2026, .daily_month = 9, .daily_day = 18,
  };
  old.results[1] = (ProfileResult){
      .seed = UINT64_C(0x11223344),
      .generator_revision = SUDOKURA_GENERATOR_REVISION_LEGACY,
      .difficulty = DIFFICULTY_HARD, .mode = MODE_CLASSIC,
      .status = SESSION_WON, .elapsed_ms = 876543,
  };
  assert(profile_validate(&old));
  return old;
}

static void write_legacy_profile(void) {
  ProfileData old = legacy_profile();
  assert(profile_save_file(primary, backup, &old) == STORE_OK);
}

static void revert_to_immutable_source(void) {
  unsigned char bytes[16384];
  size_t size = 0;
  assert(store_read_file(source, bytes, sizeof(bytes), &size) == STORE_OK);
  assert(store_atomic_write(primary, NULL, bytes, size) == STORE_OK);
}

static void assert_upgraded(const ProfileData *profile) {
  assert(profile->codec_version == SUDOKURA_PROFILE_CONTENT_VERSION);
  assert(!profile->migration_pending);
  assert(profile->preferences.language == LANG_CA);
  assert(profile->preferences.music_volume == 39);
  assert(profile->normal.present && profile->normal.retained);
  assert(profile->daily.present && profile->daily.retained);
  assert(profile->normal.revision >= 1 && profile->daily.revision == 1);
  assert(game_attempt_id_valid(profile->normal.attempt_id));
  assert(game_attempt_id_valid(profile->daily.attempt_id));
  assert(profile->result_count == 2);
  assert(game_attempt_id_equal(profile->results[0].attempt_id,
                               profile->daily.attempt_id));
  assert(game_attempt_id_valid(profile->results[1].attempt_id));
  assert(!game_attempt_id_equal(profile->results[1].attempt_id,
                                profile->daily.attempt_id));
  assert(profile->migration_source_length > 0);
}

static void test_idempotent_import_and_newer_revision(void) {
  prepare();
  write_legacy_profile();
  ProfileData state;
  assert(profile_load_file(primary, &state) == STORE_OK);
  bool upgraded = false;
  assert(game_migration_upgrade(root, primary, backup, &state, &upgraded) ==
         STORE_OK);
  assert(upgraded);
  assert_upgraded(&state);
  GameAttemptId original_id = state.normal.attempt_id;
  GameAttemptId daily_id = state.daily.attempt_id;
  GameAttemptId result_id = state.results[1].attempt_id;
  assert(store_file_exists(source) && store_file_exists(intent));

  ProfileData intent_profile;
  assert(profile_load_file(intent, &intent_profile) == STORE_OK);
  assert(intent_profile.migration_pending);
  assert(intent_profile.migration_source_length ==
         state.migration_source_length);

  GameLibraryRecord record;
  assert(game_library_load(root, daily_id, &record) == STORE_OK);
  assert(record.kind == GAME_RECORD_EXACT && record.result.status == SESSION_WON);
  assert(game_library_load(root, result_id, &record) == STORE_OK);
  assert(record.kind == GAME_RECORD_LEGACY_SUMMARY);
  assert(record.finished_utc == 0);

  upgraded = true;
  assert(game_migration_upgrade(root, primary, backup, &state, &upgraded) ==
         STORE_OK);
  assert(!upgraded);

  assert(game_library_load(root, original_id, &record) == STORE_OK);
  assert(record.kind == GAME_RECORD_EXACT);
  ProfileSession latest = record.exact;
  latest.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, original_id, 2, &latest, true));
  assert(game_library_save(root, &record) == STORE_OK);

  revert_to_immutable_source();
  assert(profile_load_file(primary, &state) == STORE_OK);
  assert(state.codec_version == SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY);
  assert(game_migration_upgrade(root, primary, backup, &state, &upgraded) ==
         STORE_OK);
  assert(upgraded);
  assert_upgraded(&state);
  assert(game_attempt_id_equal(state.normal.attempt_id, original_id));
  assert(game_attempt_id_equal(state.daily.attempt_id, daily_id));
  assert(game_attempt_id_equal(state.results[1].attempt_id, result_id));
  assert(state.normal.revision == 2);
  assert(state.normal.value.session.elapsed_ms == latest.session.elapsed_ms);
  cleanup();
}

static void test_receipt_survives_source_rollback(void) {
  prepare();
  write_legacy_profile();
  ProfileData state;
  assert(profile_load_file(primary, &state) == STORE_OK);
  bool upgraded = false;
  assert(game_migration_upgrade(root, primary, backup, &state, &upgraded) ==
         STORE_OK);
  GameAttemptId daily_id = state.daily.attempt_id;
  assert(game_library_delete(root, daily_id) == STORE_OK);
  revert_to_immutable_source();
  assert(profile_load_file(primary, &state) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &state, &upgraded) ==
         STORE_OK);
  assert(!state.daily.present);
  assert(state.normal.present);
  assert(state.result_count == 1);
  assert(game_library_load(root, daily_id, &(GameLibraryRecord){0}) ==
         STORE_NOT_FOUND);
  cleanup();
}

static void test_failure_preserves_source_and_retry(void) {
  prepare();
  write_legacy_profile();
  ProfileData original;
  assert(profile_load_file(primary, &original) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  bool upgraded = false;
  assert(game_migration_upgrade(root, primary, backup, &original,
                                &upgraded) == STORE_IO_ERROR);
  assert(!upgraded);
  ProfileData current;
  assert(profile_load_file(primary, &current) == STORE_OK);
  assert(current.codec_version == SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY);
  assert(!store_file_exists(intent));
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(game_migration_upgrade(root, primary, backup, &current,
                                &upgraded) == STORE_OK);
  assert(upgraded);
  assert_upgraded(&current);
  cleanup();
}

static void test_immutable_preimage_collision(void) {
  prepare();
  write_legacy_profile();
  assert(store_atomic_write(source, NULL,
                            (const unsigned char *)"different", 9) == STORE_OK);
  ProfileData current;
  assert(profile_load_file(primary, &current) == STORE_OK);
  bool upgraded = false;
  assert(game_migration_upgrade(root, primary, backup, &current,
                                &upgraded) == STORE_OK);
  assert(upgraded && current.migration_source_suffix == 1);
  unsigned char bytes[16];
  size_t size = 0;
  assert(store_read_file(source, bytes, sizeof(bytes), &size) == STORE_OK);
  assert(size == 9 && memcmp(bytes, "different", 9) == 0);
  cleanup();
}

static void test_existing_library_blocks_fresh_migration(void) {
  prepare();
  assert(!game_migration_has_artifacts(root));
  char path[128];
  (void)snprintf(path, sizeof(path), "%s/games", root);
  assert(store_ensure_directory(path) == STORE_OK);
  (void)snprintf(path, sizeof(path), "%s/games/leftover.deleted", root);
  assert(store_atomic_write(path, NULL,
                            (const unsigned char *)"x", 1) == STORE_OK);
  assert(game_migration_has_artifacts(root));
  cleanup();
}

static void test_record_first_failure_recovers_terminal_and_recent_result(void) {
  prepare();
  write_legacy_profile();
  ProfileData live;
  assert(profile_load_file(primary, &live) == STORE_OK);
  bool migrated = false;
  assert(game_migration_upgrade(root, primary, backup, &live, &migrated) ==
         STORE_OK);
  assert(migrated);
  GameAttemptId id = live.normal.attempt_id;
  uint64_t initial_revision = live.normal.revision;
  assert(live.normal.value.session.status == SESSION_ACTIVE);
  uint16_t old_result_count = live.result_count;

  ProfileSession won = live.normal.value;
  for (int cell = 0; cell < 81; ++cell) {
    won.session.game.puzzle[cell] = won.session.game.solution[cell];
    won.session.game.notes[cell] = 0;
  }
  won.session.status = SESSION_WON;
  won.session.elapsed_ms += UINT64_C(1500);
  won.undo_count = 0;
  won.redo_count = 0;
  assert(session_validate(&won.session));
  GameLibraryRecord archive;
  assert(game_library_make_exact(&archive, id, initial_revision + 1,
                                 &won, true));
  assert(game_library_save(root, &archive) == STORE_OK);

  ProfileData candidate = live;
  candidate.normal.value = won;
  candidate.normal.revision = initial_revision + 1;
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(profile_save_file(primary, backup, &candidate) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);

  ProfileData recovered;
  assert(profile_load_file(primary, &recovered) == STORE_OK);
  assert(recovered.normal.revision == initial_revision);
  assert(recovered.normal.value.session.status == SESSION_ACTIVE);
  assert(game_migration_reconcile(root, primary, backup, &recovered) == STORE_OK);
  assert(recovered.normal.revision == initial_revision + 1);
  assert(recovered.normal.value.session.status == SESSION_WON);
  assert(recovered.normal.value.session.elapsed_ms == won.session.elapsed_ms);
  assert(recovered.result_count == old_result_count + 1);
  const ProfileResult *last = &recovered.results[recovered.result_count - 1];
  assert(last->status == SESSION_WON);
  assert(game_attempt_id_equal(last->attempt_id, id));

  ProfileData reread;
  assert(profile_load_file(primary, &reread) == STORE_OK);
  assert(reread.normal.revision == initial_revision + 1);
  assert(reread.result_count == recovered.result_count);
  assert(game_migration_reconcile(root, primary, backup, &reread) == STORE_OK);
  assert(reread.result_count == recovered.result_count);
  cleanup();
}


static void test_missing_hot_pair_recovers_verified_intent(void) {
  prepare();
  write_legacy_profile();
  ProfileData current;
  assert(profile_load_file(primary, &current) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &current, NULL) ==
         STORE_OK);
  GameAttemptId id = current.normal.attempt_id;
  GameLibraryRecord record;
  assert(game_library_load(root, id, &record) == STORE_OK);
  ProfileSession newer = record.exact;
  newer.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, id, 2, &newer, true));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(store_remove_file(primary));
  assert(store_remove_file(backup));
  assert(game_migration_has_artifacts(root));

  ProfileData recovered;
  profile_defaults(&recovered);
  GameMigrationRecoverySummary summary;
  assert(game_migration_recover_collection(
             root, primary, backup, &recovered, &summary) ==
         STORE_IO_ERROR);
  assert(!store_file_exists(primary));
  assert(game_migration_recover_intent(root, primary, backup,
                                      &recovered) == STORE_OK);
  assert(recovered.preferences.language == LANG_CA);
  assert(recovered.normal.present &&
         game_attempt_id_equal(recovered.normal.attempt_id, id));
  assert(recovered.normal.revision == 2);
  assert(recovered.normal.value.session.elapsed_ms == newer.session.elapsed_ms);
  assert(profile_load_file(primary, &current) == STORE_OK);
  assert(profile_encoded_equal(&current, &recovered));
  cleanup();
}

static void test_explicit_collection_recovery_and_ambiguity(void) {
  prepare();
  assert(!game_migration_has_artifacts(root));
  ProfileData legacy = legacy_profile();
  ProfileSession first = legacy.normal.value;
  GameLibraryRecord record;
  GameAttemptId a = {UINT64_C(0xdeadbeef), 1};
  GameAttemptId b = {UINT64_C(0xdeadbeef), 2};
  assert(game_library_make_exact(&record, a, 1, &first, true));
  assert(game_library_save(root, &record) == STORE_OK);

  ProfileData candidate;
  GameMigrationRecoverySummary info = {0};
  assert(game_migration_preview_collection(root, &candidate, &info) ==
         STORE_OK);
  assert(candidate.normal.present && candidate.normal.retained);
  assert(game_attempt_id_equal(candidate.normal.attempt_id, a));
  assert(info.normal_active_candidates == 1);
  assert(info.compatible_records == 1);

  first.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, b, 1, &first, true));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(game_migration_preview_collection(root, &candidate, &info) ==
         STORE_OK);
  assert(!candidate.normal.present);
  assert(info.normal_active_candidates == 2);
  assert(info.compatible_records == 2);

  ProfileData recovered;
  profile_defaults(&recovered);
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_migration_recover_collection(
             root, primary, backup, &recovered, &info) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(!store_file_exists(primary));
  assert(game_library_load(root, a, &record) == STORE_OK);
  assert(game_migration_recover_collection(
             root, primary, backup, &recovered, &info) == STORE_OK);
  assert(!recovered.normal.present);
  assert(!recovered.daily.present);
  assert(info.normal_active_candidates == 2);
  assert(game_library_load(root, b, &record) == STORE_OK);
  assert(game_migration_recover_collection(
             root, primary, backup, &recovered, &info) == STORE_IO_ERROR);
  cleanup();
}

static void test_collection_recovery_isolates_bad_and_previous_only(void) {
  prepare();
  ProfileData legacy = legacy_profile();
  GameLibraryRecord record;
  GameAttemptId good = {UINT64_C(0xbead), 1};
  GameAttemptId broken = {UINT64_C(0xbead), 2};
  assert(game_library_make_exact(&record, good, 1, &legacy.normal.value, true));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(game_library_make_exact(&record, broken, 1,
                                 &legacy.normal.value, true));
  assert(game_library_save(root, &record) == STORE_OK);
  char hex[33], path[SUDOKURA_STORE_PATH_CAPACITY];
  game_attempt_id_hex(broken, hex);
  int count = snprintf(path, sizeof(path), "%s/games/%s.dat", root, hex);
  assert(count > 0 && (size_t)count < sizeof(path));
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t length = 0;
  assert(store_read_file(path, bytes, sizeof(bytes), &length) == STORE_OK);
  bytes[length - 1u] ^= 0x10u;
  assert(store_atomic_write(path, NULL, bytes, length) == STORE_OK);
  GameMigrationRecoverySummary info;
  ProfileData preview;
  assert(game_migration_preview_collection(root, &preview, &info) ==
         STORE_OK);
  assert(info.compatible_records == 1);
  assert(info.unavailable_records == 1);
  assert(preview.normal.present &&
         game_attempt_id_equal(preview.normal.attempt_id, good));
  assert(game_library_load(root, broken, &record) == STORE_CORRUPT);
  cleanup();
}

int main(void) {
  cleanup();
  test_idempotent_import_and_newer_revision();
  test_receipt_survives_source_rollback();
  test_failure_preserves_source_and_retry();
  test_immutable_preimage_collision();
  test_record_first_failure_recovers_terminal_and_recent_result();
  test_existing_library_blocks_fresh_migration();
  test_missing_hot_pair_recovers_verified_intent();
  test_explicit_collection_recovery_and_ambiguity();
  test_collection_recovery_isolates_bad_and_previous_only();
  cleanup();
  puts("immutable v1 import, replay-safe IDs, cut-point retry and receipts passed");
  return 0;
}
