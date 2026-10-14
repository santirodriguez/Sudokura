#define SUDOKURA_STORE_TESTING 1
#include "profile.h"
#include "save_policy.h"
#include "store_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *active_path = ".sudokura-live-save-test.dat";
static const char *backup_path = ".sudokura-live-save-test.dat.bak";

static void cleanup(void) {
  (void)store_remove_file(active_path);
  (void)store_remove_file(backup_path);
}

static int first_playable(const Game *game) {
  for (int i = 0; i < 81; ++i)
    if (!game->fixed[i]) return i;
  return -1;
}

static SessionState make_session(void) {
  SessionState state;
  memset(&state, 0, sizeof(state));
  assert(game_new_difficulty_revision(
      &state.game, UINT64_C(0x445566778899aabb), DIFFICULTY_MEDIUM,
      SUDOKURA_GENERATOR_REVISION_LEGACY));
  state.mode = MODE_CLASSIC;
  state.selected_row = 4;
  state.selected_column = 4;
  state.status = SESSION_ACTIVE;
  state.elapsed_ms = UINT64_C(10000);
  assert(session_validate(&state));
  return state;
}

typedef struct {
  const ProfileData *profile;
  unsigned writes;
} TerminalWriter;

static StoreStatus write_terminal_snapshot(void *context) {
  TerminalWriter *writer = (TerminalWriter *)context;
  ++writer->writes;
  return profile_save_runtime_file(active_path, backup_path, writer->profile);
}

static void test_winning_checkpoint_is_durable_without_debounce(void) {
  cleanup();
  ProfileData profile;
  profile_defaults(&profile);
  SessionState won = make_session();
  won.mode = MODE_TIME;
  won.elapsed_ms = UINT64_C(599999);
  won.status = SESSION_WON;
  for (int i = 0; i < 81; ++i) {
    won.game.puzzle[i] = won.game.solution[i];
    won.game.notes[i] = 0;
  }
  assert(session_validate(&won));
  assert(profile_slot_set_runtime(&profile.normal, &won, false));

  SavePolicy policy;
  save_policy_init(&policy);
  save_policy_mark(&policy, SAVE_DIRTY_NORMAL | SAVE_DIRTY_RESULTS, 100);
  assert(!save_policy_checkpoint_due(&policy, 100));
  TerminalWriter writer = {&profile, 0};
  assert(save_policy_checkpoint_now(&policy, write_terminal_snapshot,
                                    &writer, 100));
  assert(writer.writes == 1);
  assert(!save_policy_dirty(&policy));

  ProfileData recovered;
  assert(profile_load_file(active_path, &recovered) == STORE_OK);
  assert(recovered.normal.present);
  assert(recovered.normal.value.session.status == SESSION_WON);
  assert(recovered.normal.value.session.elapsed_ms == UINT64_C(599999));
  assert(!memcmp(recovered.normal.value.session.game.puzzle,
                 won.game.puzzle, sizeof(won.game.puzzle)));
  cleanup();
}

