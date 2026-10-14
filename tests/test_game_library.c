#define SUDOKURA_STORE_TESTING 1
#include "game_library.h"
#include "seed.h"
#include "store_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#define test_rmdir _rmdir
#else
#include <unistd.h>
#define test_rmdir rmdir
#endif

static const char *root = ".sudokura-library-test";

/* Diagnostic elapsed time only: no machine-dependent timing assertions. */
static double wall_clock_ms(void) {
#if defined(_WIN32)
  /* MinGW targets using MSVCRT may not expose C11 timespec_get. */
  LARGE_INTEGER frequency, counter;
  if (!QueryPerformanceFrequency(&frequency) ||
      frequency.QuadPart <= 0 ||
      !QueryPerformanceCounter(&counter))
    return -1.0;
  return 1000.0 * (double)counter.QuadPart /
         (double)frequency.QuadPart;
#else
  struct timespec now;
  if (timespec_get(&now, TIME_UTC) != TIME_UTC) return -1.0;
  return (double)now.tv_sec * 1000.0 +
         (double)now.tv_nsec / 1000000.0;
#endif
}

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

static void test_saved_board_roundtrip_after_restart(void) {
  prepare();
  ProfileSession edited = make_session(SESSION_ACTIVE);
  int cells[3],count=0;
  for(int index=0;index<81 && count<3;++index)
    if(!edited.session.game.fixed[index] &&
       !edited.session.game.puzzle[index]) cells[count++]=index;
  assert(count==3);
  int a=cells[0],b=cells[1],c=cells[2];
  edited.session.game.puzzle[a]=edited.session.game.solution[a];
  edited.session.game.notes[b]=1u<<2;
  edited.session.selected_row=(uint8_t)(a/9);
  edited.session.selected_column=(uint8_t)(a%9);
  edited.session.elapsed_ms=UINT64_C(47000);
  assert(session_validate(&edited.session));

  GameAttemptId id=make_id(9951);
  ProfileData hot,loaded;
  profile_defaults(&hot);
  hot.normal.present=true;
  hot.normal.retained=true;
  hot.normal.revision=7;
  hot.normal.attempt_id=id;
  hot.normal.value=edited;
  assert(profile_validate(&hot));
  char primary[160],backup[160];
  int n=snprintf(primary,sizeof(primary),"%s/profile.dat",root);
  assert(n>0 && (size_t)n<sizeof(primary));
  n=snprintf(backup,sizeof(backup),"%s/profile.dat.bak",root);
  assert(n>0 && (size_t)n<sizeof(backup));

  GameLibraryRecord record,disk;
  assert(game_library_make_exact(&record,id,7,&edited,true));
  assert(game_library_save(root,&record)==STORE_OK);
  assert(profile_save_runtime_file(primary,backup,&hot)==STORE_OK);
  assert(game_library_verify_retained_snapshot(
      root,&hot.normal)==STORE_OK);

  /* Simulate a fresh process: load independently from both disk files. */
  assert(profile_load_file(primary,&loaded)==STORE_OK);
  assert(game_library_load(root,id,&disk)==STORE_OK);
  assert(game_attempt_id_equal(loaded.normal.attempt_id,id));
  assert(disk.exact.session.game.puzzle[a]==edited.session.game.puzzle[a]);
  assert(disk.exact.session.game.notes[b]==(1u<<2));
  assert(disk.exact.session.selected_row==(uint8_t)(a/9));
  assert(disk.exact.session.selected_column==(uint8_t)(a%9));
  assert(disk.exact.session.elapsed_ms==UINT64_C(47000));
  assert(loaded.normal.value.session.game.puzzle[a]==
         edited.session.game.solution[a]);
  assert(loaded.normal.value.session.game.notes[b]==(1u<<2));
  assert(game_library_verify_retained_snapshot(root,&loaded.normal)==STORE_OK);
  bool prefer_record=true;
  assert(game_library_resolve_hot_record(
      &loaded.normal,&disk,&prefer_record)==STORE_OK);
  assert(!prefer_record);

  /* Save means latest revision, not an immutable original board. */
  edited.session.game.notes[b]=0;
  edited.session.game.puzzle[b]=edited.session.game.solution[b];
  edited.session.game.notes[c]=1u<<5;
  edited.session.elapsed_ms=UINT64_C(88000);
  assert(session_validate(&edited.session));
  hot.normal.revision=8;
  hot.normal.value=edited;
  assert(game_library_verify_retained_snapshot(root,&hot.normal)==STORE_CORRUPT);
  assert(game_library_make_exact(&record,id,8,&edited,true));
  assert(game_library_save(root,&record)==STORE_OK);
  assert(profile_save_runtime_file(primary,backup,&hot)==STORE_OK);
  assert(profile_load_file(primary,&loaded)==STORE_OK);
  assert(game_library_load(root,id,&disk)==STORE_OK);
  assert(game_library_verify_retained_snapshot(root,&loaded.normal)==STORE_OK);
  assert(loaded.normal.value.session.game.puzzle[a]==
         edited.session.game.solution[a]);
  assert(loaded.normal.value.session.game.puzzle[b]==
         edited.session.game.solution[b]);
  assert(loaded.normal.value.session.game.notes[b]==0);
  assert(loaded.normal.value.session.game.notes[c]==(1u<<5));
  assert(disk.revision==8 && loaded.normal.revision==8);

  (void)store_remove_file(primary);
  (void)store_remove_file(backup);
  cleanup();
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

static void test_first_save_record_cuts_and_terminal_retry(void) {
  prepare();
  ProfileSlot hot = {0}, retained = {0};
  hot.present = true;
  hot.attempt_id = make_id(9841);
  hot.revision = 7;
  hot.value = make_session(SESSION_ACTIVE);
  GameLibraryHotCache cache = {0};
  retained.revision = 99;

  /* Before record replacement, no retained snapshot may be acknowledged. */
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_retain_hot_slot_runtime(
      root, &hot, &cache, false, &retained) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(retained.revision == 99 && !retained.present);
  GameLibraryRecord disk;
  assert(game_library_load(root, hot.attempt_id, &disk) == STORE_NOT_FOUND);

  char hex[33], primary[160], backup[160];
  game_attempt_id_hex(hot.attempt_id, hex);
  int n = snprintf(primary, sizeof(primary), "%s/games/%s.dat", root, hex);
  assert(n > 0 && (size_t)n < sizeof(primary));
  n = snprintf(backup, sizeof(backup), "%s/games/%s.dat.bak", root, hex);
  assert(n > 0 && (size_t)n < sizeof(backup));
  store_test_fail_parent_sync_at(primary);
  assert(game_library_retain_hot_slot_runtime(
      root, &hot, &cache, false, &retained) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(!retained.present && retained.revision == 99);
  assert(game_library_load(root, hot.attempt_id, &disk) == STORE_OK);
  assert(disk.retained && disk.revision == 8);
  assert(!store_file_exists(backup));
  assert(!hot.retained && hot.revision == 7);

  /* Retry same ID/revision: re-acknowledge directory sync without
     rotating a previous copy; no second game can be allocated. */
  assert(game_library_retain_hot_slot_runtime(
      root, &hot, &cache, false, &retained) == STORE_IO_ERROR);
  assert(game_library_retain_hot_slot_runtime(
      root, &hot, &cache, true, &retained) == STORE_OK);
  assert(retained.retained && retained.revision == 8);
  assert(game_attempt_id_equal(retained.attempt_id, hot.attempt_id));
  assert(!store_file_exists(backup));

  /* Input staged while Save was pending may advance the same attempt. */
  ProfileSlot progressed = hot;
  progressed.revision = 8;
  progressed.value.session.elapsed_ms += UINT64_C(3000);
  progressed.value.session.selected_row = 2;
  assert(game_library_retain_hot_slot_runtime(
      root, &progressed, &cache, true, &retained) == STORE_OK);
  assert(retained.revision == 9);
  assert(game_library_load(root, hot.attempt_id, &disk) == STORE_OK);
  assert(disk.revision == 9 &&
         disk.exact.session.elapsed_ms == progressed.value.session.elapsed_ms);
  assert(store_file_exists(backup));

  /* The earlier valid record must survive a repeat acknowledgement. */
  unsigned char before[SUDOKURA_GAME_RECORD_MAX_BYTES];
  unsigned char after[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t before_size = 0, after_size = 0;
  assert(store_read_file(backup, before, sizeof(before),
                         &before_size) == STORE_OK);
  assert(game_library_retain_hot_slot_runtime(
      root, &progressed, &cache, true, &retained) == STORE_OK);
  assert(retained.revision == 9);
  assert(store_read_file(backup, after, sizeof(after),
                         &after_size) == STORE_OK);
  assert(before_size == after_size &&
         memcmp(before, after, before_size) == 0);

  ProfileSlot wrong = progressed;
  wrong.value.session.game.seed ^= UINT64_C(1);
  retained.revision = 99;
  assert(game_library_retain_hot_slot_runtime(
      root, &wrong, &cache, true, &retained) == STORE_IO_ERROR);
  assert(retained.revision == 99);

  /* A win after a pending first Save must supersede its active revision
     without changing the attempt ID or reopening the terminal snapshot. */
  ProfileSlot won = progressed;
  won.revision = 9;
  won.value = make_session(SESSION_WON);
  won.value.session.elapsed_ms = UINT64_C(21000);
  assert(game_library_retain_hot_slot_runtime(
      root, &won, &cache, true, &retained) == STORE_OK);
  assert(retained.revision == 10 && retained.retained);
  assert(game_library_load(root, hot.attempt_id, &disk) == STORE_OK);
  assert(disk.revision == 10 && disk.result.status == SESSION_WON);
  assert(game_library_retain_hot_slot_runtime(
      root, &won, &cache, true, &retained) == STORE_OK);
  assert(retained.revision == 10);
  won.revision = 10;
  won.value.session.elapsed_ms += 1000;
  assert(game_library_retain_hot_slot_runtime(
      root, &won, &cache, true, &retained) == STORE_IO_ERROR);
  assert(game_library_load(root, hot.attempt_id, &disk) == STORE_OK);
  assert(disk.revision == 10 && disk.result.status == SESSION_WON);
  cleanup();
}

static void test_first_save_profile_ack_preserves_previous_copy(void) {
  prepare();
  ProfileData original, candidate, reread, previous;
  profile_defaults(&original);
  original.normal.present = true;
  original.normal.attempt_id = make_id(9842);
  original.normal.revision = 6;
  original.normal.value = make_session(SESSION_ACTIVE);
  assert(profile_validate(&original));

  char primary[160], backup[160];
  int n = snprintf(primary, sizeof(primary),
                   "%s/profile-retain-cut.dat", root);
  assert(n > 0 && (size_t)n < sizeof(primary));
  n = snprintf(backup, sizeof(backup),
               "%s/profile-retain-cut.dat.bak", root);
  assert(n > 0 && (size_t)n < sizeof(backup));
  assert(profile_save_runtime_file(primary, backup, &original) == STORE_OK);
  assert(!store_file_exists(backup));

  ProfileSlot retained = {0};
  GameLibraryHotCache cache = {0};
  assert(game_library_retain_hot_slot_runtime(
      root, &original.normal, &cache, false, &retained) == STORE_OK);
  candidate = original;
  candidate.normal = retained;
  assert(profile_validate(&candidate));

  /* The record is committed, but profile.dat returns an ambiguous
     post-rename failure. Previous profile bytes must remain protected. */
  store_test_fail_parent_sync_at(primary);
  assert(profile_save_runtime_file(primary, backup, &candidate) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_load_file(primary, &reread) == STORE_OK);
  assert(profile_encoded_equal(&reread, &candidate));
  assert(profile_load_file(backup, &previous) == STORE_OK);
  assert(profile_encoded_equal(&previous, &original));

  /* The in-memory hot slot remains unretained until the profile link
     is acknowledged. Retry the record and profile without backup rotation. */
  assert(!original.normal.retained && original.normal.revision == 6);
  assert(game_library_retain_hot_slot_runtime(
      root, &original.normal, &cache, true, &retained) == STORE_OK);
  assert(retained.retained && retained.revision == 7);
  assert(profile_save_runtime_file(primary, NULL, &candidate) == STORE_OK);
  assert(profile_load_file(primary, &reread) == STORE_OK);
  assert(profile_encoded_equal(&reread, &candidate));
  assert(profile_load_file(backup, &previous) == STORE_OK);
  assert(profile_encoded_equal(&previous, &original));

  char hex[33], record_backup[160];
  game_attempt_id_hex(original.normal.attempt_id, hex);
  n = snprintf(record_backup, sizeof(record_backup),
               "%s/games/%s.dat.bak", root, hex);
  assert(n > 0 && (size_t)n < sizeof(record_backup));
  assert(!store_file_exists(record_backup));

  assert(store_remove_file(primary));
  assert(store_remove_file(backup));
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

  /* A cut after durable receipt publication may leave BOTH old copies.
     Neither must resurrect the deleted attempt. Retrying cleans them. */
  char stale_primary[128];
  (void)snprintf(stale_primary, sizeof(stale_primary),
                 "%s/games/123456789abcdef00000000000000005.dat", root);
  assert(store_atomic_write(stale_primary, NULL, old, bytes) == STORE_OK);
  assert(game_library_load(root, id, &record) == STORE_NOT_FOUND);
  assert(game_library_save(root, &record) == STORE_IO_ERROR);
  assert(game_library_scan(root, scan_callback, &probe) == STORE_OK);
  assert(probe.count == 0);
  assert(game_library_delete(root, id) == STORE_OK);
  assert(!store_file_exists(stale_primary));
  assert(!store_file_exists(leftover));
  assert(game_library_receipt_status(root, id) == STORE_OK);
  cleanup();
}

static void test_delete_receipt_fault_matrix(void) {
  const StoreTestFault faults[] = {
      STORE_TEST_FAULT_OPEN, STORE_TEST_FAULT_DURING_WRITE,
      STORE_TEST_FAULT_NO_SPACE, STORE_TEST_FAULT_SYNC,
      STORE_TEST_FAULT_REPLACE};
  for (size_t index = 0; index < sizeof(faults) / sizeof(faults[0]);
       ++index) {
    prepare();
    ProfileSession active = make_session(SESSION_ACTIVE);
    GameAttemptId id = make_id(710 + index);
    GameLibraryRecord record;
    assert(game_library_make_exact(&record, id, 1, &active, true));
    assert(game_library_save(root, &record) == STORE_OK);
    active.session.elapsed_ms += 1000;
    assert(game_library_make_exact(&record, id, 2, &active, true));
    assert(game_library_save(root, &record) == STORE_OK);

    store_test_set_fault(faults[index]);
    assert(game_library_delete(root, id) == STORE_IO_ERROR);
    store_test_set_fault(STORE_TEST_FAULT_NONE);
    assert(game_library_receipt_status(root, id) == STORE_NOT_FOUND);
    GameLibraryRecord loaded;
    assert(game_library_load(root, id, &loaded) == STORE_OK);
    assert(loaded.revision == 2);
    ScanProbe scan = {0, false, false};
    assert(game_library_scan(root, scan_callback, &scan) == STORE_OK);
    assert(scan.count == 1);
    assert(game_library_delete(root, id) == STORE_OK);
    assert(game_library_receipt_status(root, id) == STORE_OK);
    assert(game_library_load(root, id, &loaded) == STORE_NOT_FOUND);
    cleanup();
  }
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
  double checkpoints_start = wall_clock_ms();
  for (uint64_t revision = 1; revision <= 40; ++revision) {
    live.session.elapsed_ms += 1000;
    assert(game_library_make_exact_runtime(&record, id, revision,
                                           &live, true));
    assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
    assert(game_test_generation_count() == 0);
    assert(cache.valid && cache.revision == revision);
  }
  double checkpoints_end = wall_clock_ms();
  if (checkpoints_start >= 0 && checkpoints_end >= checkpoints_start)
    printf("40 hot checkpoints wall time: %.2f ms\n",
           checkpoints_end - checkpoints_start);
  /* Idempotent retries must not replace a terminal/retained record. */
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
  store_test_set_fault(STORE_TEST_FAULT_NONE);

  GameLibraryRecord checked;
  double load_start = wall_clock_ms();
  assert(game_library_load(root, id, &checked) == STORE_OK);
  double load_end = wall_clock_ms();
  if (load_start >= 0 && load_end >= load_start)
    printf("Canonical full-record load wall time: %.2f ms\n",
           load_end - load_start);
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
  game_test_reset_generation_count();
  clock_t started = clock();
  double scan_start = wall_clock_ms();
  assert(game_library_scan(root, count_metadata, &count) == STORE_OK);
  double scan_end = wall_clock_ms();
  clock_t finished = clock();
  assert(count == 1000u);
  assert(game_test_generation_count() == 0u);
  if (started != (clock_t)-1 && finished != (clock_t)-1 &&
      finished >= started) {
    printf("1000-entry metadata scan CPU time: %.2f ms\n",
           1000.0 * (double)(finished - started) / CLOCKS_PER_SEC);
  }
  if (scan_start >= 0 && scan_end >= scan_start)
    printf("1000-entry metadata scan wall time: %.2f ms\n",
           scan_end - scan_start);
  count = 0;
  /* User cancellation must not allocate a list or scan the entire folder. */
  assert(game_library_scan(root, count_metadata, &count) == STORE_OK);
  assert(count == 1000u);
  ScanProbe cancel = {0, false, true};
  assert(game_library_scan(root, scan_callback, &cancel) == STORE_CANCELLED);
  assert(cancel.count == 1u);

  /* Drive the same thousand-file catalogue through bounded UI-sized batches. */
  GameLibraryScanCursor cursor = {0};
  assert(game_library_scan_begin(root, &cursor) == STORE_OK);
  assert(!cursor.finished);
  size_t streamed = 0;
  unsigned frames = 0;
  bool complete = false;
  do {
    assert(++frames <= 125u);
    assert(game_library_scan_step(&cursor, 16u, count_metadata,
                                  &streamed, &complete) == STORE_OK);
  } while (!complete);
  assert(streamed == 1000u && frames > 1u);
  assert(game_test_generation_count() == 0u);
  game_library_scan_close(&cursor);
  game_library_scan_close(&cursor);

  GameLibraryScanCursor interrupted = {0};
  assert(game_library_scan_begin(root, &interrupted) == STORE_OK);
  ScanProbe stop = {0, false, true};
  assert(game_library_scan_step(&interrupted, 16u, scan_callback,
                                &stop, &complete) == STORE_CANCELLED);
  assert(complete && stop.count == 1u);
  game_library_scan_close(&interrupted);

  GameLibraryScanCursor missing = {0};
  assert(game_library_scan_begin(".sudokura-missing-library-test", &missing)
         == STORE_OK);
  assert(missing.finished);
  game_library_scan_close(&missing);
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

static void test_recover_empty_corrupt_primary_losslessly(void) {
  prepare();
  ProfileSession live = make_session(SESSION_ACTIVE);
  GameAttemptId id = make_id(620);
  GameLibraryRecord record;
  assert(game_library_make_exact(&record, id, 1, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);
  live.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, id, 2, &live, true));
  assert(game_library_save(root, &record) == STORE_OK);

  char hex[33], primary[160], backup[160], original[180];
  game_attempt_id_hex(id, hex);
  (void)snprintf(primary, sizeof(primary), "%s/games/%s.dat", root, hex);
  (void)snprintf(backup, sizeof(backup), "%s/games/%s.dat.bak", root, hex);
  (void)snprintf(original, sizeof(original),
                 "%s/games/%s.dat.pre-recovery", root, hex);
  /* Simulate an interrupted external truncation, not a valid empty record. */
  FILE *file = fopen(primary, "wb");
  assert(file != NULL);
  assert(fclose(file) == 0);
  assert(game_library_load(root, id, &record) == STORE_CORRUPT);
  assert(store_copy_once(primary, original) == STORE_CORRUPT);
  assert(!store_file_exists(original));

  /* The preservation step itself must be atomic: no false recovery on
     failed replace and no loss of either the corrupt source or good .bak. */
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_recover_previous(root, id, &record) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t count = 1;
  assert(store_read_file(primary, bytes, sizeof(bytes), &count) == STORE_OK);
  assert(count == 0);
  assert(!store_file_exists(original));
  assert(store_file_exists(backup));

  assert(game_library_recover_previous(root, id, &record) == STORE_OK);
  assert(record.revision == 1);
  assert(store_read_file(original, bytes, sizeof(bytes), &count) == STORE_OK);
  assert(count == 0);
  assert(game_library_load(root, id, &record) == STORE_OK);
  assert(record.revision == 1);
  assert(store_file_exists(backup));
  cleanup();
}

static void test_first_archive_after_unretained_hot_revisions(void) {
  prepare();
  ProfileSession won = make_session(SESSION_WON);
  GameAttemptId id = make_id(513);
  GameLibraryRecord record;
  GameLibraryHotCache cache = {0};

  /* Unretained Continue checkpoints can advance the profile revision
     before the first archive is ever created. */
  assert(game_library_make_exact_runtime(&record, id, 17, &won, true));
  assert(game_library_save_runtime(root, &record, &cache) == STORE_OK);
  assert(cache.valid && cache.revision == 17);
  GameLibraryRecord readback;
  assert(game_library_load(root, id, &readback) == STORE_OK);
  assert(readback.revision == 17 && readback.result.status == SESSION_WON);

  /* A committed record disappearing in the same live session cannot
     silently be recreated from its stale hot cache. */
  char hex[33], path[160];
  game_attempt_id_hex(id, hex);
  (void)snprintf(path, sizeof(path), "%s/games/%s.dat", root, hex);
  assert(store_remove_file(path));
  assert(game_library_save_runtime(root, &record, &cache) == STORE_IO_ERROR);
  assert(!cache.valid);
  assert(game_library_load(root, id, &readback) == STORE_NOT_FOUND);

  ProfileSession active = make_session(SESSION_ACTIVE);
  GameAttemptId second_id = make_id(514);
  GameLibraryHotCache next_cache = {0};
  assert(game_library_make_exact_runtime(&record, second_id, 8, &active, true));
  assert(game_library_save_runtime(root, &record, &next_cache) == STORE_OK);
  assert(game_library_load(root, second_id, &readback) == STORE_OK);
  assert(readback.revision == 8 && readback.result.status == SESSION_ACTIVE);
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

typedef struct {
  GameAttemptId id;
  uint64_t created, updated, finished;
  unsigned count;
} DateScan;

static bool scan_date(const GameLibraryEntry *item, void *context) {
  DateScan *probe = (DateScan *)context;
  assert(game_attempt_id_equal(item->id, probe->id));
  assert(item->status == STORE_OK);
  assert(item->created_utc == probe->created);
  assert(item->updated_utc == probe->updated);
  assert(item->finished_utc == probe->finished);
  ++probe->count;
  return true;
}

static void test_runtime_utc_dates_and_unknown_legacy(void) {
  prepare();
  GameAttemptId id = make_id(571);
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameLibraryHotCache cache = {0};
  GameLibraryRecord record, readback;
  const uint64_t created = UINT64_C(1700001000);
  assert(game_library_make_exact_runtime(&record, id, 1, &active, true));
  assert(game_library_save_runtime_at(root, &record, &cache, created) ==
         STORE_OK);
  assert(game_library_load(root, id, &readback) == STORE_OK);
  assert(readback.created_utc == created && readback.updated_utc == created);
  assert(readback.finished_utc == 0);

  active.session.elapsed_ms += 1000;
  assert(game_library_make_exact_runtime(&record, id, 2, &active, true));
  assert(game_library_save_runtime_at(root, &record, &cache, created + 30) ==
         STORE_OK);
  assert(game_library_load(root, id, &readback) == STORE_OK);
  assert(readback.created_utc == created &&
         readback.updated_utc == created + 30 &&
         readback.finished_utc == 0);

  /* A cold session resumes the original creation time and validates disk. */
  memset(&cache, 0, sizeof(cache));
  active.session.elapsed_ms += 1000;
  assert(game_library_make_exact_runtime(&record, id, 3, &active, true));
  assert(game_library_save_runtime_at(root, &record, &cache, created + 60) ==
         STORE_OK);
  assert(game_library_load(root, id, &readback) == STORE_OK);
  assert(readback.created_utc == created &&
         readback.updated_utc == created + 60);

  ProfileSession won = make_session(SESSION_WON);
  won.session.elapsed_ms = active.session.elapsed_ms + 1000;
  assert(game_library_make_exact_runtime(&record, id, 4, &won, true));
  assert(game_library_save_runtime_at(root, &record, &cache, created + 90) ==
         STORE_OK);
  assert(game_library_load(root, id, &readback) == STORE_OK);
  assert(readback.created_utc == created &&
         readback.updated_utc == created + 90 &&
         readback.finished_utc == created + 90);

  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_save_runtime_at(root, &record, &cache, created + 900) ==
         STORE_OK); /* Retry cannot alter a terminal timestamp. */
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(game_library_load(root, id, &readback) == STORE_OK);
  assert(readback.finished_utc == created + 90 &&
         readback.updated_utc == created + 90);
  DateScan probe = {id, created, created + 90, created + 90, 0};
  assert(game_library_scan(root, scan_date, &probe) == STORE_OK);
  assert(probe.count == 1);

  readback.updated_utc = created - 1;
  assert(!game_library_record_validate(&readback));
  readback.updated_utc = created + 90;
  readback.finished_utc = created + 91;
  assert(!game_library_record_validate(&readback));

  GameAttemptId old_id = make_id(572);
  ProfileResult result = {
      .seed = UINT64_C(98877),
      .generator_revision = SUDOKURA_GENERATOR_REVISION,
      .difficulty = DIFFICULTY_EASY,
      .mode = MODE_CLASSIC, .status = SESSION_WON,
      .elapsed_ms = UINT64_C(54000),
  };
  assert(game_library_make_legacy(&record, old_id, &result));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(game_library_load(root, old_id, &readback) == STORE_OK);
  assert(readback.created_utc == 0 &&
         readback.updated_utc == 0 && readback.finished_utc == 0);
  cleanup();
}

static uint32_t fixture_crc32(const unsigned char *data, size_t size) {
  uint32_t crc = UINT32_C(0xffffffff);
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^
            (UINT32_C(0xedb88320) & (0u - (crc & 1u)));
  }
  return ~crc;
}

static void fixture_put_u32(unsigned char *out, uint32_t number) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    out[shift / 8] = (unsigned char)(number >> shift);
}

typedef struct {
  GameAttemptId id;
  unsigned incompatible, healthy;
} UnsupportedScan;

static bool scan_unsupported(const GameLibraryEntry *entry, void *ctx) {
  UnsupportedScan *scan = (UnsupportedScan *)ctx;
  if (game_attempt_id_equal(entry->id, scan->id)) {
    assert(entry->status == STORE_INCOMPATIBLE);
    assert(entry->previous_status == STORE_OK);
    ++scan->incompatible;
  } else {
    assert(entry->status == STORE_OK);
    ++scan->healthy;
  }
  return true;
}

static void fixture_put_u64(unsigned char *out, uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8)
    out[shift / 8] = (unsigned char)(value >> shift);
}

