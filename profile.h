#ifndef SUDOKURA_PROFILE_H
#define SUDOKURA_PROFILE_H

#include "session.h"

#include <stdbool.h>
#include <stdint.h>

#define SUDOKURA_PROFILE_CONTAINER_VERSION 2u
#define SUDOKURA_PROFILE_CONTENT_VERSION 1u
#define SUDOKURA_PREFERENCES_CONTENT_VERSION 1u
#define SUDOKURA_SESSION_CONTENT_VERSION 2u
#define SUDOKURA_HISTORY_LIMIT 64u
#define SUDOKURA_RESULT_LIMIT 128u

typedef struct {
  uint8_t row;
  uint8_t column;
  uint8_t before_value;
  uint8_t after_value;
  uint16_t before_notes;
  uint16_t after_notes;
  uint8_t before_hinted;
  uint8_t after_hinted;
} ProfileEdit;

typedef struct {
  SessionState session;
  bool assisted;
  uint16_t undo_count;
  uint16_t redo_count;
  ProfileEdit undo[SUDOKURA_HISTORY_LIMIT];
  ProfileEdit redo[SUDOKURA_HISTORY_LIMIT];
} ProfileSession;

typedef struct {
  bool present;
  ProfileSession value;
} ProfileSlot;

typedef struct {
  uint64_t seed;
  uint32_t generator_revision;
  GameDifficulty difficulty;
  GameMode mode;
  SessionStatus status;
  bool assisted;
  bool is_daily;
  uint64_t elapsed_ms;
  uint32_t mistakes;
  uint32_t strikes;
  uint16_t daily_year;
  uint8_t daily_month;
  uint8_t daily_day;
} ProfileResult;

typedef struct {
  Preferences preferences;
  ProfileSlot normal;
  ProfileSlot daily;
  uint16_t result_count;
  ProfileResult results[SUDOKURA_RESULT_LIMIT];
} ProfileData;

typedef struct {
  const char *session_path;
  const char *preferences_path;
  const char *audio_levels_path;
  const char *session_backup_path;
  const char *preferences_backup_path;
  const char *audio_levels_backup_path;
} ProfileLegacyPaths;

void profile_defaults(ProfileData *profile);
bool profile_validate(const ProfileData *profile);
bool profile_slot_set(ProfileSlot *slot, const SessionState *session,
                      bool assisted);
bool profile_slot_set_runtime(ProfileSlot *slot, const SessionState *session,
                              bool assisted);
const SessionState *profile_slot_session(const ProfileSlot *slot);

StoreStatus profile_save_file(const char *path, const char *backup_path,
                              const ProfileData *profile);
StoreStatus profile_save_runtime_file(const char *path,
                                      const char *backup_path,
                                      const ProfileData *profile);
StoreStatus profile_load_file(const char *path, ProfileData *profile);
StoreStatus profile_recover_previous(const char *profile_path,
                                     const char *profile_backup_path,
                                     ProfileData *profile);
StoreStatus profile_load_or_migrate_v12(const char *profile_path,
                                        const char *profile_backup_path,
                                        const ProfileLegacyPaths *legacy,
                                        ProfileData *profile,
                                        bool *migrated);

#endif
