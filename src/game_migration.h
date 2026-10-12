#ifndef SUDOKURA_GAME_MIGRATION_H
#define SUDOKURA_GAME_MIGRATION_H

#include "profile.h"

#include <stdbool.h>
#include <stddef.h>

/* Call with the existing exclusive profile writer lock held. */
StoreStatus game_migration_upgrade(const char *root, const char *profile_path,
                                   const char *backup_path, ProfileData *profile,
                                   bool *upgraded);

/* If hot profile copies are lost, collection artifacts prevent fresh defaults. */
bool game_migration_has_artifacts(const char *root);

/* Emergency recovery never chooses between multiple different live attempts.
   All operations require the existing exclusive profile writer lock. */
typedef struct {
  size_t compatible_records;
  size_t unavailable_records;
  size_t previous_only_records;
  size_t normal_active_candidates;
  size_t daily_active_candidates;
} GameMigrationRecoverySummary;

/* Read-only preview: recovered preferences default when both hot files are
   lost and no explicit immutable-intent recovery has been chosen. */
StoreStatus game_migration_preview_collection(
    const char *root, ProfileData *candidate,
    GameMigrationRecoverySummary *summary);

/* Explicit recovery path when BOTH profile.dat and .bak are genuinely missing.
   Never silently re-import old migration sources over the collection. */
StoreStatus game_migration_recover_collection(
    const char *root, const char *profile_path, const char *backup_path,
    ProfileData *profile, GameMigrationRecoverySummary *summary);

/* Separate explicit earlier-checkpoint option: validate immutable source
   and intent, then resume its original IDs and reconcile newer records. */
StoreStatus game_migration_recover_intent(
    const char *root, const char *profile_path, const char *backup_path,
    ProfileData *profile);
/* Apply newer durable records before publishing hot Continue state. */
StoreStatus game_migration_reconcile(const char *root,
                                     const char *profile_path,
                                     const char *backup_path,
                                     ProfileData *profile);

#endif