static void test_invalid_timestamp_is_not_healthy_catalogue_metadata(void) {
  prepare();
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameAttemptId id = make_id(583);
  GameLibraryRecord record, unchanged;
  GameLibraryHotCache cache = {0};
  const uint64_t epoch = UINT64_C(1700001000);
  assert(game_library_make_exact_runtime(&record, id, 1, &active, true));
  assert(game_library_save_runtime_at(root, &record, &cache, epoch) == STORE_OK);

  char hex[33], path[160];
  game_attempt_id_hex(id, hex);
  (void)snprintf(path, sizeof(path), "%s/games/%s.dat", root, hex);
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t size = 0;
  assert(store_read_file(path, bytes, sizeof(bytes), &size) == STORE_OK);

  /* Valid envelope checksum with inconsistent recorded UTC dates. */
  enum { HEADER = 18, UPDATED_UTC = 18 + 4 + 16 + 16 + 8 + 8 };
  assert(size > UPDATED_UTC + 8u);
  fixture_put_u64(bytes + UPDATED_UTC, epoch - 1u);
  fixture_put_u32(bytes + 14u,
                  fixture_crc32(bytes + HEADER, size - HEADER));
  assert(store_atomic_write(path, NULL, bytes, size) == STORE_OK);
  GameLibraryEntry metadata;
  assert(game_library_peek(bytes, size, &metadata) == STORE_CORRUPT);
  unchanged = record;
  assert(game_library_load(root, id, &unchanged) == STORE_CORRUPT);
  assert(memcmp(&unchanged, &record, sizeof(record)) == 0);
  MixedScanProbe probe = {0};
  assert(game_library_scan(root, mixed_scan_callback, &probe) == STORE_OK);
  assert(probe.damaged == 1 && probe.healthy == 0);
  cleanup();
}

