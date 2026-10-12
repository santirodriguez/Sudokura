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

static void test_durable_delete_receipt_survives_profile_write_failures(void) {
  prepare();
  write_legacy_profile();
  ProfileData baseline;
  assert(profile_load_file(primary, &baseline) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &baseline, NULL) ==
         STORE_OK);
  assert_upgraded(&baseline);

  GameAttemptId daily_id = baseline.daily.attempt_id;
  GameAttemptId normal_id = baseline.normal.attempt_id;
  GameAttemptId unrelated_result = baseline.results[1].attempt_id;
  assert(game_library_delete(root, daily_id) == STORE_OK);
  assert(game_library_receipt_status(root, daily_id) == STORE_OK);

  /* Deletion is already logically committed, but the hot profile is stale. */
  const StoreTestFault faults[] = {
      STORE_TEST_FAULT_OPEN, STORE_TEST_FAULT_DURING_WRITE,
      STORE_TEST_FAULT_NO_SPACE, STORE_TEST_FAULT_SYNC,
      STORE_TEST_FAULT_REPLACE
  };
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    ProfileData attempted = baseline;
    store_test_set_fault(faults[i]);
    assert(game_migration_reconcile(root, primary, backup, &attempted) ==
           STORE_IO_ERROR);
    store_test_set_fault(STORE_TEST_FAULT_NONE);

    /* In-memory and on-disk snapshots remain unchanged on failure. */
    assert(profile_encoded_equal(&attempted, &baseline));
    ProfileData disk;
    assert(profile_load_file(primary, &disk) == STORE_OK);
    assert(profile_encoded_equal(&disk, &baseline));
    assert(game_library_receipt_status(root, daily_id) == STORE_OK);
    assert(game_library_load(root, daily_id, &(GameLibraryRecord){0}) ==
           STORE_NOT_FOUND);
    GameLibraryRecord valid;
    assert(game_library_load(root, normal_id, &valid) == STORE_OK);
    assert(game_library_load(root, unrelated_result, &valid) == STORE_OK);
  }

  ProfileData reconciled = baseline;
  assert(game_migration_reconcile(root, primary, backup, &reconciled) ==
         STORE_OK);
  assert(!reconciled.daily.present);
  assert(reconciled.normal.present);
  assert(game_attempt_id_equal(reconciled.normal.attempt_id, normal_id));
  assert(reconciled.result_count == 1);
  assert(game_attempt_id_equal(reconciled.results[0].attempt_id,
                               unrelated_result));
  assert(preferences_validate(&reconciled.preferences));
  assert(reconciled.preferences.language == baseline.preferences.language);
  assert(reconciled.preferences.music_volume ==
         baseline.preferences.music_volume);
  ProfileData disk;
  assert(profile_load_file(primary, &disk) == STORE_OK);
  assert(profile_encoded_equal(&disk, &reconciled));

  /* An idempotent second replay must not rotate copies or write at all. */
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_migration_reconcile(root, primary, backup, &reconciled) ==
         STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_load_file(primary, &disk) == STORE_OK);
  assert(profile_encoded_equal(&disk, &reconciled));

  /* Even restoring the immutable old-format predecessor cannot resurrect it. */
  revert_to_immutable_source();
  assert(profile_load_file(primary, &disk) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &disk, NULL) ==
         STORE_OK);
  assert(!disk.daily.present && disk.normal.present);
  assert(disk.result_count == 1);
  assert(game_attempt_id_equal(disk.normal.attempt_id, normal_id));
  assert(game_attempt_id_equal(disk.results[0].attempt_id,
                               unrelated_result));
  assert(game_library_receipt_status(root, daily_id) == STORE_OK);
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


