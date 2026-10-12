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

int main(void) {
  cleanup();
  prepare();
  test_hex_and_collision();
  test_codec_and_atomic_decode();
  cleanup();
  test_save_revision_and_previous_recovery();
  test_backup_only_scan_and_recovery();
  test_delete_receipt_prevents_resurrection();
  test_unbounded_wins_and_scan_cancellation();
  cleanup();
  puts("game record codec, revisions, backup, receipts and >128 archive passed");
  return 0;
}