static void test_unsupported_generator_preserves_previous(void) {
  prepare();
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameAttemptId unsupported = make_id(580);
  GameAttemptId healthy = make_id(581);
  GameLibraryRecord old, updated, output;
  assert(game_library_make_exact(&old, unsupported, 1, &active, true));
  assert(game_library_save(root, &old) == STORE_OK);
  active.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&updated, unsupported, 2, &active, true));
  assert(game_library_save(root, &updated) == STORE_OK);
  assert(game_library_make_exact(&updated, healthy, 1, &active, true));
  assert(game_library_save(root, &updated) == STORE_OK);

  char hex[33], primary[160], preserved[180];
  game_attempt_id_hex(unsupported, hex);
  (void)snprintf(primary, sizeof(primary),
                 "%s/games/%s.dat", root, hex);
  (void)snprintf(preserved, sizeof(preserved),
                 "%s/games/%s.dat.pre-recovery", root, hex);
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t size = 0;
  assert(store_read_file(primary, bytes, sizeof(bytes), &size) == STORE_OK);
  /* Schema v1: header 18, record identity 4+16+16+8+24+4,
     result seed 8, then its generator revision u32. */
  enum { HEADER_BYTES = 18, GENERATOR_OFFSET = 18 + 4 + 16 + 16 + 8 +
                          24 + 4 + 8 };
  assert(size > GENERATOR_OFFSET + 4u);
  fixture_put_u32(bytes + GENERATOR_OFFSET, UINT32_C(99999));
  fixture_put_u32(bytes + 14u,
                  fixture_crc32(bytes + HEADER_BYTES, size - HEADER_BYTES));
  assert(store_atomic_write(primary, NULL, bytes, size) == STORE_OK);

  GameLibraryRecord unchanged = old;
  assert(game_library_load(root, unsupported, &unchanged) ==
         STORE_INCOMPATIBLE);
  assert(memcmp(&unchanged, &old, sizeof(old)) == 0);
  UnsupportedScan scan = {unsupported, 0, 0};
  assert(game_library_scan(root, scan_unsupported, &scan) == STORE_OK);
  assert(scan.incompatible == 1 && scan.healthy == 1);
  assert(game_library_load(root, healthy, &output) == STORE_OK);

  assert(game_library_recover_previous(root, unsupported, &output) == STORE_OK);
  assert(output.revision == 1);
  unsigned char before[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t preserved_size = 0;
  assert(store_read_file(preserved, before, sizeof(before),
                         &preserved_size) == STORE_OK);
  assert(preserved_size == size && memcmp(before, bytes, size) == 0);
  cleanup();
}

