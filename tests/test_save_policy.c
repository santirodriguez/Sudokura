#include "save_policy.h"

#include <assert.h>
#include <stdio.h>

static void test_grouped_checkpoint_window(void) {
  SavePolicy policy;
  save_policy_init(&policy);
  assert(!save_policy_dirty(&policy));
  assert(!save_policy_checkpoint_due(&policy, 1000));

  save_policy_mark(&policy, SAVE_DIRTY_NORMAL, 1000);
  assert(save_policy_dirty(&policy));
  assert(!save_policy_checkpoint_due(&policy, 1600));
  assert(save_policy_checkpoint_due(&policy, 1750));

  save_policy_mark(&policy, SAVE_DIRTY_NORMAL, 1700);
  assert(!save_policy_checkpoint_due(&policy, 2200));
  assert(save_policy_checkpoint_due(&policy, 6000));
}

static void test_error_preserves_dirty_state(void) {
  SavePolicy policy;
  save_policy_init(&policy);
  save_policy_mark(&policy, SAVE_DIRTY_NORMAL | SAVE_DIRTY_PREFERENCES, 10);
  save_policy_record_result(&policy, STORE_IO_ERROR, 20);
  assert(save_policy_dirty(&policy));
  assert(save_policy_has_error(&policy));
  assert(save_policy_slot_dirty(&policy, false));
  assert(!save_policy_slot_dirty(&policy, true));
  assert(!save_policy_checkpoint_due(&policy, 1000));
  assert(save_policy_checkpoint_due(&policy, 5020));

  save_policy_record_result(&policy, STORE_OK, 5030);
  assert(!save_policy_dirty(&policy));
  assert(!save_policy_has_error(&policy));
}

static void test_independent_slot_dirty_bits(void) {
  SavePolicy policy;
  save_policy_init(&policy);
  save_policy_mark(&policy, SAVE_DIRTY_DAILY, 100);
  assert(save_policy_slot_dirty(&policy, true));
  assert(!save_policy_slot_dirty(&policy, false));
  save_policy_mark(&policy, SAVE_DIRTY_NORMAL, 110);
  assert(save_policy_slot_dirty(&policy, true));
  assert(save_policy_slot_dirty(&policy, false));
  save_policy_mark(&policy, SAVE_DIRTY_RESULTS, 120);
  assert((policy.dirty_mask & SAVE_DIRTY_RESULTS) != 0);
}

typedef struct {
  StoreStatus status;
  unsigned calls;
} CheckpointProbe;

static StoreStatus probe_write(void *context) {
  CheckpointProbe *probe = (CheckpointProbe *)context;
  ++probe->calls;
  return probe->status;
}

static void test_terminal_checkpoint_is_synchronous_and_retryable(void) {
  SavePolicy policy;
  save_policy_init(&policy);
  save_policy_mark(&policy, SAVE_DIRTY_NORMAL | SAVE_DIRTY_RESULTS, 100);
  assert(!save_policy_checkpoint_due(&policy, 100));
  CheckpointProbe probe = {STORE_OK, 0};
  assert(save_policy_checkpoint_now(&policy, probe_write, &probe, 100));
  assert(probe.calls == 1);
  assert(!save_policy_dirty(&policy));

  save_policy_mark(&policy, SAVE_DIRTY_NORMAL, 200);
  probe.status = STORE_IO_ERROR;
  assert(!save_policy_checkpoint_now(&policy, probe_write, &probe, 200));
  assert(probe.calls == 2);
  assert(save_policy_has_error(&policy));
  assert(save_policy_slot_dirty(&policy, false));

  probe.status = STORE_OK;
  assert(save_policy_checkpoint_now(&policy, probe_write, &probe, 300));
  assert(probe.calls == 3);
  assert(!save_policy_dirty(&policy));
}

int main(void) {
  test_grouped_checkpoint_window();
  test_error_preserves_dirty_state();
  test_independent_slot_dirty_bits();
  test_terminal_checkpoint_is_synchronous_and_retryable();
  puts("save policy dirty-state, retry, and checkpoint-window tests passed");
  return 0;
}
