#include "save_policy.h"

void save_policy_init(SavePolicy *policy) {
  if (!policy) return;
  policy->dirty_mask = SAVE_DIRTY_NONE;
  policy->dirty_since_ms = 0;
  policy->last_change_ms = 0;
  policy->last_status = STORE_OK;
}

void save_policy_mark(SavePolicy *policy, unsigned dirty_mask, uint64_t now_ms) {
  if (!policy || dirty_mask == SAVE_DIRTY_NONE) return;
  if (policy->dirty_mask == SAVE_DIRTY_NONE) policy->dirty_since_ms = now_ms;
  policy->dirty_mask |= dirty_mask;
  policy->last_change_ms = now_ms;
}

bool save_policy_dirty(const SavePolicy *policy) {
  return policy && policy->dirty_mask != SAVE_DIRTY_NONE;
}

bool save_policy_has_error(const SavePolicy *policy) {
  return save_policy_dirty(policy) && policy->last_status != STORE_OK;
}

bool save_policy_slot_dirty(const SavePolicy *policy, bool daily) {
  if (!policy) return false;
  unsigned bit = daily ? SAVE_DIRTY_DAILY : SAVE_DIRTY_NORMAL;
  return (policy->dirty_mask & bit) != 0;
}

bool save_policy_checkpoint_due(const SavePolicy *policy, uint64_t now_ms) {
  if (!save_policy_dirty(policy)) return false;
  if (now_ms < policy->dirty_since_ms || now_ms < policy->last_change_ms)
    return true;
  return now_ms - policy->last_change_ms >= SUDOKURA_AUTOSAVE_QUIET_MS ||
         now_ms - policy->dirty_since_ms >= SUDOKURA_AUTOSAVE_MAX_MS;
}

void save_policy_record_result(SavePolicy *policy, StoreStatus status,
                               uint64_t now_ms) {
  if (!policy) return;
  policy->last_status = status;
  if (status == STORE_OK) {
    policy->dirty_mask = SAVE_DIRTY_NONE;
    policy->dirty_since_ms = now_ms;
    policy->last_change_ms = now_ms;
  }
}