typedef struct {
  GameAttemptId target;
  GameAttemptId neighbor;
  unsigned incompatible;
  unsigned healthy;
} ReceiptVersionScan;

static bool inspect_future_receipt(const GameLibraryEntry *entry,
                                   void *context) {
  ReceiptVersionScan *scan = (ReceiptVersionScan *)context;
  if (game_attempt_id_equal(entry->id, scan->target)) {
    assert(entry->status == STORE_INCOMPATIBLE);
    ++scan->incompatible;
  } else {
    assert(game_attempt_id_equal(entry->id, scan->neighbor));
    assert(entry->status == STORE_OK);
    ++scan->healthy;
  }
  return true;
}

static void test_future_receipt_versions_fail_closed(void) {
  prepare();
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameAttemptId target = make_id(740);
  GameAttemptId neighbor = make_id(741);
  GameLibraryRecord record;
  assert(game_library_make_exact(&record, target, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(game_library_make_exact(&record, neighbor, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);

  char hex[33], path[160];
  game_attempt_id_hex(target, hex);
  int written = snprintf(path, sizeof(path), "%s/games/%s.deleted",
                         root, hex);
  assert(written > 0 && (size_t)written < sizeof(path));
  unsigned char future[40] = {0};
  memcpy(future, "SUDODEL1", 8);
  future[8] = 2; /* Future, unsupported receipt version. */
  assert(store_atomic_write(path, NULL, future, sizeof(future)) == STORE_OK);
  assert(game_library_receipt_status(root, target) == STORE_INCOMPATIBLE);
  GameLibraryRecord unchanged = record;
  assert(game_library_load(root, target, &unchanged) == STORE_INCOMPATIBLE);
  assert(memcmp(&unchanged, &record, sizeof(record)) == 0);
  assert(game_library_delete(root, target) == STORE_INCOMPATIBLE);
  ReceiptVersionScan scan = {target, neighbor, 0, 0};
  assert(game_library_scan(root, inspect_future_receipt, &scan) == STORE_OK);
  assert(scan.incompatible == 1 && scan.healthy == 1);

  /* Even a recognizable truncated future header is incompatible. */
  assert(store_atomic_write(path, NULL, future, 10) == STORE_OK);
  assert(game_library_receipt_status(root, target) == STORE_INCOMPATIBLE);

  /* Known v1 with surplus bytes is corrupt, never accepted as deletion. */
  future[8] = 1;
  assert(store_atomic_write(path, NULL, future, sizeof(future)) == STORE_OK);
  assert(game_library_receipt_status(root, target) == STORE_CORRUPT);
  assert(game_library_delete(root, target) == STORE_CORRUPT);
  assert(store_remove_file(path));
  assert(game_library_load(root, target, &unchanged) == STORE_OK);
  assert(unchanged.revision == 1);
  assert(game_library_load(root, neighbor, &unchanged) == STORE_OK);
  cleanup();
}

typedef struct {
  GameAttemptId id;
  unsigned count;
} InvalidBackupScan;

static bool inspect_invalid_backup(const GameLibraryEntry *entry,
                                   void *context) {
  InvalidBackupScan *scan = (InvalidBackupScan *)context;
  assert(game_attempt_id_equal(entry->id, scan->id));
  assert(entry->status == STORE_OK);
  assert(entry->previous_status == STORE_CORRUPT);
  ++scan->count;
  return true;
}

static void test_corrupt_previous_copy_does_not_replace_good_primary(void) {
  prepare();
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameAttemptId id = make_id(742);
  GameLibraryRecord record, prior;
  assert(game_library_make_exact(&record, id, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  active.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, id, 2, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  assert(game_library_load(root, id, &prior) == STORE_OK);
  assert(prior.revision == 2);
  char hex[33], backup[160];
  game_attempt_id_hex(id, hex);
  int written = snprintf(backup, sizeof(backup), "%s/games/%s.dat.bak",
                         root, hex);
  assert(written > 0 && (size_t)written < sizeof(backup));
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t length = 0;
  assert(store_read_file(backup, bytes, sizeof(bytes), &length) == STORE_OK);
  bytes[length - 1] ^= 0x35;
  assert(store_atomic_write(backup, NULL, bytes, length) == STORE_OK);
  GameLibraryRecord candidate = prior;
  assert(game_library_recover_previous(root, id, &candidate) == STORE_CORRUPT);
  assert(memcmp(&candidate, &prior, sizeof(candidate)) == 0);
  assert(game_library_load(root, id, &candidate) == STORE_OK);
  assert(candidate.revision == 2);
  InvalidBackupScan scan = {id, 0};
  assert(game_library_scan(root, inspect_invalid_backup, &scan) == STORE_OK);
  assert(scan.count == 1);
  cleanup();
}

static void test_unacknowledged_record_commit_is_idempotent(void) {
  prepare();
  GameAttemptId id = make_id(743);
  ProfileSession won = make_session(SESSION_WON);
  GameLibraryRecord record;
  assert(game_library_make_exact(&record, id, 1, &won, true));

  store_test_set_fault(STORE_TEST_FAULT_PARENT_SYNC);
  assert(game_library_save(root, &record) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  GameLibraryRecord loaded;
  assert(game_library_load(root, id, &loaded) == STORE_OK);
  assert(loaded.revision == record.revision &&
         loaded.result.status == SESSION_WON);

  /* Retrying a committed record must not create a second win or rotate a
     valid record into backup merely because the first acknowledgement failed. */
  assert(game_library_save(root, &record) == STORE_OK);
  char hex[33], previous[SUDOKURA_STORE_PATH_CAPACITY];
  game_attempt_id_hex(id, hex);
  int length = snprintf(previous, sizeof(previous), "%s/games/%s.dat.bak",
                        root, hex);
  assert(length > 0 && (size_t)length < sizeof(previous));
  assert(!store_file_exists(previous));
  ScanProbe scan = {0, false, false};
  assert(game_library_scan(root, scan_callback, &scan) == STORE_OK);
  assert(scan.count == 1u && !scan.saw_backup);
  cleanup();
}

static void test_unacknowledged_revision_retains_previous_copy(void) {
  prepare();
  GameAttemptId id = make_id(744);
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameLibraryRecord record, previous;
  assert(game_library_make_exact(&record, id, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  char hex[33], primary[SUDOKURA_STORE_PATH_CAPACITY];
  char backup[SUDOKURA_STORE_PATH_CAPACITY];
  game_attempt_id_hex(id, hex);
  int n = snprintf(primary, sizeof(primary), "%s/games/%s.dat", root, hex);
  assert(n > 0 && (size_t)n < sizeof(primary));
  n = snprintf(backup, sizeof(backup), "%s/games/%s.dat.bak", root, hex);
  assert(n > 0 && (size_t)n < sizeof(backup));

  active.session.elapsed_ms += 1000;
  assert(game_library_make_exact(&record, id, 2, &active, true));
  store_test_fail_parent_sync_at(primary);
  assert(game_library_save(root, &record) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(game_library_load(root, id, &previous) == STORE_OK);
  assert(previous.revision == 2);
  assert(store_file_exists(backup));

  unsigned char before[SUDOKURA_GAME_RECORD_MAX_BYTES];
  unsigned char after[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t before_size = 0, after_size = 0;
  assert(store_read_file(backup, before, sizeof(before),
                         &before_size) == STORE_OK);
  assert(game_library_save(root, &record) == STORE_OK);
  assert(store_read_file(backup, after, sizeof(after),
                         &after_size) == STORE_OK);
  assert(before_size == after_size &&
         memcmp(before, after, before_size) == 0);
  assert(game_library_recover_previous(root, id, &previous) == STORE_OK);
  assert(previous.revision == 1);
  cleanup();
}

static void test_unacknowledged_delete_receipt_blocks_resurrection(void) {
  prepare();
  GameAttemptId id = make_id(745);
  ProfileSession active = make_session(SESSION_ACTIVE);
  GameLibraryRecord record;
  assert(game_library_make_exact(&record, id, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  char hex[33], receipt[SUDOKURA_STORE_PATH_CAPACITY];
  game_attempt_id_hex(id, hex);
  int n = snprintf(receipt, sizeof(receipt), "%s/games/%s.deleted", root, hex);
  assert(n > 0 && (size_t)n < sizeof(receipt));

  store_test_fail_parent_sync_at(receipt);
  assert(game_library_delete(root, id) == STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(game_library_receipt_status(root, id) == STORE_OK);
  assert(game_library_load(root, id, &record) == STORE_NOT_FOUND);
  assert(game_library_save(root, &record) == STORE_IO_ERROR);
  assert(game_library_delete(root, id) == STORE_OK);
  assert(game_library_receipt_status(root, id) == STORE_OK);
  assert(game_library_load(root, id, &record) == STORE_NOT_FOUND);
  cleanup();
}

static void test_replay_and_selective_reference_pruning(void) {
  ProfileSession completed = make_session(SESSION_WON);
  GameLibraryRecord source, replay;
  GameAttemptId original = make_id(810);
  GameAttemptId fresh = make_id(811);
  assert(game_library_make_exact(&source, original, 3, &completed, true));
  assert(game_library_prepare_replay(&replay, &source, fresh));
  assert(game_library_record_validate(&replay));
  assert(replay.practice && replay.retained && replay.result.practice);
  assert(game_attempt_id_equal(replay.parent, original));
  assert(game_attempt_id_equal(replay.id, fresh) && replay.revision == 1);
  assert(replay.exact.session.status == SESSION_ACTIVE);
  assert(replay.exact.session.elapsed_ms == 0);
  assert(replay.exact.undo_count == 0 && replay.exact.redo_count == 0);
  for (int i = 0; i < 81; ++i) {
    assert(replay.exact.session.game.initial[i] ==
           source.exact.session.game.initial[i]);
    assert(replay.exact.session.game.puzzle[i] ==
           source.exact.session.game.initial[i]);
    assert(replay.exact.session.game.notes[i] == 0);
    assert(replay.exact.session.game.hinted[i] == 0);
  }
  assert(source.result.status == SESSION_WON);
  assert(!game_library_prepare_replay(&replay, &source, original));
  assert(!game_library_prepare_replay(&replay, &replay, make_id(812)));

  ProfileData profile, pruned;
  profile_defaults(&profile);
  ProfileSession current = make_session(SESSION_ACTIVE);
  assert(profile_slot_set(&profile.normal, &current.session, false));
  profile.normal.attempt_id = original;
  profile.normal.revision = 1;
  profile.normal.retained = true;
  ProfileResult old_result = source.result;
  old_result.attempt_id = original;
  ProfileResult other_result = source.result;
  other_result.attempt_id = fresh;
  bool inserted = false;
  assert(profile_record_result(&profile, &old_result, &inserted) && inserted);
  assert(profile_record_result(&profile, &other_result, &inserted) && inserted);
  assert(profile_validate(&profile));
  assert(game_library_forget_attempt(&pruned, &profile, original));
  assert(!pruned.normal.present && pruned.result_count == 1);
  assert(game_attempt_id_equal(pruned.results[0].attempt_id, fresh));
  assert(profile_validate(&pruned));
  assert(profile.normal.present && profile.result_count == 2);
  assert(!game_library_forget_attempt(&pruned, &profile,
                                      (GameAttemptId){0,0}));
}

static ProfileSession daily_fixture_revision(int year, int month, int day,
                                             SessionStatus status,
                                             uint8_t generator_revision) {
  ProfileSession out = make_session(SESSION_ACTIVE);
  uint64_t seed=0;
  assert(game_daily_seed_revision(year,month,day,
                                  generator_revision,&seed));
  assert(game_new_difficulty_revision(&out.session.game,seed,
                                      DIFFICULTY_MEDIUM,
                                      generator_revision));
  out.session.is_daily=true;
  out.session.daily_year=year;
  out.session.daily_month=month;
  out.session.daily_day=day;
  out.session.status=status;
  if(status==SESSION_WON)
    for(size_t i=0;i<81;i++) {
      out.session.game.puzzle[i]=out.session.game.solution[i];
      out.session.game.notes[i]=0;
    }
  assert(session_validate(&out.session));
  return out;
}
static ProfileSession daily_fixture(int year, int month, int day,
                                    SessionStatus status) {
  return daily_fixture_revision(year, month, day, status,
                                SUDOKURA_GENERATOR_REVISION_LEGACY);
}
static bool daily_lookup_visit(const GameLibraryEntry *entry,void *ctx) {
  game_library_daily_lookup_consider((GameDailyLookup *)ctx,entry,2026,10,13);
  return true;
}
static void test_daily_lookup_metadata_and_errors(void) {
  prepare();
  ProfileSession active=daily_fixture(2026,10,13,SESSION_ACTIVE);
  GameLibraryRecord original,other,practice;
  GameAttemptId id=make_id(3901),replay_id=make_id(3902);
  assert(game_library_make_exact(&original,id,1,&active,true));
  assert(game_library_save(root,&original)==STORE_OK);
  GameDailyLookup lookup={0};
  assert(game_library_scan(root,daily_lookup_visit,&lookup)==STORE_OK);
  assert(lookup.matching_attempts==1 && !lookup.uncertain);
  assert(game_attempt_id_equal(lookup.match.id,id));
  assert(game_library_is_original_daily_on(&lookup.match,2026,10,13));
  assert(!game_library_is_original_daily_on(&lookup.match,2026,10,14));

  ProfileSession terminal=daily_fixture(2026,10,13,SESSION_WON);
  assert(game_library_make_exact(&original,id,2,&terminal,true));
  assert(game_library_save(root,&original)==STORE_OK);
  memset(&lookup,0,sizeof(lookup));
  assert(game_library_scan(root,daily_lookup_visit,&lookup)==STORE_OK);
  assert(lookup.matching_attempts==1);
  assert(lookup.match.result.status==SESSION_WON);

  assert(game_library_prepare_replay(&practice,&original,replay_id));
  assert(practice.practice && !practice.result.is_daily);
  assert(game_library_save(root,&practice)==STORE_OK);
  memset(&lookup,0,sizeof(lookup));
  assert(game_library_scan(root,daily_lookup_visit,&lookup)==STORE_OK);
  assert(lookup.matching_attempts==1 && !lookup.uncertain);

  assert(game_library_make_exact(&other,make_id(3903),1,&active,true));
  assert(game_library_save(root,&other)==STORE_OK);
  memset(&lookup,0,sizeof(lookup));
  assert(game_library_scan(root,daily_lookup_visit,&lookup)==STORE_OK);
  assert(lookup.matching_attempts==2);

  assert(game_library_delete(root,other.id)==STORE_OK);
  assert(game_library_delete(root,replay_id)==STORE_OK);
  assert(game_library_delete(root,id)==STORE_OK);
  memset(&lookup,0,sizeof(lookup));
  assert(game_library_scan(root,daily_lookup_visit,&lookup)==STORE_OK);
  assert(lookup.matching_attempts==0 && !lookup.uncertain);
  cleanup();
}
static void test_daily_replay_revisions_survive_disk_roundtrip(void) {
  const uint8_t generators[] = {
      SUDOKURA_GENERATOR_REVISION_LEGACY, SUDOKURA_GENERATOR_REVISION};
  for (size_t variant = 0;
       variant < sizeof(generators) / sizeof(generators[0]); ++variant) {
    prepare();
    GameAttemptId original_id = make_id(4100u + (uint64_t)variant * 2u);
    GameAttemptId replay_id = make_id(4101u + (uint64_t)variant * 2u);
    ProfileSession won = daily_fixture_revision(
        2026, 10, 13, SESSION_WON, generators[variant]);
    GameLibraryRecord source, replay, disk, updated;
    assert(game_library_make_exact(&source, original_id, 1, &won, true));
    assert(game_library_save(root, &source) == STORE_OK);
    assert(game_library_load(root, original_id, &source) == STORE_OK);
    assert(source.original_daily && source.result.is_daily);
    assert(game_library_prepare_replay(&replay, &source, replay_id));
    GameLibraryHotCache cache = {0};
    assert(game_library_save_runtime_at(
        root, &replay, &cache, UINT64_C(1791916200)) == STORE_OK);
    assert(game_library_load(root, replay_id, &disk) == STORE_OK);
    assert(disk.revision == 1 && disk.practice && disk.result.practice);
    assert(disk.original_daily && !disk.exact.session.is_daily);
    assert(disk.original_daily_year == 2026 &&
           disk.original_daily_month == 10 && disk.original_daily_day == 13);
    assert(game_attempt_id_equal(disk.parent, original_id));
    assert(game_library_replay_pending_matches(&source, &disk));

    /* A later normal-slot checkpoint must keep the source Daily identity. */
    ProfileSession progressed = disk.exact;
    progressed.session.elapsed_ms = UINT64_C(4321);
    progressed.session.selected_row = 2;
    progressed.session.selected_column = 3;
    progressed.session.notes_mode = true;
    assert(game_library_make_exact_runtime(
        &updated, replay_id, 2, &progressed, true));
    updated.practice = disk.practice;
    updated.parent = disk.parent;
    updated.result.practice = disk.result.practice;
    updated.original_daily = disk.original_daily;
    updated.original_daily_year = disk.original_daily_year;
    updated.original_daily_month = disk.original_daily_month;
    updated.original_daily_day = disk.original_daily_day;
    assert(game_library_record_validate_runtime(&updated));
    assert(game_library_save_runtime_at(
        root, &updated, &cache, UINT64_C(1791916240)) == STORE_OK);
    assert(game_library_load(root, replay_id, &disk) == STORE_OK);
    assert(disk.revision == 2 && disk.practice && disk.result.practice);
    assert(disk.exact.session.elapsed_ms == UINT64_C(4321) &&
           disk.exact.session.selected_row == 2 &&
           disk.exact.session.selected_column == 3);
    assert(!disk.exact.session.is_daily &&
           disk.original_daily && disk.original_daily_year == 2026);
    assert(game_attempt_id_equal(disk.parent, original_id));
    assert(!game_library_replay_pending_matches(&source, &disk));

    /* A won Replay is separate from the immutable original Daily. */
    ProfileSession completed = disk.exact;
    for (size_t cell = 0; cell < 81; ++cell) {
      completed.session.game.puzzle[cell] =
          completed.session.game.solution[cell];
      completed.session.game.notes[cell] = 0;
    }
    completed.session.status = SESSION_WON;
    completed.session.elapsed_ms = UINT64_C(12345);
    assert(session_validate_runtime(&completed.session));
    assert(game_library_make_exact_runtime(
        &updated, replay_id, 3, &completed, true));
    updated.practice = disk.practice;
    updated.parent = disk.parent;
    updated.result.practice = disk.result.practice;
    updated.original_daily = disk.original_daily;
    updated.original_daily_year = disk.original_daily_year;
    updated.original_daily_month = disk.original_daily_month;
    updated.original_daily_day = disk.original_daily_day;
    assert(game_library_record_validate_runtime(&updated));
    assert(game_library_save_runtime_at(
        root, &updated, &cache, UINT64_C(1791916290)) == STORE_OK);
    assert(game_library_load(root, replay_id, &disk) == STORE_OK);
    assert(disk.revision == 3 && disk.result.status == SESSION_WON);
    assert(disk.practice && disk.result.practice && !disk.result.is_daily);
    assert(disk.original_daily && disk.original_daily_year == 2026 &&
           disk.original_daily_month == 10 && disk.original_daily_day == 13);
    assert(disk.exact.session.status == SESSION_WON &&
           !disk.exact.session.is_daily);
    assert(disk.finished_utc == UINT64_C(1791916290));
    assert(game_attempt_id_equal(disk.parent, original_id));
    assert(!game_library_replay_pending_matches(&source, &disk));
    assert(game_library_load(root, original_id, &source) == STORE_OK);
    assert(source.result.status == SESSION_WON && source.original_daily);
    GameDailyLookup lookup = {0};
    assert(game_library_scan(root, daily_lookup_visit, &lookup) == STORE_OK);
    assert(lookup.matching_attempts == 1 && !lookup.uncertain);
    assert(game_attempt_id_equal(lookup.match.id, original_id));
    cleanup();
  }
}

static void test_daily_lookup_previous_copy_and_uncertainty(void) {
  prepare();
  const GameAttemptId id = make_id(3905);
  ProfileSession active = daily_fixture(2026, 10, 13, SESSION_ACTIVE);
  GameLibraryRecord record, previous;
  assert(game_library_make_exact(&record, id, 1, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);
  active.session.elapsed_ms += UINT64_C(1000);
  assert(game_library_make_exact(&record, id, 2, &active, true));
  assert(game_library_save(root, &record) == STORE_OK);

  char hex[33], path[160];
  game_attempt_id_hex(id, hex);
  int written = snprintf(path, sizeof(path), "%s/games/%s.dat", root, hex);
  assert(written > 0 && (size_t)written < sizeof(path));
  const unsigned char invalid[10] = {0};
  assert(store_atomic_write(path, NULL, invalid, sizeof(invalid)) == STORE_OK);

  GameDailyLookup lookup = {0};
  assert(game_library_scan(root, daily_lookup_visit, &lookup) == STORE_OK);
  assert(lookup.matching_attempts == 1 && !lookup.uncertain);
  assert(lookup.match.status == STORE_CORRUPT);
  assert(lookup.match.previous_status == STORE_OK);
  assert(game_library_recover_previous(root, id, &previous) == STORE_OK);
  assert(previous.revision == 1 && previous.original_daily);
  assert(game_library_load(root, id, &record) == STORE_OK);
  assert(record.revision == 1);

  assert(store_remove_file(path));
  memset(&lookup, 0, sizeof(lookup));
  assert(game_library_scan(root, daily_lookup_visit, &lookup) == STORE_OK);
  assert(lookup.matching_attempts == 1 && !lookup.uncertain);
  assert(lookup.match.status == STORE_NOT_FOUND);
  assert(lookup.match.previous_status == STORE_OK);
  assert(game_library_recover_previous(root, id, &previous) == STORE_OK);

  /* Damaged unknown entries must not be silently classified as other dates. */
  GameLibraryEntry unknown;
  memset(&unknown, 0, sizeof(unknown));
  unknown.status = STORE_CORRUPT;
  unknown.previous_status = STORE_NOT_FOUND;
  memset(&lookup, 0, sizeof(lookup));
  game_library_daily_lookup_consider(&lookup, &unknown, 2026, 10, 13);
  assert(lookup.uncertain && lookup.matching_attempts == 0);
  cleanup();
}

static void test_daily_rollover_retains_distinct_attempts(void) {
  prepare();
  ProfileSession previous = daily_fixture(2026, 10, 12, SESSION_ACTIVE);
  GameAttemptId previous_id = make_id(3907);
  GameAttemptId today_id = make_id(3908);
  GameLibraryRecord record, loaded;
  GameLibraryHotCache previous_cache = {0};
  GameLibraryHotCache today_cache = {0};
  /* An empty earlier Daily still needs a durable record. A failed
     first write must not be reported as a retained attempt. */
  assert(game_library_make_exact_runtime(&record, previous_id, 3,
                                         &previous, true));
  store_test_set_fault(STORE_TEST_FAULT_SYNC);
  assert(game_library_save_runtime(root, &record, &previous_cache) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(game_library_load(root, previous_id, &loaded) == STORE_NOT_FOUND);
  assert(game_library_save_runtime(root, &record, &previous_cache) == STORE_OK);
  assert(game_library_load(root, previous_id, &loaded) == STORE_OK);
  assert(loaded.revision == 3);
  assert(loaded.original_daily && loaded.result.daily_day == 12);

  ProfileSession today = daily_fixture(2026, 10, 13, SESSION_ACTIVE);
  assert(game_library_make_exact_runtime(&record, today_id, 1, &today, true));
  assert(game_library_save_runtime(root, &record, &today_cache) == STORE_OK);
  GameDailyLookup lookup = {0};
  assert(game_library_scan(root, daily_lookup_visit, &lookup) == STORE_OK);
  assert(lookup.matching_attempts == 1 && !lookup.uncertain);
  assert(game_attempt_id_equal(lookup.match.id, today_id));
  assert(game_library_load(root, previous_id, &loaded) == STORE_OK);
  assert(loaded.result.daily_day == 12 && loaded.revision == 3);
  assert(game_library_load(root, today_id, &loaded) == STORE_OK);
  assert(loaded.result.daily_day == 13 && loaded.revision == 1);
  cleanup();
}

static void test_explicit_game_recovery_gate(void) {
  GameLibraryEntry item;
  memset(&item, 0, sizeof(item));
  assert(!game_library_entry_recoverable(NULL));
  item.id = make_id(3929);
  item.status = STORE_CORRUPT;
  item.previous_status = STORE_OK;
  assert(game_library_entry_recoverable(&item));
  item.status = STORE_NOT_FOUND;
  assert(!game_library_entry_recoverable(&item));
  item.only_backup = true;
  assert(game_library_entry_recoverable(&item));
  item.status = STORE_INCOMPATIBLE;
  assert(!game_library_entry_recoverable(&item));
  item.status = STORE_IO_ERROR;
  assert(!game_library_entry_recoverable(&item));
  item.status = STORE_OK;
  assert(!game_library_entry_recoverable(&item));
  item.status = STORE_CORRUPT;
  item.previous_status = STORE_INCOMPATIBLE;
  assert(!game_library_entry_recoverable(&item));
  item.previous_status = STORE_OK;
  item.id = (GameAttemptId){0, 0};
  assert(!game_library_entry_recoverable(&item));
}

static void test_same_attempt_load_prefers_durable_revision(void) {
  GameAttemptId id = make_id(3951);
  ProfileSession saved = make_session(SESSION_ACTIVE);
  ProfileSlot hot = {0};
  hot.present = true;
  hot.retained = true;
  hot.attempt_id = id;
  hot.revision = 1;
  hot.value = saved;

  GameLibraryRecord disk;
  bool prefer = true;
  assert(game_library_make_exact(&disk, id, 1, &saved, true));
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) == STORE_OK);
  assert(!prefer);

  ProfileSession newer = saved;
  newer.session.elapsed_ms += UINT64_C(5000);
  newer.session.selected_column = 6;
  assert(game_library_make_exact(&disk, id, 2, &newer, true));
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) == STORE_OK);
  assert(prefer);
  /* The decision is pure: the old slot cannot be displaced until the
     target's new hot pointer has been durably committed. */
  assert(hot.revision == 1 &&
         hot.value.session.elapsed_ms == saved.session.elapsed_ms);

  hot.revision = 3;
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) ==
         STORE_IO_ERROR);
  assert(!prefer);
  hot.retained = false; /* Disposable hot-only progress may be newer. */
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) == STORE_OK);
  assert(!prefer);

  hot.retained = true;
  hot.revision = 2;
  /* Same revision with different notes/time/history is a collision,
     never a tie decided by a filename or wall clock. */
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) ==
         STORE_CORRUPT);
  hot.value = newer;
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) == STORE_OK);
  assert(!prefer);
  hot.retained = false;
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) == STORE_OK);
  assert(prefer); /* Complete an interrupted retention linkage. */

  hot.retained = true;
  hot.value.session.game.seed ^= UINT64_C(1);
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) ==
         STORE_CORRUPT);
  hot.value = newer;
  hot.value.session.is_daily = true;
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) ==
         STORE_CORRUPT);

  hot.value = make_session(SESSION_WON);
  hot.revision = 3;
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) ==
         STORE_CORRUPT); /* Never reactivate a finished attempt. */
  hot.value = newer;
  assert(game_library_resolve_hot_record(NULL, &disk, &prefer) ==
         STORE_IO_ERROR);
  assert(game_library_resolve_hot_record(&hot, &disk, NULL) ==
         STORE_IO_ERROR);
  disk.id = make_id(3952);
  assert(game_library_resolve_hot_record(&hot, &disk, &prefer) ==
         STORE_CORRUPT);
}