static void test_intent_recovery_rejects_tampered_immutable_pair(void) {
  prepare();
  write_legacy_profile();
  ProfileData saved;
  assert(profile_load_file(primary, &saved) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &saved, NULL) ==
         STORE_OK);
  GameAttemptId normal_id = saved.normal.attempt_id;
  GameAttemptId daily_id = saved.daily.attempt_id;
  unsigned char source_bytes[16384], intent_bytes[16384];
  size_t source_length = 0, intent_length = 0;
  assert(store_read_file(source, source_bytes, sizeof(source_bytes),
                         &source_length) == STORE_OK);
  assert(store_read_file(intent, intent_bytes, sizeof(intent_bytes),
                         &intent_length) == STORE_OK);
  assert(source_length > 0 && intent_length > 0);

  assert(store_remove_file(primary));
  assert(store_remove_file(backup));
  ProfileData output;
  profile_defaults(&output);
  output.preferences.language = LANG_ES;
  ProfileData sentinel = output;

  /* A valid intent referencing modified pre-upgrade bytes must not write a
     profile, even though the matching game records remain recoverable. */
  unsigned char modified[16384];
  memcpy(modified, source_bytes, source_length);
  modified[source_length - 1u] ^= 0x42u;
  assert(store_atomic_write(source, NULL, modified, source_length) == STORE_OK);
  assert(game_migration_recover_intent(root, primary, backup, &output) ==
         STORE_CORRUPT);
  assert(profile_encoded_equal(&output, &sentinel));
  assert(!store_file_exists(primary));
  assert(!store_file_exists(backup));

  assert(store_atomic_write(source, NULL, source_bytes, source_length) ==
         STORE_OK);
  memcpy(modified, intent_bytes, intent_length);
  modified[intent_length - 1u] ^= 0x29u;
  assert(store_atomic_write(intent, NULL, modified, intent_length) == STORE_OK);
  assert(game_migration_recover_intent(root, primary, backup, &output) ==
         STORE_CORRUPT);
  assert(profile_encoded_equal(&output, &sentinel));
  assert(!store_file_exists(primary));
  assert(!store_file_exists(backup));

  /* Restoring the exact immutable pair permits one consistent recovery with
     the pre-existing attempt IDs, not a new import or duplicate result. */
  assert(store_atomic_write(intent, NULL, intent_bytes, intent_length) ==
         STORE_OK);
  assert(game_migration_recover_intent(root, primary, backup, &output) ==
         STORE_OK);
  assert(output.codec_version == SUDOKURA_PROFILE_CONTENT_VERSION);
  assert(!output.migration_pending);
  assert(game_attempt_id_equal(output.normal.attempt_id, normal_id));
  assert(game_attempt_id_equal(output.daily.attempt_id, daily_id));
  assert(output.result_count == saved.result_count);
  assert(output.preferences.language == LANG_CA);
  ProfileData from_disk;
  assert(profile_load_file(primary, &from_disk) == STORE_OK);
  assert(profile_encoded_equal(&from_disk, &output));
  GameLibraryRecord record;
  assert(game_library_load(root, normal_id, &record) == STORE_OK);
  assert(game_library_load(root, daily_id, &record) == STORE_OK);
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

static void test_preimage_write_fault_matrix(void) {
  const StoreTestFault faults[] = {
      STORE_TEST_FAULT_OPEN, STORE_TEST_FAULT_DURING_WRITE,
      STORE_TEST_FAULT_NO_SPACE, STORE_TEST_FAULT_SYNC,
      STORE_TEST_FAULT_REPLACE};
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    prepare();
    write_legacy_profile();
    ProfileData live;
    assert(profile_load_file(primary, &live) == STORE_OK);
    store_test_set_fault(faults[i]);
    bool upgraded = true;
    assert(game_migration_upgrade(root, primary, backup, &live,
                                  &upgraded) == STORE_IO_ERROR);
    assert(!upgraded);
    store_test_set_fault(STORE_TEST_FAULT_NONE);
    assert(!store_file_exists(intent));
    ProfileData original;
    assert(profile_load_file(primary, &original) == STORE_OK);
    assert(original.codec_version ==
           SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY);
    assert(original.preferences.language == LANG_CA);
    assert(game_migration_upgrade(root, primary, backup, &original,
                                  &upgraded) == STORE_OK);
    assert(upgraded);
    assert_upgraded(&original);
    cleanup();
  }
}

typedef struct {
  unsigned count;
} MigrationRecordCount;

static bool count_migrated_record(const GameLibraryEntry *entry,
                                  void *context) {
  MigrationRecordCount *result = (MigrationRecordCount *)context;
  assert(entry->status == STORE_OK);
  ++result->count;
  return true;
}

