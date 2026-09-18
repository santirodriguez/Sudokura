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
  game_new_difficulty(&state.game, UINT64_C(0x445566778899aabb),
                      DIFFICULTY_MEDIUM);
  state.mode = MODE_CLASSIC;
  state.selected_row = 4;
  state.selected_column = 4;
  state.status = SESSION_ACTIVE;
  state.elapsed_ms = UINT64_C(10000);
  assert(session_validate(&state));
  return state;
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
  puts("live profile remains authoritative across failed save and retry");
  return 0;
}
