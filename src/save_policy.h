#ifndef SUDOKURA_SAVE_POLICY_H
#define SUDOKURA_SAVE_POLICY_H

#include "store_status.h"

#include <stdbool.h>
#include <stdint.h>

#define SUDOKURA_AUTOSAVE_QUIET_MS UINT64_C(750)
#define SUDOKURA_AUTOSAVE_MAX_MS UINT64_C(5000)

enum {
  SAVE_DIRTY_NONE = 0u,
  SAVE_DIRTY_PREFERENCES = 1u << 0,
  SAVE_DIRTY_NORMAL = 1u << 1,
  SAVE_DIRTY_DAILY = 1u << 2,
  SAVE_DIRTY_RESULTS = 1u << 3
};

typedef struct {
  unsigned dirty_mask;
  uint64_t dirty_since_ms;
  uint64_t last_change_ms;
  StoreStatus last_status;
} SavePolicy;

void save_policy_init(SavePolicy *policy);
void save_policy_mark(SavePolicy *policy, unsigned dirty_mask, uint64_t now_ms);
bool save_policy_dirty(const SavePolicy *policy);
bool save_policy_has_error(const SavePolicy *policy);
/* A transition may release its live board only after a successful complete
   checkpoint, with no outstanding dirty persistence domains. */
bool save_policy_transition_ready(const SavePolicy *policy,
                                  bool checkpoint_succeeded);
bool save_policy_slot_dirty(const SavePolicy *policy, bool daily);
/* Discard in a replacement gate is allowed only for one isolated active
   unretained slot, never for results, preferences or pending first Saves. */
bool save_policy_replacement_discard_allowed(
    const SavePolicy *policy, bool daily, bool unretained_active,
    bool capture_failed, unsigned keep_pending_mask);
bool save_policy_checkpoint_due(const SavePolicy *policy, uint64_t now_ms);
typedef StoreStatus (*SavePolicyWriteFn)(void *context);

/* Synchronous checkpoint; bypasses the regular autosave quiet window. */
bool save_policy_checkpoint_now(SavePolicy *policy, SavePolicyWriteFn write,
                                void *context, uint64_t now_ms);
void save_policy_record_result(SavePolicy *policy, StoreStatus status,
                               uint64_t now_ms);

#endif