static void test_immutable_intent_cutpoint_replays(void) {
  prepare();
  write_legacy_profile();
  ProfileData initial;
  assert(profile_load_file(primary, &initial) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &initial, NULL) ==
         STORE_OK);
  GameAttemptId id = initial.normal.attempt_id;
  GameAttemptId daily_id = initial.daily.attempt_id;
  GameAttemptId legacy_id = initial.results[1].attempt_id;
  unsigned char preimage[16384], intent_bytes[16384];
  size_t preimage_size = 0, intent_size = 0;
  assert(store_read_file(source, preimage, sizeof(preimage),
                         &preimage_size) == STORE_OK);
  assert(store_read_file(intent, intent_bytes, sizeof(intent_bytes),
                         &intent_size) == STORE_OK);

  /* Cut after both immutable files, before publishing the pending profile. */
  revert_to_immutable_source();
  ProfileData staged;
  assert(profile_load_file(primary, &staged) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_migration_upgrade(root, primary, backup, &staged, NULL) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_load_file(primary, &staged) == STORE_OK);
  assert(staged.codec_version == SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY);
  assert(game_migration_upgrade(root, primary, backup, &staged, NULL) ==
         STORE_OK);
  assert(game_attempt_id_equal(staged.normal.attempt_id, id));

  /* Cut after the pending profile, before one required record commit. */
  ProfileData pending;
  assert(profile_load_file(intent, &pending) == STORE_OK);
  assert(pending.migration_pending);
  assert(profile_save_file(primary, NULL, &pending) == STORE_OK);
  char hex[33], record_path[SUDOKURA_STORE_PATH_CAPACITY];
  game_attempt_id_hex(id, hex);
  int n = snprintf(record_path, sizeof(record_path),
                   "%s/games/%s.dat", root, hex);
  assert(n > 0 && (size_t)n < sizeof(record_path));
  assert(store_remove_file(record_path));
  assert(game_library_load(root, id, &(GameLibraryRecord){0}) ==
         STORE_NOT_FOUND);
  assert(profile_load_file(primary, &staged) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_migration_upgrade(root, primary, backup, &staged, NULL) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_load_file(primary, &staged) == STORE_OK);
  assert(staged.migration_pending);
  assert(game_migration_upgrade(root, primary, backup, &staged, NULL) ==
         STORE_OK);
  assert(game_attempt_id_equal(staged.normal.attempt_id, id));

  /* Cut after records/readback, before clearing the pending bit. */
  assert(profile_save_file(primary, NULL, &pending) == STORE_OK);
  assert(profile_load_file(primary, &staged) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_migration_upgrade(root, primary, backup, &staged, NULL) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_load_file(primary, &staged) == STORE_OK);
  assert(staged.migration_pending);
  assert(game_migration_upgrade(root, primary, backup, &staged, NULL) ==
         STORE_OK);
  assert_upgraded(&staged);
  assert(game_attempt_id_equal(staged.normal.attempt_id, id));
  assert(game_attempt_id_equal(staged.daily.attempt_id, daily_id));
  assert(game_attempt_id_equal(staged.results[1].attempt_id, legacy_id));

  unsigned char verified[16384];
  size_t length = 0;
  assert(store_read_file(source, verified, sizeof(verified), &length) ==
         STORE_OK);
  assert(length == preimage_size &&
         memcmp(verified, preimage, preimage_size) == 0);
  assert(store_read_file(intent, verified, sizeof(verified), &length) ==
         STORE_OK);
  assert(length == intent_size &&
         memcmp(verified, intent_bytes, intent_size) == 0);
  MigrationRecordCount count = {0};
  assert(game_library_scan(root, count_migrated_record, &count) == STORE_OK);
  assert(count.count == 3);
  cleanup();
}

