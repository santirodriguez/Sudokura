#ifndef SUDOKURA_STORAGE_H
#define SUDOKURA_STORAGE_H

#include <stdbool.h>

#include "session.h"

typedef bool (*StorageSessionSaveFn)(const char *path,
                                     const SessionState *state);
typedef bool (*StoragePreferencesSaveFn)(const char *path,
                                         const Preferences *preferences);

typedef struct {
  StorageSessionSaveFn save_session;
  StoragePreferencesSaveFn save_preferences;
} StorageOps;

static inline bool storage_save_session(const StorageOps *ops,
                                        const char *path,
                                        const SessionState *state) {
  return ops && ops->save_session && ops->save_session(path, state);
}

static inline bool storage_save_preferences(const StorageOps *ops,
                                            const char *path,
                                            const Preferences *preferences) {
  return ops && ops->save_preferences &&
         ops->save_preferences(path, preferences);
}

#endif
