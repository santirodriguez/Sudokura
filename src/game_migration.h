#ifndef SUDOKURA_GAME_MIGRATION_H
#define SUDOKURA_GAME_MIGRATION_H

#include "profile.h"

#include <stdbool.h>

/* Call with the existing exclusive profile writer lock held. */
StoreStatus game_migration_upgrade(const char *root, const char *profile_path,
                                   const char *backup_path, ProfileData *profile,
                                   bool *upgraded);

/* If hot profile copies are lost, collection artifacts prevent fresh defaults. */
bool game_migration_has_artifacts(const char *root);

#endif