static void test_repeated_terminal_summaries_are_distinct_attempts(void) {
  prepare();
  ProfileData old = legacy_profile();
  old.result_count = SUDOKURA_RESULT_LIMIT;
  for (uint16_t i = 2; i < old.result_count; ++i)
    old.results[i] = old.results[0]; /* Same event as the won Daily slot. */
  assert(profile_validate(&old));
  assert(profile_save_file(primary, backup, &old) == STORE_OK);

  ProfileData migrated;
  assert(profile_load_file(primary, &migrated) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &migrated, NULL) ==
         STORE_OK);
  assert(!migrated.migration_pending);
  assert(migrated.result_count == SUDOKURA_RESULT_LIMIT);
  assert(game_attempt_id_equal(migrated.results[0].attempt_id,
                               migrated.daily.attempt_id));
  GameAttemptId ids[SUDOKURA_RESULT_LIMIT];
  for (uint16_t i = 0; i < migrated.result_count; ++i) {
    ids[i] = migrated.results[i].attempt_id;
    assert(game_attempt_id_valid(ids[i]));
    for (uint16_t j = 0; j < i; ++j)
      assert(!game_attempt_id_equal(ids[i], ids[j]));
    GameLibraryRecord record;
    assert(game_library_load(root, ids[i], &record) == STORE_OK);
    assert(record.kind == (i == 0 ? GAME_RECORD_EXACT
                                 : GAME_RECORD_LEGACY_SUMMARY));
    assert(record.result.seed == old.results[i].seed);
    assert(record.result.elapsed_ms == old.results[i].elapsed_ms);
    if (i != 0) assert(record.finished_utc == 0);
  }
  MigrationRecordCount count = {0};
  assert(game_library_scan(root, count_migrated_record, &count) == STORE_OK);
  assert(count.count == SUDOKURA_RESULT_LIMIT + 1u); /* Plus normal slot. */

  revert_to_immutable_source();
  assert(profile_load_file(primary, &migrated) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &migrated, NULL) ==
         STORE_OK);
  assert(migrated.result_count == SUDOKURA_RESULT_LIMIT);
  for (uint16_t i = 0; i < migrated.result_count; ++i)
    assert(game_attempt_id_equal(migrated.results[i].attempt_id, ids[i]));
  count.count = 0;
  assert(game_library_scan(root, count_migrated_record, &count) == STORE_OK);
  assert(count.count == SUDOKURA_RESULT_LIMIT + 1u);

  assert(game_library_delete(root, ids[64]) == STORE_OK);
  revert_to_immutable_source();
  assert(profile_load_file(primary, &migrated) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &migrated, NULL) ==
         STORE_OK);
  assert(migrated.result_count == SUDOKURA_RESULT_LIMIT - 1u);
  assert(game_library_load(root, ids[64], &(GameLibraryRecord){0}) ==
         STORE_NOT_FOUND);
  count.count = 0;
  assert(game_library_scan(root, count_migrated_record, &count) == STORE_OK);
  assert(count.count == SUDOKURA_RESULT_LIMIT);
  cleanup();
}


static void assert_session_bytes_equal(const ProfileSession *session,
                                       const unsigned char *original,
                                       size_t original_size) {
  unsigned char encoded[4096];
  size_t size = 0;
  assert(profile_session_encode_blob(session, encoded, sizeof(encoded), &size));
  assert(size == original_size && memcmp(encoded, original, size) == 0);
}