static void test_replay_linked_selection_respects_source(void) {
  const GameAttemptId source_id = make_id(9811);
  const GameAttemptId attempt_id = make_id(9812);
  const GameAttemptId other_source = make_id(9813);
  ProfileSession finished = make_session(SESSION_WON);
  GameLibraryRecord source, replay;
  assert(game_library_make_exact(&source, source_id, 1, &finished, true));
  assert(game_library_prepare_replay(&replay, &source, attempt_id));
  assert(game_library_record_validate(&replay));
  GameReplayIntent intent = {
      source_id, attempt_id, GAME_REPLAY_PREPARED};
  ProfileSlot hot = {0};
  hot.present = true;
  hot.retained = true;
  hot.attempt_id = attempt_id;
  hot.revision = replay.revision;
  hot.value = replay.exact;

  bool resume = false;
  assert(game_library_replay_linked_selection(
      &intent, source_id, &hot, &replay, &resume) == STORE_OK);
  assert(resume); /* Retry Replay of A resumes A, no duplicate ID. */

  resume = true;
  assert(game_library_replay_linked_selection(
      &intent, other_source, &hot, &replay, &resume) == STORE_OK);
  assert(!resume); /* Replay of B must not unexpectedly reopen A. */
  assert(intent.state == GAME_REPLAY_PREPARED &&
         hot.revision == replay.revision &&
         game_attempt_id_equal(hot.attempt_id, attempt_id));

  /* A source/parent collision, wrong live slot, or a newer disk snapshot
     must not finalize the marker and publish an older or unrelated board. */
  GameLibraryRecord unrelated = replay;
  unrelated.parent = other_source;
  resume = true;
  assert(game_library_replay_linked_selection(
      &intent, source_id, &hot, &unrelated, &resume) == STORE_CORRUPT);
  assert(resume); /* Failed resolution leaves the result untouched. */
  ProfileSlot wrong = hot;
  wrong.attempt_id = other_source;
  assert(game_library_replay_linked_selection(
      &intent, source_id, &wrong, &replay, &resume) == STORE_CORRUPT);

  GameLibraryRecord newer_disk = replay;
  ++newer_disk.revision;
  assert(game_library_record_validate(&newer_disk));
  assert(game_library_replay_linked_selection(
      &intent, source_id, &hot, &newer_disk, &resume) == STORE_IO_ERROR);

  GameReplayIntent completed = intent;
  completed.state = GAME_REPLAY_COMMITTED;
  assert(game_library_replay_linked_selection(
      &completed, source_id, &hot, &replay, &resume) == STORE_CORRUPT);
  assert(game_library_replay_linked_selection(
      &intent, (GameAttemptId){0, 0}, &hot, &replay, &resume) ==
         STORE_CORRUPT);
  assert(game_library_replay_linked_selection(
      NULL, source_id, &hot, &replay, &resume) == STORE_IO_ERROR);
  assert(game_library_replay_linked_selection(
      &intent, source_id, &hot, &replay, NULL) == STORE_IO_ERROR);
}

