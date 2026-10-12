#define SUDOKURA_STORE_TESTING 1
#include "game_library.h"
#include "seed.h"
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

static const char *root = ".sudokura-library-test";

static GameAttemptId make_id(uint64_t number) {
  GameAttemptId id = {UINT64_C(0x123456789abcdef0), number};
  return id;
}
static bool cleanup_visit(const char *name, void *context) {
  const char *directory = (const char *)context;
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  int length = snprintf(path, sizeof(path), "%s/%s", directory, name);
  assert(length >= 0 && (size_t)length < sizeof(path));
  assert(store_remove_file(path));
  return true;
}
static void cleanup(void) {
  char directory[128];
  int length = snprintf(directory, sizeof(directory), "%s/games", root);
  assert(length >= 0 && (size_t)length < sizeof(directory));
  StoreStatus status = store_list_directory(directory, cleanup_visit,
                                          directory);
  assert(status == STORE_OK || status == STORE_NOT_FOUND);
  (void)test_rmdir(directory);
  (void)test_rmdir(root);
}
static void prepare(void) {
  cleanup();
  assert(store_ensure_directory(root) == STORE_OK);
}

static ProfileSession make_session(SessionStatus status) {
  SessionState state;
  memset(&state, 0, sizeof(state));
  assert(game_new_difficulty_revision(
      &state.game, UINT64_C(0x445566778899aabb), DIFFICULTY_MEDIUM,
      SUDOKURA_GENERATOR_REVISION_LEGACY));
  state.mode = MODE_CLASSIC;
  state.selected_row = 4;
  state.selected_column = 4;
  state.status = status;
  state.elapsed_ms = UINT64_C(14000);
  if (status == SESSION_WON) {
    for (size_t i = 0; i < 81; ++i) {
      state.game.puzzle[i] = state.game.solution[i];
      state.game.notes[i] = 0;
    }
  }
  assert(session_validate(&state));
  ProfileSlot slot;
  assert(profile_slot_set(&slot, &state, false));
  return slot.value;
}

static void test_hex_and_collision(void) {
  GameAttemptId id = make_id(1);
  char hex[33];
  game_attempt_id_hex(id, hex);
  assert(strlen(hex) == 32);
  GameAttemptId parsed = {0, 0};
  assert(game_attempt_id_parse(hex, &parsed));
  assert(game_attempt_id_equal(id, parsed));
  hex[0] = 'A';
  assert(!game_attempt_id_parse(hex, &parsed));
  assert(!game_attempt_id_parse("00000000000000000000000000000000",
                                 &parsed));
  GameAttemptId reserved[] = {id};
  GameAttemptId random;
  assert(game_library_new_id(root, reserved, 1, &random));
  assert(game_attempt_id_valid(random));
  assert(!game_attempt_id_equal(random, id));
}

static void test_long_paths_fail_closed(void) {
  char long_root[SUDOKURA_STORE_PATH_CAPACITY + 40u];
  memset(long_root, 'x', sizeof(long_root));
  long_root[sizeof(long_root) - 1u] = '\0';
  GameAttemptId id = make_id(7);
  GameLibraryRecord record;
  assert(game_library_receipt_status(long_root, id) == STORE_IO_ERROR);
  assert(game_library_load(long_root, id, &record) == STORE_IO_ERROR);
  assert(!game_library_new_id(long_root, NULL, 0u, &id));
  assert(game_library_scan(long_root, NULL, NULL) == STORE_IO_ERROR);
}