static void test_legacy_v1_rich_state_survives_migration_and_recovery(void) {
  prepare();
  ProfileData old = legacy_profile();
  old.preferences.dark_theme = false;
  old.preferences.strict_mode = true;
  old.preferences.mode = MODE_STRIKES;
  old.preferences.difficulty = DIFFICULTY_HARD;
  old.preferences.audio_enabled = false;
  old.preferences.music_volume = 39;
  old.preferences.fx_volume = 72;
  old.preferences.music_muted = true;
  old.preferences.fx_muted = true;
  old.preferences.reduced_motion = true;
  old.preferences.auto_remove_peer_notes = true;
  old.preferences.window_x = 40;
  old.preferences.window_y = 60;
  old.preferences.window_width = 1280;
  old.preferences.window_height = 800;
  old.preferences.window_maximized = true;

  ProfileSession *saved = &old.normal.value;
  Game *game = &saved->session.game;
  int editable[4];
  unsigned found = 0;
  for (int i = 0; i < 81 && found < 4; ++i)
    if (!game->fixed[i]) editable[found++] = i;
  assert(found == 4);

  const int placed = editable[0];
  const int redo_cell = editable[1];
  const int hinted = editable[2];
  const int noted = editable[3];
  GameEdit placed_edit, redo_edit;
  assert(game_apply_input_recorded(game, placed / 9, placed % 9,
                                   game->solution[placed], false, false,
                                   false, &placed_edit) == GAME_INPUT_CORRECT);
  assert(game_apply_input_recorded(game, redo_cell / 9, redo_cell % 9,
                                   game->solution[redo_cell], true, false,
                                   false, &redo_edit) == GAME_INPUT_NOTE_ADDED);
  assert(game_apply_edit(game, &redo_edit, false));
  assert(game_hint(game, hinted / 9, hinted % 9));
  assert(game_toggle_note(game, noted / 9, noted % 9,
                          game->solution[noted]));

  saved->session.selected_row = 2;
  saved->session.selected_column = 7;
  saved->session.notes_mode = true;
  saved->session.strict_mode = true;
  saved->session.manual_paused = true;
  saved->session.mistakes = 2;
  saved->session.strikes = 2;
  saved->session.elapsed_ms = UINT64_C(654321);
  saved->assisted = true;
  saved->undo_count = 1;
  saved->undo[0] = placed_edit;
  saved->redo_count = 1;
  saved->redo[0] = redo_edit;
  assert(profile_validate(&old));

  unsigned char normal_bytes[4096], daily_bytes[4096];
  size_t normal_size = 0, daily_size = 0;
  assert(profile_session_encode_blob(&old.normal.value, normal_bytes,
                                     sizeof(normal_bytes), &normal_size));
  assert(profile_session_encode_blob(&old.daily.value, daily_bytes,
                                     sizeof(daily_bytes), &daily_size));
  assert(profile_save_file(primary, backup, &old) == STORE_OK);

  unsigned char source_bytes[16384], readback[16384];
  size_t source_size = 0, readback_size = 0;
  assert(store_read_file(primary, source_bytes, sizeof(source_bytes),
                         &source_size) == STORE_OK);
  ProfileData staged;
  assert(profile_load_file(primary, &staged) == STORE_OK);
  assert(staged.codec_version == SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY);
  assert(game_migration_upgrade(root, primary, backup, &staged, NULL) ==
         STORE_OK);
  assert(!staged.migration_pending);
  assert(staged.normal.present && staged.daily.present);
  assert(staged.normal.retained && staged.daily.retained);
  assert_session_bytes_equal(&staged.normal.value, normal_bytes, normal_size);
  assert_session_bytes_equal(&staged.daily.value, daily_bytes, daily_size);
  assert(staged.normal.value.undo_count == 1 &&
         staged.normal.value.redo_count == 1);
  assert(staged.normal.value.session.game.hinted[hinted] == 1);
  assert(staged.normal.value.session.game.notes[noted] ==
         (uint16_t)(1u << game->solution[noted]));
  assert(staged.normal.value.assisted && staged.normal.value.session.notes_mode);
  assert(staged.normal.value.session.manual_paused);
  assert(staged.normal.value.session.strikes == 2);
  assert(staged.normal.value.session.elapsed_ms == UINT64_C(654321));

  const Preferences *prefs = &staged.preferences;
  assert(prefs->language == LANG_CA && !prefs->dark_theme);
  assert(prefs->strict_mode && prefs->mode == MODE_STRIKES);
  assert(prefs->difficulty == DIFFICULTY_HARD);
  assert(!prefs->audio_enabled);
  assert(prefs->music_volume == 39 && prefs->fx_volume == 72);
  assert(prefs->music_muted && prefs->fx_muted);
  assert(prefs->reduced_motion && prefs->auto_remove_peer_notes);
  assert(prefs->window_x == 40 && prefs->window_y == 60);
  assert(prefs->window_width == 1280 && prefs->window_height == 800);
  assert(prefs->window_maximized);

  GameAttemptId normal_id = staged.normal.attempt_id;
  GameAttemptId daily_id = staged.daily.attempt_id;
  GameLibraryRecord record;
  assert(game_library_load(root, normal_id, &record) == STORE_OK);
  assert(record.kind == GAME_RECORD_EXACT && record.retained);
  assert_session_bytes_equal(&record.exact, normal_bytes, normal_size);
  assert(game_library_load(root, daily_id, &record) == STORE_OK);
  assert_session_bytes_equal(&record.exact, daily_bytes, daily_size);

  assert(store_read_file(source, readback, sizeof(readback),
                         &readback_size) == STORE_OK);
  assert(readback_size == source_size &&
         memcmp(readback, source_bytes, source_size) == 0);
  ProfileData immutable;
  assert(profile_load_file(source, &immutable) == STORE_OK);
  assert(profile_encoded_equal(&immutable, &old));

  /* Recovered old rotating profile must reuse immutable-intent IDs and data. */
  revert_to_immutable_source();
  assert(profile_load_file(primary, &staged) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &staged, NULL) ==
         STORE_OK);
  assert(game_attempt_id_equal(staged.normal.attempt_id, normal_id));
  assert(game_attempt_id_equal(staged.daily.attempt_id, daily_id));
  assert_session_bytes_equal(&staged.normal.value, normal_bytes, normal_size);
  assert_session_bytes_equal(&staged.daily.value, daily_bytes, daily_size);

  /* Both hot copies lost: explicit validated intent reconstruction. */
  assert(store_remove_file(primary));
  assert(store_remove_file(backup));
  ProfileData recovered;
  assert(game_migration_recover_intent(root, primary, backup, &recovered) ==
         STORE_OK);
  assert(game_attempt_id_equal(recovered.normal.attempt_id, normal_id));
  assert(game_attempt_id_equal(recovered.daily.attempt_id, daily_id));
  assert_session_bytes_equal(&recovered.normal.value,
                             normal_bytes, normal_size);
  assert_session_bytes_equal(&recovered.daily.value, daily_bytes, daily_size);
  assert(profile_encoded_equal(&recovered, &staged));
  cleanup();
}