static void test_verified_replacement_faults_and_backups(void) {
  cleanup();
  ProfileData original, candidate, disk, previous;
  profile_defaults(&original);
  SessionState original_game = make_session();
  assert(profile_slot_set(&original.normal, &original_game, false));
  assert(profile_save_runtime_file(active_path, backup_path, &original) == STORE_OK);
  candidate = original;
  candidate.preferences.music_volume = 42;
  assert(!profile_encoded_equal(&original, &candidate));

  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(profile_save_runtime_verified(active_path, backup_path, &candidate) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_load_file(active_path, &disk) == STORE_OK);
  assert(profile_encoded_equal(&disk, &original));

  /* Both writes may have landed before the directory sync failed. */
  store_test_fail_parent_sync_at(active_path);
  assert(profile_save_runtime_verified(active_path, backup_path, &candidate) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_load_file(active_path, &disk) == STORE_OK);
  assert(profile_encoded_equal(&disk, &candidate));
  assert(profile_load_file(backup_path, &previous) == STORE_OK);
  assert(profile_encoded_equal(&previous, &original));

  /* A retry acknowledges the same candidate without rotating the backup. */
  assert(profile_save_runtime_verified(active_path, backup_path, &candidate) ==
         STORE_OK);
  assert(profile_load_file(backup_path, &previous) == STORE_OK);
  assert(profile_encoded_equal(&previous, &original));
  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(profile_save_runtime_verified(active_path, backup_path, &candidate) ==
         STORE_IO_ERROR);
  store_test_set_fault(STORE_TEST_FAULT_NONE);
  assert(profile_load_file(backup_path, &previous) == STORE_OK);
  assert(profile_encoded_equal(&previous, &original));
  assert(profile_save_runtime_verified(active_path, backup_path, &candidate) ==
         STORE_OK);
  assert(profile_load_file(backup_path, &previous) == STORE_OK);
  assert(profile_encoded_equal(&previous, &original));

  /* Unsupported/corrupt primaries are not silently replaced with a copy. */
  unsigned char altered[16384];
  size_t bytes = 0;
  assert(store_read_file(active_path, altered, sizeof(altered), &bytes) == STORE_OK);
  assert(bytes > 20);
  altered[bytes - 1] ^= 1u;
  assert(store_atomic_write(active_path, NULL, altered, bytes) == STORE_OK);
  assert(profile_save_runtime_verified(active_path, backup_path, &candidate) ==
         STORE_CORRUPT);
  unsigned char unchanged[16384];
  size_t unchanged_bytes = 0;
  assert(store_read_file(active_path, unchanged, sizeof(unchanged),
                         &unchanged_bytes) == STORE_OK);
  assert(unchanged_bytes == bytes && memcmp(unchanged, altered, bytes) == 0);
  assert(profile_load_file(backup_path, &previous) == STORE_OK);
  assert(profile_encoded_equal(&previous, &original));

  assert(store_remove_file(active_path));
  assert(profile_save_runtime_verified(active_path, backup_path, &candidate) ==
         STORE_NOT_FOUND);
  assert(!store_file_exists(active_path));
  assert(profile_load_file(backup_path, &previous) == STORE_OK);
  assert(profile_encoded_equal(&previous, &original));
  cleanup();
}

int main(void) {
  cleanup();

  ProfileData live;
  profile_defaults(&live);
  SessionState original = make_session();
  assert(profile_slot_set(&live.normal, &original, false));
  assert(profile_save_file(active_path, backup_path, &live) == STORE_OK);

  SessionState changed = original;
  int cell = first_playable(&changed.game);
  assert(cell >= 0);
  int value = changed.game.solution[cell];
  assert(game_place(&changed.game, cell / 9, cell % 9, value, false) ==
         GAME_INPUT_CORRECT);
  changed.elapsed_ms = UINT64_C(12000);
  assert(profile_slot_set_runtime(&live.normal, &changed, false));

  SavePolicy policy;
  save_policy_init(&policy);
  save_policy_mark(&policy, SAVE_DIRTY_NORMAL, 100);

  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  StoreStatus failed =
      profile_save_runtime_file(active_path, backup_path, &live);
  assert(failed == STORE_IO_ERROR);
  save_policy_record_result(&policy, failed, 200);
  assert(save_policy_has_error(&policy));

  const SessionState *live_session = profile_slot_session(&live.normal);
  assert(live_session);
  assert(live_session->game.puzzle[cell] == value);
  assert(live_session->elapsed_ms == UINT64_C(12000));

  ProfileData disk;
  assert(profile_load_file(active_path, &disk) == STORE_OK);
  const SessionState *disk_session = profile_slot_session(&disk.normal);
  assert(disk_session);
  assert(disk_session->game.puzzle[cell] == original.game.puzzle[cell]);
  assert(disk_session->elapsed_ms == UINT64_C(10000));

  store_test_set_fault(STORE_TEST_FAULT_NONE);
  StoreStatus retried =
      profile_save_runtime_file(active_path, backup_path, &live);
  assert(retried == STORE_OK);
  save_policy_record_result(&policy, retried, 300);
  assert(!save_policy_dirty(&policy));

  assert(profile_load_file(active_path, &disk) == STORE_OK);
  disk_session = profile_slot_session(&disk.normal);
  assert(disk_session);
  assert(disk_session->game.puzzle[cell] == value);
  assert(disk_session->elapsed_ms == UINT64_C(12000));

  cleanup();
  test_winning_checkpoint_is_durable_without_debounce();
  test_verified_replacement_faults_and_backups();
  puts("live profile remains authoritative across failed save and retry");
  return 0;
}