static void test_codec_and_atomic_decode(void) {
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameLibraryRecord input;
  assert(game_library_make_exact(&input, make_id(1), 1, &active, true));
  input.created_utc = UINT64_C(1700000000);
  input.updated_utc = UINT64_C(1700000001);
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t count = 0;
  assert(game_library_encode(&input, bytes, &count));
  assert(count <= SUDOKURA_GAME_RECORD_MAX_BYTES);
  assert(count < 4096);

  GameLibraryEntry metadata;
  assert(game_library_peek(bytes, count, &metadata) == STORE_OK);
  assert(metadata.revision == 1 && metadata.result.status == SESSION_ACTIVE);
  GameLibraryRecord output;
  memset(&output, 0x63, sizeof(output));
  assert(game_library_decode(bytes, count, &output) == STORE_OK);
  assert(output.result.elapsed_ms == UINT64_C(14000));
  assert(output.exact.session.game.seed == active.session.game.seed);
  assert(output.created_utc == input.created_utc);

  GameLibraryRecord unchanged = output;
  bytes[count - 1] ^= 0x45;
  assert(game_library_decode(bytes, count, &output) == STORE_CORRUPT);
  assert(memcmp(&output, &unchanged, sizeof(output)) == 0);
  bytes[count - 1] ^= 0x45;
  bytes[8] = 2;
  assert(game_library_decode(bytes, count, &output) == STORE_INCOMPATIBLE);
  assert(memcmp(&output, &unchanged, sizeof(output)) == 0);
  bytes[8] = 1;
  assert(game_library_decode(bytes, count - 1, &output) == STORE_CORRUPT);

  ProfileResult won = {
      .seed = UINT64_C(987), .generator_revision = SUDOKURA_GENERATOR_REVISION,
      .difficulty = DIFFICULTY_MEDIUM, .mode = MODE_CLASSIC,
      .status = SESSION_WON, .elapsed_ms = UINT64_C(54000),
  };
  GameLibraryRecord legacy;
  assert(game_library_make_legacy(&legacy, make_id(2), &won));
  assert(game_library_encode(&legacy, bytes, &count));
  assert(game_library_decode(bytes, count, &output) == STORE_OK);
  assert(output.kind == GAME_RECORD_LEGACY_SUMMARY);
  assert(output.result.seed == won.seed);
  assert(!output.created_utc && !output.finished_utc);
}