static void test_unacknowledged_immutable_source_replays_safely(void) {
  prepare();
  write_legacy_profile();
  ProfileData source_profile;
  assert(profile_load_file(primary, &source_profile) == STORE_OK);
  ProfileData unchanged = source_profile;

  /* The immutable source file may exist after the directory sync fails,
     although the migration must not report completion or mint intent IDs. */
  store_test_set_fault(STORE_TEST_FAULT_PARENT_SYNC);
  assert(game_migration_upgrade(root, primary, backup,
                                &source_profile, NULL) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_encoded_equal(&source_profile, &unchanged));
  ProfileData on_disk;
  assert(profile_load_file(primary, &on_disk) == STORE_OK);
  assert(profile_encoded_equal(&on_disk, &unchanged));
  assert(store_file_exists(source));
  assert(!store_file_exists(intent));
  GameLibraryRecord inaccessible;
  assert(game_migration_has_artifacts(root));

  assert(game_migration_upgrade(root, primary, backup, &on_disk, NULL) ==
         STORE_OK);
  assert_upgraded(&on_disk);
  assert(game_library_load(root, on_disk.normal.attempt_id, &inaccessible) ==
         STORE_OK);
  assert(game_library_load(root, on_disk.daily.attempt_id, &inaccessible) ==
         STORE_OK);
  assert(profile_load_file(source, &source_profile) == STORE_OK);
  assert(profile_encoded_equal(&source_profile, &unchanged));
  cleanup();
}

