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

int main(void) {
  test_grouped_checkpoint_window();
  test_error_preserves_dirty_state();
  test_independent_slot_dirty_bits();
  puts("save policy dirty-state, retry, and checkpoint-window tests passed");
  return 0;
}