static void test_save_revision_and_previous_recovery(void) {
  prepare();
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameLibraryRecord record;
  GameAttemptId id = make_id(3);
  assert(game_library_make_exact(&record, id, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(game_library_save(root, &record) == STORE_OK);
  GameLibraryRecord recovered;
  assert(game_library_load(root, id, &recovered) == STORE_OK);
  assert(recovered.revision == 1);

  active.session.elapsed_ms += 1000;
  GameLibraryRecord updated;
  assert(game_library_make_exact(&updated, id, 2, &active, true));
  assert(game_library_save(root, &updated) == STORE_OK);
  assert(game_library_load(root, id, &recovered) == STORE_OK);
  assert(recovered.revision == 2);
  assert(game_library_save(root, &record) != STORE_OK);

  char primary[128];
  (void)snprintf(primary, sizeof(primary),
                 "%s/games/123456789abcdef00000000000000003.dat", root);
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t count = 0;
  assert(store_read_file(primary, bytes, sizeof(bytes), &count) == STORE_OK);
  bytes[count - 1] ^= 0x55;
  assert(store_atomic_write(primary, NULL, bytes, count) == STORE_OK);
  assert(game_library_load(root, id, &recovered) == STORE_CORRUPT);
  assert(game_library_save(root, &updated) != STORE_OK);
  assert(game_library_recover_previous(root, id, &recovered) == STORE_OK);
  assert(recovered.revision == 1);
  assert(game_library_load(root, id, &recovered) == STORE_OK);
  assert(recovered.revision == 1);
  char preserved[160];
  (void)snprintf(preserved, sizeof(preserved),
                 "%s/games/123456789abcdef00000000000000003.dat.pre-recovery",
                 root);
  unsigned char original[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t original_size = 0;
  assert(store_read_file(preserved, original, sizeof(original),
                         &original_size) == STORE_OK);
  assert(original_size == count && memcmp(original, bytes, count) == 0);
  cleanup();
}

typedef struct {
  unsigned count;
  bool saw_backup;
  bool cancelled;
} ScanProbe;

static bool scan_callback(const GameLibraryEntry *entry, void *context) {
  ScanProbe *probe = (ScanProbe *)context;
  ++probe->count;
  if (entry->only_backup) {
    assert(entry->status == STORE_NOT_FOUND);
    probe->saw_backup = true;
  } else {
    assert(entry->status == STORE_OK);
  }
  return !probe->cancelled;
}

static void test_backup_only_scan_and_recovery(void) {
  prepare();
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameLibraryRecord record;
  GameAttemptId id = make_id(4);
  assert(game_library_make_exact(&record, id, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  active.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, id, 2, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  char path[128];
  (void)snprintf(path, sizeof(path),
                 "%s/games/123456789abcdef00000000000000004.dat", root);
  assert(store_remove_file(path));

  ScanProbe probe = {0, false, false};
  assert(game_library_scan(root, scan_callback, &probe) == STORE_OK);
  assert(probe.count == 1 && probe.saw_backup);
  GameLibraryRecord recovered;
  assert(game_library_load(root, id, &recovered) == STORE_NOT_FOUND);
  assert(game_library_save(root, &record) != STORE_OK);
  assert(game_library_recover_previous(root, id, &recovered) == STORE_OK);
  assert(recovered.revision == 1);
  cleanup();
}

static void test_delete_receipt_prevents_resurrection(void) {
  prepare();
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameAttemptId id = make_id(5);
  GameLibraryRecord record;
  assert(game_library_make_exact(&record, id, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  unsigned char old[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t bytes = 0;
  assert(game_library_encode(&record, old, &bytes));

  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_delete(root, id) == STORE_IO_ERROR);
  assert(game_library_receipt_status(root, id) == STORE_NOT_FOUND);
  assert(game_library_load(root, id, &record) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_NONE);

  assert(game_library_delete(root, id) == STORE_OK);
  assert(game_library_delete(root, id) == STORE_OK);
  assert(game_library_receipt_status(root, id) == STORE_OK);
  assert(game_library_load(root, id, &record) == STORE_NOT_FOUND);
  ScanProbe probe = {0, false, false};
  assert(game_library_scan(root, scan_callback, &probe) == STORE_OK);
  assert(probe.count == 0);

  char leftover[128];
  (void)snprintf(leftover, sizeof(leftover),
                 "%s/games/123456789abcdef00000000000000005.dat.bak", root);
  assert(store_atomic_write(leftover, NULL, old, bytes) == STORE_OK);
  assert(game_library_load(root, id, &record) == STORE_NOT_FOUND);
  assert(game_library_recover_previous(root, id, &record) == STORE_NOT_FOUND);
  assert(game_library_scan(root, scan_callback, &probe) == STORE_OK);
  assert(probe.count == 0);
  cleanup();
}

static void test_unbounded_wins_and_scan_cancellation(void) {
  prepare();
  ProfileSession won = make_session(SESSION_WON);
  for (uint64_t n = 1; n <= 201; ++n) {
    GameLibraryRecord record;
    assert(game_library_make_exact(&record, make_id(n), 1, &won, false));
    assert(game_library_save(root, &record) == STORE_OK);
    assert(game_library_save(root, &record) == STORE_OK);
  }
  ScanProbe probe = {0, false, false};
  assert(game_library_scan(root, scan_callback, &probe) == STORE_OK);
  assert(probe.count == 201);

  probe = (ScanProbe){0, false, true};
  assert(game_library_scan(root, scan_callback, &probe) == STORE_CANCELLED);
  assert(probe.count == 1);

  GameLibraryRecord reread;
  assert(game_library_load(root, make_id(201), &reread) == STORE_OK);
  assert(reread.result.status == SESSION_WON);
  assert(reread.revision == 1);
  cleanup();
}

static void test_live_checkpoint_avoids_puzzle_regeneration(void) {
  prepare();
  ProfileSession live = make_session(SESSION_ACTIVE);
  GameAttemptId id = make_id(702);
  GameLibraryHotCache cache = {0};
  GameLibraryRecord record;

  game_test_reset_generation_count();
  for (uint64_t revision = 1; revision <= 40; ++revision) {
    live.session.elapsed_ms += 1000;
    assert(game_library_make_exact_runtime(&record, id, revision,
                                           &live, true));
    assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
    assert(game_test_generation_count() == 0);
    assert(cache.valid && cache.revision == revision);
  }
  /* Idempotent retries must not replace a terminal/retained record. */
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_NONE);

  GameLibraryRecord checked;
  assert(game_library_load(root, id, &checked) == STORE_OK);
  assert(game_test_generation_count() > 0);
  assert(checked.revision == 40);
  assert(checked.exact.session.elapsed_ms == live.session.elapsed_ms);

  char name[33], primary[SUDOKURA_STORE_PATH_CAPACITY];
  game_attempt_id_hex(id, name);
  int written = snprintf(primary, sizeof(primary), "%s/games/%s.dat",
                         root, name);
  assert(written > 0 && (size_t)written < sizeof(primary));
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t size = 0;
  assert(store_read_file(primary, bytes, sizeof(bytes), &size) == STORE_OK);
  bytes[size - 1] ^= 0x4a;
  assert(store_atomic_write(primary, NULL, bytes, size) == STORE_OK);

  live.session.elapsed_ms += 1000;
  assert(game_library_make_exact_runtime(&record, id, 41, &live, true));
  assert(game_library_save_runtime(root, &record, &cache) == STORE_CORRUPT);
  assert(!cache.valid);
  assert(game_library_load(root, id, &checked) == STORE_CORRUPT);
  cleanup();
}

static void test_hot_write_failure_keeps_last_good_revision(void) {
  prepare();
  ProfileSession live = make_session(SESSION_ACTIVE);
  GameAttemptId id = make_id(704);
  GameLibraryHotCache cache = {0};
  GameLibraryRecord record;
  assert(game_library_make_exact_runtime(&record, id, 1, &live, true));
  assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);

  live.session.elapsed_ms += 1000;
  assert(game_library_make_exact_runtime(&record, id, 2, &live, true));
  game_test_reset_generation_count();
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_save_runtime(root, &record, &cache) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(cache.valid && cache.revision == 1);
  assert(game_test_generation_count() == 0);

  GameLibraryRecord from_disk;
  assert(game_library_load(root, id, &from_disk) == STORE_OK);
  assert(from_disk.revision == 1);
  game_test_reset_generation_count();
  assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
  assert(cache.revision == 2);
  assert(game_test_generation_count() == 0);
  assert(game_library_load(root, id, &from_disk) == STORE_OK);
  assert(from_disk.revision == 2);
  cleanup();
}

static void test_terminal_hot_retry_is_immutable(void) {
  prepare();
  ProfileSession won = make_session(SESSION_WON);
  GameAttemptId id = make_id(703);
  GameLibraryHotCache cache = {0};
  GameLibraryRecord record;
  assert(game_library_make_exact_runtime(&record, id, 1, &won, true));
  game_test_reset_generation_count();
  assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  won.session.elapsed_ms += 1000;
  assert(game_library_make_exact_runtime(&record, id, 2, &won, true));
  assert(game_library_save_runtime(root, &record, &cache) == STORE_IO_ERROR);
  assert(game_test_generation_count() == 0);
  GameLibraryRecord reopened;
  assert(game_library_load(root, id, &reopened) == STORE_OK);
  assert(reopened.revision == 1);
  cleanup();
}

typedef struct {
  unsigned healthy, damaged, incompatible;
  bool recovery_available;
} MixedScanProbe;

static bool mixed_scan_callback(const GameLibraryEntry *entry, void *context) {
  MixedScanProbe *probe = (MixedScanProbe *)context;
  if (entry->status == STORE_OK) {
    ++probe->healthy;
  } else if (entry->status == STORE_CORRUPT) {
    ++probe->damaged;
    if (entry->previous_status == STORE_OK)
      probe->recovery_available = true;
  } else if (entry->status == STORE_INCOMPATIBLE) {
    ++probe->incompatible;
  } else {
    assert(!"unexpected catalogue status");
  }
  return true;
}

static void test_mixed_record_health_and_oversized_future_envelope(void) {
  prepare();
  ProfileSession live = make_session(SESSION_ACTIVE);
  GameLibraryRecord record;
  GameAttemptId good = make_id(800), broken = make_id(801);
  GameAttemptId future = make_id(802);
  assert(game_library_make_exact(&record, good, 1, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(game_library_make_exact(&record, broken, 1, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);
  live.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, broken, 2, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);
  char broken_path[128], future_path[128];
  (void)snprintf(broken_path, sizeof(broken_path),
                 "%s/games/123456789abcdef00000000000000321.dat", root);
  (void)snprintf(future_path, sizeof(future_path),
                 "%s/games/123456789abcdef00000000000000322.dat", root);
  unsigned char existing[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t current = 0;
  assert(store_read_file(broken_path, existing, sizeof(existing),
                         &current) == STORE_OK);
  existing[current - 1u] ^= 0x40u;
  assert(store_atomic_write(broken_path, NULL, existing, current) == STORE_OK);

  unsigned char oversized[SUDOKURA_GAME_RECORD_MAX_BYTES + 16u] = {0};
  memcpy(oversized, "SUDOGAME", 8u);
  oversized[8] = 2u; /* recognized, unsupported container */
  assert(store_atomic_write(future_path, NULL, oversized,
                            sizeof(oversized)) == STORE_OK);
  assert(game_library_load(root, future, &record) == STORE_INCOMPATIBLE);
  MixedScanProbe probe = {0};
  assert(game_library_scan(root, mixed_scan_callback, &probe) == STORE_OK);
  assert(probe.healthy == 1 && probe.damaged == 1 &&
         probe.incompatible == 1 && probe.recovery_available);
  assert(game_library_load(root, good, &record) == STORE_OK);
  assert(record.revision == 1);

  oversized[8] = 1u;
  oversized[20] = 2u; /* recognized, unsupported payload */
  assert(store_atomic_write(future_path, NULL, oversized,
                            sizeof(oversized)) == STORE_OK);
  assert(game_library_load(root, future, &record) == STORE_INCOMPATIBLE);
  oversized[20] = 1u; /* known oversized format is corrupt */
  assert(store_atomic_write(future_path, NULL, oversized,
                            sizeof(oversized)) == STORE_OK);
  assert(game_library_load(root, future, &record) == STORE_CORRUPT);
  assert(game_library_recover_previous(root, broken, &record) == STORE_OK);
  assert(record.revision == 1);
  cleanup();
}

static bool count_metadata(const GameLibraryEntry *entry, void *context) {
  size_t *count = (size_t *)context;
  assert(entry->status == STORE_OK);
  assert(entry->kind == GAME_RECORD_LEGACY_SUMMARY);
  ++*count;
  return true;
}

static void test_thousand_record_incremental_scan(void) {
  prepare();
  ProfileResult result = {
      .seed = UINT64_C(10000),
      .generator_revision = SUDOKURA_GENERATOR_REVISION,
      .difficulty = DIFFICULTY_EASY,
      .mode = MODE_CLASSIC,
      .status = SESSION_WON,
      .elapsed_ms = UINT64_C(12000),
  };
  for (uint64_t i = 0; i < UINT64_C(1000); ++i) {
    GameLibraryRecord record;
    result.seed = UINT64_C(10000) + i;
    assert(game_library_make_legacy(&record, make_id(10000 + i), &result));
    assert(game_library_save(root, &record) == STORE_OK);
  }
  size_t count = 0;
  assert(game_library_scan(root, count_metadata, &count) == STORE_OK);
  assert(count == 1000u);
  count = 0;
  /* User cancellation must not allocate a list or scan the entire folder. */
  assert(game_library_scan(root, count_metadata, &count) == STORE_OK);
  assert(count == 1000u);
  ScanProbe cancel = {0, false, true};
  assert(game_library_scan(root, scan_callback, &cancel) == STORE_CANCELLED);
  assert(cancel.count == 1u);
  cleanup();
}

static void test_recovery_copy_collision_is_lossless(void) {
  prepare();
  ProfileSession live = make_session(SESSION_ACTIVE);
  GameAttemptId id = make_id(512);
  GameLibraryRecord record;
  assert(game_library_make_exact(&record, id, 1, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);
  live.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, id, 2, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);
  char hex[33], primary[160], first[160], second[160];
  game_attempt_id_hex(id, hex);
  (void)snprintf(primary, sizeof(primary), "%s/games/%s.dat", root, hex);
  (void)snprintf(first, sizeof(first),
                 "%s/games/%s.dat.pre-recovery", root, hex);
  (void)snprintf(second, sizeof(second),
                 "%s/games/%s.dat.pre-recovery.1", root, hex);

  unsigned char corrupt[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t count = 0;
  assert(store_read_file(primary, corrupt, sizeof(corrupt), &count) == STORE_OK);
  corrupt[count - 1u] ^= 0x52u;
  assert(store_atomic_write(primary, NULL, corrupt, count) == STORE_OK);
  const unsigned char sentinel[] = "older-preserved-bytes";
  assert(store_atomic_write(first, NULL, sentinel,
                            sizeof(sentinel)) == STORE_OK);

  assert(game_library_recover_previous(root, id, &record) == STORE_OK);
  assert(record.revision == 1);
  unsigned char checked[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t length = 0;
  assert(store_read_file(first, checked, sizeof(checked), &length) == STORE_OK);
  assert(length == sizeof(sentinel) &&
         memcmp(checked, sentinel, length) == 0);
  assert(store_read_file(second, checked, sizeof(checked), &length) == STORE_OK);
  assert(length == count && memcmp(checked, corrupt, count) == 0);
  cleanup();
}

static void test_missing_record_does_not_recreate_revision(void) {
  prepare();
  GameAttemptId id = make_id(513);
  ProfileSession live = make_session(SESSION_ACTIVE);
  GameLibraryRecord record;
  GameLibraryHotCache cache = {0};
  assert(game_library_make_exact(&record, id, 2, &live, true));
  assert(game_library_save(root, &record) == STORE_IO_ERROR);
  assert(game_library_save_runtime(root, &record, &cache) == STORE_IO_ERROR);
  assert(game_library_make_exact(&record, id, 1, &live, true));
  assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
  char hex[33], path[160];
  game_attempt_id_hex(id, hex);
  (void)snprintf(path, sizeof(path), "%s/games/%s.dat", root, hex);
  assert(store_remove_file(path));
  live.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, id, 2, &live, true));
  assert(game_library_save(root, &record) == STORE_IO_ERROR);
  assert(game_library_save_runtime(root, &record, &cache) == STORE_IO_ERROR);
  assert(game_library_load(root, id, &record) == STORE_NOT_FOUND);
  cleanup();
}

static void test_write_fault_matrix_keeps_last_known_good(void) {
  const StoreTestFault faults[] = {
      STORE_TEST_FAULT_OPEN, STORE_TEST_FAULT_DURING_WRITE,
      STORE_TEST_FAULT_NO_SPACE, STORE_TEST_FAULT_SYNC,
      STORE_TEST_FAULT_REPLACE};
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    prepare();
    ProfileSession live = make_session(SESSION_ACTIVE);
    GameAttemptId id = make_id(520 + i);
    GameLibraryRecord record;
    GameLibraryHotCache cache = {0};
    assert(game_library_make_exact_runtime(&record, id, 1, &live, true));
    assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
    live.session.elapsed_ms += 1000;
    assert(game_library_make_exact_runtime(&record, id, 2, &live, true));
    store_test_set_fault(faults[i]);
    assert(game_library_save_runtime(root, &record, &cache) == STORE_IO_ERROR);
    store_test_set_fault(STORE_TEST_FAULT_NONE);
    assert(cache.valid && cache.revision == 1);
    GameLibraryRecord verified;
    assert(game_library_load(root, id, &verified) == STORE_OK);
    assert(verified.revision == 1);
    assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
    assert(game_library_load(root, id, &verified) == STORE_OK);
    assert(verified.revision == 2);
    cleanup();
  }
}

typedef struct {
  unsigned healthy;
  unsigned corrupt;
} ReceiptHealth;

static bool receipt_health_visit(const GameLibraryEntry *entry,
                                 void *context) {
  ReceiptHealth *health = (ReceiptHealth *)context;
  if (entry->status == STORE_OK) ++health->healthy;
  else if (entry->status == STORE_CORRUPT) ++health->corrupt;
  else assert(!"unexpected receipt status");
  return true;
}

static void test_corrupt_receipt_isolates_one_attempt(void) {
  prepare();
  ProfileSession live = make_session(SESSION_ACTIVE);
  GameAttemptId damaged = make_id(531);
  GameAttemptId healthy = make_id(532);
  GameLibraryRecord record;
  assert(game_library_make_exact(&record, damaged, 1, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(game_library_make_exact(&record, healthy, 1, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);

  char hex[33], receipt[160];
  game_attempt_id_hex(damaged, hex);
  (void)snprintf(receipt, sizeof(receipt),
                 "%s/games/%s.deleted", root, hex);
  const unsigned char bogus[34] = {0};
  assert(store_atomic_write(receipt, NULL, bogus, sizeof(bogus)) == STORE_OK);
  assert(game_library_receipt_status(root, damaged) == STORE_CORRUPT);
  assert(game_library_load(root, damaged, &record) == STORE_CORRUPT);
  assert(game_library_delete(root, damaged) == STORE_CORRUPT);
  assert(game_library_load(root, healthy, &record) == STORE_OK);
  ReceiptHealth health = {0};
  assert(game_library_scan(root, receipt_health_visit, &health) == STORE_OK);
  assert(health.healthy == 1 && health.corrupt == 1);
  cleanup();
}

int main(void) {
  cleanup();
  prepare();
  test_hex_and_collision();
  test_long_paths_fail_closed();
  test_codec_and_atomic_decode();
  cleanup();
  test_save_revision_and_previous_recovery();
  test_backup_only_scan_and_recovery();
  test_delete_receipt_prevents_resurrection();
  test_unbounded_wins_and_scan_cancellation();
  test_live_checkpoint_avoids_puzzle_regeneration();
  test_terminal_hot_retry_is_immutable();
  test_hot_write_failure_keeps_last_good_revision();
  test_mixed_record_health_and_oversized_future_envelope();
  test_thousand_record_incremental_scan();
  test_recovery_copy_collision_is_lossless();
  test_missing_record_does_not_recreate_revision();
  test_write_fault_matrix_keeps_last_known_good();
  test_corrupt_receipt_isolates_one_attempt();
  cleanup();
  puts("game record codec, revisions, backup, receipts and >128 archive passed");
  return 0;
}