static void test_profile_backup_recovery_preserves_corrupt_primary(void) {
  prepare();
  ProfileData old = legacy_profile();
  assert(profile_save_file(primary, backup, &old) == STORE_OK);
  ProfileData changed = old;
  changed.preferences.language = LANG_ES;
  assert(profile_save_file(primary, backup, &changed) == STORE_OK);
  unsigned char corrupted[16384], checked[16384];
  size_t original_size = 0, count = 0;
  assert(store_read_file(primary, corrupted, sizeof(corrupted),
                         &original_size) == STORE_OK);
  assert(original_size > 20u);
  corrupted[original_size - 1u] ^= 0x7bu;
  assert(store_atomic_write(primary, NULL, corrupted,
                            original_size) == STORE_OK);
  ProfileData rejected;
  assert(profile_load_file(primary, &rejected) == STORE_CORRUPT);

  const char *occupied = ".sudokura-migration-test/profile.dat.pre-recovery";
  const char *evidence = ".sudokura-migration-test/profile.dat.pre-recovery.1";
  const char *next = ".sudokura-migration-test/profile.dat.pre-recovery.2";
  const unsigned char marker[] = "older-evidence";
  assert(store_atomic_write(occupied, NULL, marker,
                            sizeof(marker) - 1u) == STORE_OK);
  ProfileData restored = changed;
  store_test_fail_parent_sync_at(evidence);
  assert(profile_recover_previous(primary, backup, &restored) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_encoded_equal(&restored, &changed));
  assert(profile_load_file(primary, &rejected) == STORE_CORRUPT);
  assert(store_read_file(evidence, checked, sizeof(checked),
                         &count) == STORE_OK);
  assert(count == original_size &&
         memcmp(checked, corrupted, count) == 0);
  assert(store_file_exists(backup));

  assert(profile_recover_previous(primary, backup, &restored) == STORE_OK);
  assert(profile_encoded_equal(&restored, &old));
  assert(store_read_file(evidence, checked, sizeof(checked),
                         &count) == STORE_OK);
  assert(count == original_size &&
         memcmp(checked, corrupted, count) == 0);
  assert(store_read_file(occupied, checked, sizeof(checked),
                         &count) == STORE_OK);
  assert(count == sizeof(marker) - 1u &&
         memcmp(checked, marker, count) == 0);
  assert(!store_file_exists(next));
  ProfileData disk;
  assert(profile_load_file(primary, &disk) == STORE_OK);
  assert(profile_encoded_equal(&disk, &old));

  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(profile_recover_previous(primary, backup, &restored) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(!store_file_exists(next));
  cleanup();
}

static void test_profile_backup_recovery_preserves_empty_primary(void) {
  prepare();
  ProfileData previous = legacy_profile();
  assert(profile_save_file(primary, backup, &previous) == STORE_OK);
  ProfileData changed = previous;
  changed.preferences.language = LANG_ES;
  assert(profile_save_file(primary, backup, &changed) == STORE_OK);
  /* A physically empty primary is still evidence and must not be discarded. */
  FILE *empty = fopen(primary, "wb");
  assert(empty != NULL && fclose(empty) == 0);
  ProfileData recovered = changed;
  assert(profile_recover_previous(primary, backup, &recovered) == STORE_OK);
  assert(profile_encoded_equal(&recovered, &previous));
  unsigned char bytes[16];
  size_t count = 1;
  const char *preserved = ".sudokura-migration-test/profile.dat.pre-recovery";
  assert(store_read_file(preserved, bytes, sizeof(bytes), &count) == STORE_OK);
  assert(count == 0);
  cleanup();
}

static void test_terminal_reconcile_after_unacknowledged_profile_replace(void) {
  prepare();
  write_legacy_profile();
  ProfileData live;
  assert(profile_load_file(primary, &live) == STORE_OK);
  assert(game_migration_upgrade(root, primary, backup, &live, NULL) ==
         STORE_OK);
  ProfileData before = live;
  GameAttemptId id = live.normal.attempt_id;
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
  GameLibraryRecord record;
  assert(game_library_make_exact(&record, id, live.normal.revision + 1u,
                                 &won, true));
  assert(game_library_save(root, &record) == STORE_OK);

  store_test_fail_parent_sync_at(primary);
  assert(game_migration_reconcile(root, primary, backup, &live) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_encoded_equal(&live, &before));

  ProfileData committed, prior;
  assert(profile_load_file(primary, &committed) == STORE_OK);
  assert(profile_load_file(backup, &prior) == STORE_OK);
  assert(prior.normal.revision == before.normal.revision);
  assert(committed.normal.value.session.status == SESSION_WON);
  assert(committed.normal.revision == before.normal.revision + 1u);
  assert(committed.result_count == before.result_count + 1u);
  assert(game_attempt_id_equal(
             committed.results[committed.result_count - 1u].attempt_id, id));
  store_test_fail_parent_sync_at(primary);
  assert(game_migration_reconcile(root, primary, backup, &committed) ==
         STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(committed.result_count == before.result_count + 1u);
  ProfileData still_prior;
  assert(profile_load_file(backup, &still_prior) == STORE_OK);
  assert(profile_encoded_equal(&still_prior, &prior));
  cleanup();
}

int main(void) {
  cleanup();
  test_idempotent_import_and_newer_revision();
  test_receipt_survives_source_rollback();
  test_durable_delete_receipt_survives_profile_write_failures();
  test_failure_preserves_source_and_retry();
  test_immutable_preimage_collision();
  test_record_first_failure_recovers_terminal_and_recent_result();
  test_existing_library_blocks_fresh_migration();
  test_missing_hot_pair_recovers_verified_intent();
  test_intent_recovery_rejects_tampered_immutable_pair();
  test_explicit_collection_recovery_and_ambiguity();
  test_collection_recovery_isolates_bad_and_previous_only();
  test_preimage_write_fault_matrix();
  test_immutable_intent_cutpoint_replays();
  test_repeated_terminal_summaries_are_distinct_attempts();
  test_legacy_v1_rich_state_survives_migration_and_recovery();
  test_unacknowledged_immutable_source_replays_safely();
  test_profile_backup_recovery_preserves_corrupt_primary();
  test_profile_backup_recovery_preserves_empty_primary();
  test_terminal_reconcile_after_unacknowledged_profile_replace();
  cleanup();
  puts("immutable v1 import, replay-safe IDs, cut-point retry and receipts passed");
  return 0;
}