static void test_replay_intent_survives_failed_cuts(void) {
  prepare();
  GameAttemptId source_id = make_id(9801);
  GameAttemptId first = make_id(9802);
  GameAttemptId second = make_id(9803);
  GameReplayIntent intent = {source_id, first, GAME_REPLAY_PREPARED};
  GameReplayIntent loaded = {make_id(9), make_id(10),
                             GAME_REPLAY_ABANDONED};
  GameReplayIntent untouched = loaded;
  assert(game_library_replay_intent_load(root, &loaded) == STORE_NOT_FOUND);
  assert(game_attempt_id_equal(untouched.source_id, loaded.source_id) &&
         game_attempt_id_equal(untouched.attempt_id, loaded.attempt_id) &&
         untouched.state == loaded.state);

  /* A failed intent publication creates no record and no fresh attempt.
     The very next retry can still safely publish this same reservation. */
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_replay_intent_write(root, &intent) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_NOT_FOUND);
  assert(game_library_replay_intent_write(root, &intent) == STORE_OK);
  assert(game_library_replay_intent_write(root, &intent) == STORE_OK);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_OK);
  assert(loaded.state == GAME_REPLAY_PREPARED &&
         game_attempt_id_equal(loaded.attempt_id, first));

  GameReplayIntent competing = {source_id, second, GAME_REPLAY_PREPARED};
  assert(game_library_replay_intent_write(root, &competing) ==
         STORE_IO_ERROR);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_OK &&
         game_attempt_id_equal(loaded.attempt_id, first));

  GameLibraryRecord original, replay, reread;
  ProfileSession won = make_session(SESSION_WON);
  assert(game_library_make_exact(&original, source_id, 1, &won, true));
  assert(game_library_save(root, &original) == STORE_OK);
  assert(game_library_prepare_replay(&replay, &original, first));
  assert(game_library_replay_pending_matches(&original, &replay));

  GameLibraryRecord changed = replay;
  changed.exact.session.elapsed_ms += 1000u;
  changed.result.elapsed_ms += 1000u;
  assert(!game_library_replay_pending_matches(&original, &changed));
  changed = replay;
  changed.parent = make_id(9804);
  assert(!game_library_replay_pending_matches(&original, &changed));

  GameLibraryHotCache cache = {0};
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(game_library_save_runtime(root, &replay, &cache) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(game_library_load(root, first, &reread) == STORE_NOT_FOUND);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_OK &&
         game_attempt_id_equal(loaded.attempt_id, first));

  /* Simulate process restart: no GamesBrowser state, only the intent ID. */
  assert(game_library_save_runtime(root, &replay, &cache) == STORE_OK);
  assert(game_library_load(root, loaded.attempt_id, &reread) == STORE_OK);
  assert(game_library_replay_pending_matches(&original, &reread));

  char primary[160], backup[160], intent_path[160];
  /* Keep the fault-injection profile inside the fixture directory cleared
     by prepare(): normal tests and sanitizers run in separate processes. */
  int n = snprintf(primary, sizeof(primary),
                   "%s/games/replay-profile-cut.dat", root);
  assert(n > 0 && (size_t)n < sizeof(primary));
  n = snprintf(backup, sizeof(backup),
               "%s/games/replay-profile-cut.dat.bak", root);
  assert(n > 0 && (size_t)n < sizeof(backup));
  n = snprintf(intent_path, sizeof(intent_path), "%s/games/replay.intent", root);
  assert(n > 0 && (size_t)n < sizeof(intent_path));

  ProfileData candidate;
  profile_defaults(&candidate);
  candidate.normal.present = true;
  candidate.normal.retained = true;
  candidate.normal.attempt_id = reread.id;
  candidate.normal.revision = reread.revision;
  candidate.normal.value = reread.exact;
  assert(profile_validate(&candidate));
  assert(!store_file_exists(primary));

  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(profile_save_file(primary, backup, &candidate) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  ProfileData disk;
  assert(profile_load_file(primary, &disk) == STORE_NOT_FOUND);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_OK);
  assert(loaded.state == GAME_REPLAY_PREPARED);
  assert(game_library_load(root, first, &reread) == STORE_OK);

  assert(profile_save_file(primary, backup, &candidate) == STORE_OK);
  assert(profile_load_file(primary, &disk) == STORE_OK);
  assert(profile_encoded_equal(&candidate, &disk));
  intent.state = GAME_REPLAY_COMMITTED;
  assert(game_library_replay_intent_write(root, &intent) == STORE_OK);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_OK &&
         loaded.state == GAME_REPLAY_COMMITTED);
  assert(game_library_load(root, first, &reread) == STORE_OK);

  /* An explicit later Replay of the same parent gets a distinct ID.
     Earlier successful and pending records stay discoverable. */
  assert(game_library_replay_intent_write(root, &competing) == STORE_OK);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_OK);
  assert(loaded.state == GAME_REPLAY_PREPARED &&
         game_attempt_id_equal(loaded.attempt_id, second));
  assert(game_library_replay_intent_write(root, &intent) == STORE_IO_ERROR);
  competing.state = GAME_REPLAY_ABANDONED;
  assert(game_library_replay_intent_write(root, &competing) == STORE_OK);
  assert(game_library_load(root, first, &reread) == STORE_OK);
  /* An explicitly deleted pending ID cannot be silently resurrected. */
  assert(game_library_delete(root, second) == STORE_OK);
  GameLibraryRecord forbidden;
  GameLibraryHotCache forbidden_cache = {0};
  assert(game_library_prepare_replay(&forbidden, &original, second));
  assert(game_library_save_runtime(root, &forbidden, &forbidden_cache) ==
         STORE_IO_ERROR);

  /* Reject damaged markers rather than guessing another attempt ID. */
  unsigned char bytes[48], original_bytes[48];
  size_t length = 0;
  assert(store_read_file(intent_path, bytes, sizeof(bytes), &length) ==
         STORE_OK && length == sizeof(bytes));
  memcpy(original_bytes, bytes, length);
  bytes[16] ^= 0x01u;
  assert(store_atomic_write(intent_path, NULL, bytes, length) == STORE_OK);
  untouched = loaded;
  assert(game_library_replay_intent_load(root, &loaded) == STORE_CORRUPT);
  assert(game_attempt_id_equal(loaded.source_id, untouched.source_id) &&
         game_attempt_id_equal(loaded.attempt_id, untouched.attempt_id) &&
         loaded.state == untouched.state);
  assert(game_library_replay_intent_write(root, &intent) == STORE_CORRUPT);

  memcpy(bytes, original_bytes, length);
  bytes[8] = 2u;
  assert(store_atomic_write(intent_path, NULL, bytes, length) == STORE_OK);
  assert(game_library_replay_intent_load(root, &loaded) ==
         STORE_INCOMPATIBLE);
  assert(game_library_replay_intent_write(root, &intent) ==
         STORE_INCOMPATIBLE);
  assert(store_atomic_write(intent_path, NULL, original_bytes, length) ==
         STORE_OK);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_OK &&
         loaded.state == GAME_REPLAY_ABANDONED);

  /* An oversized future version remains incompatible, not corrupt. */
  unsigned char future[80] = {0};
  memcpy(future, original_bytes, length);
  future[8] = 2u;
  assert(store_atomic_write(intent_path, NULL, future, sizeof(future)) ==
         STORE_OK);
  assert(game_library_replay_intent_load(root, &loaded) ==
         STORE_INCOMPATIBLE);
  assert(store_atomic_write(intent_path, NULL, original_bytes, length) ==
         STORE_OK);

  /* An acknowledged rename with unacknowledged directory sync can be
     retried without allocating a third ID or erasing the old record. */
  GameReplayIntent third = {source_id, make_id(9805),
                            GAME_REPLAY_PREPARED};
  store_test_fail_parent_sync_at(intent_path);
  assert(game_library_replay_intent_write(root, &third) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(game_library_replay_intent_load(root, &loaded) == STORE_OK);
  if (loaded.state == GAME_REPLAY_ABANDONED) {
    assert(game_library_replay_intent_write(root, &third) == STORE_OK);
  } else {
    assert(loaded.state == GAME_REPLAY_PREPARED &&
           game_attempt_id_equal(loaded.attempt_id, third.attempt_id));
    assert(game_library_replay_intent_write(root, &third) == STORE_OK);
  }
  assert(game_library_load(root, first, &reread) == STORE_OK);
  cleanup();
}

int main(void) {
  cleanup();
  prepare();
  test_hex_and_collision();
  test_explicit_game_recovery_gate();
  test_same_attempt_load_prefers_durable_revision();
  test_daily_lookup_metadata_and_errors();
  test_daily_lookup_previous_copy_and_uncertainty();
  test_daily_replay_revisions_survive_disk_roundtrip();
  test_daily_rollover_retains_distinct_attempts();
  test_replay_and_selective_reference_pruning();
  test_replay_intent_survives_failed_cuts();
  test_replay_linked_selection_respects_source();
  test_long_paths_fail_closed();
  test_codec_and_atomic_decode();
  cleanup();
  test_save_revision_and_previous_recovery();
  test_saved_board_roundtrip_after_restart();
  test_first_save_record_cuts_and_terminal_retry();
  test_first_save_profile_ack_preserves_previous_copy();
  test_backup_only_scan_and_recovery();
  test_delete_receipt_prevents_resurrection();
  test_delete_receipt_fault_matrix();
  test_unbounded_wins_and_scan_cancellation();
  test_live_checkpoint_avoids_puzzle_regeneration();
  test_terminal_hot_retry_is_immutable();
  test_hot_write_failure_keeps_last_good_revision();
  test_mixed_record_health_and_oversized_future_envelope();
  test_thousand_record_incremental_scan();
  test_recovery_copy_collision_is_lossless();
  test_recover_empty_corrupt_primary_losslessly();
  test_first_archive_after_unretained_hot_revisions();
  test_write_fault_matrix_keeps_last_known_good();
  test_corrupt_receipt_isolates_one_attempt();
  test_runtime_utc_dates_and_unknown_legacy();
  test_unsupported_generator_preserves_previous();
  test_invalid_timestamp_is_not_healthy_catalogue_metadata();
  test_future_receipt_versions_fail_closed();
  test_corrupt_previous_copy_does_not_replace_good_primary();
  test_unacknowledged_record_commit_is_idempotent();
  test_unacknowledged_revision_retains_previous_copy();
  test_unacknowledged_delete_receipt_blocks_resurrection();
  cleanup();
  puts("game record codec, revisions, backup, receipts and >128 archive passed");
  return 0;
}
