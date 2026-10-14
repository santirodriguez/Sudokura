#ifndef SUDOKURA_PROFILE_H
#define SUDOKURA_PROFILE_H

#include "session.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SUDOKURA_PROFILE_CONTAINER_VERSION 2u
#define SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY 1u
#define SUDOKURA_PROFILE_CONTENT_VERSION 2u
#define SUDOKURA_PREFERENCES_CONTENT_VERSION_LEGACY 1u
#define SUDOKURA_PREFERENCES_CONTENT_VERSION_V2 2u
#define SUDOKURA_PREFERENCES_CONTENT_VERSION 3u
#define SUDOKURA_SESSION_CONTENT_VERSION_LEGACY 2u
#define SUDOKURA_SESSION_CONTENT_VERSION 3u
#define SUDOKURA_RESULT_LIMIT 128u

typedef struct {
  uint64_t high;
  uint64_t low;
} GameAttemptId;

typedef GameEdit ProfileEdit;

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
  bool retained;
  GameAttemptId attempt_id;
  uint64_t revision;
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
  GameAttemptId attempt_id;
  bool practice;
} ProfileResult;

typedef struct {
  uint16_t codec_version; /* Legacy source stays legacy until safe upgrade. */
  Preferences preferences;
  ProfileSlot normal;
  ProfileSlot daily;
  uint16_t result_count;
  ProfileResult results[SUDOKURA_RESULT_LIMIT];
  bool migration_pending;
  uint32_t migration_source_length;
  uint32_t migration_source_crc;
  uint8_t migration_source_suffix;
} ProfileData;

typedef struct {
  uint32_t finished;
  bool best_time_available;
  uint64_t best_time_ms;
} ProfileResultSummary;

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
/* Compare encoded state, excluding C struct padding and transient fields. */
bool profile_encoded_equal(const ProfileData *left, const ProfileData *right);
bool profile_slot_set(ProfileSlot *slot, const SessionState *session,
                      bool assisted);
bool profile_slot_set_runtime(ProfileSlot *slot, const SessionState *session,
                              bool assisted);
const SessionState *profile_slot_session(const ProfileSlot *slot);
bool profile_record_result(ProfileData *profile, const ProfileResult *result,
                           bool *inserted);
ProfileResultSummary profile_result_summary(const ProfileData *profile,
                                            const ProfileResult *reference);

bool profile_result_is_valid(const ProfileResult *result);
/* Shared bounded codec for independent persistent game records. */
bool profile_session_encode_blob(const ProfileSession *session,
                                 unsigned char *bytes, size_t capacity,
                                 size_t *size_out);
/* Encode a trusted live session without reconstructing its puzzle. */
bool profile_session_encode_blob_runtime(const ProfileSession *session,
                                         unsigned char *bytes, size_t capacity,
                                         size_t *size_out);
StoreStatus profile_session_decode_blob(const unsigned char *bytes, size_t size,
                                        bool expected_daily,
                                        ProfileSession *session);

StoreStatus profile_save_file(const char *path, const char *backup_path,
                              const ProfileData *profile);
StoreStatus profile_save_runtime_file(const char *path,
                                      const char *backup_path,
                                      const ProfileData *profile);
StoreStatus profile_save_runtime_verified(const char *path,
                                          const char *backup_path,
                                          const ProfileData *profile);
/* Cancel a failed activation only when disk still holds the old snapshot
   or exactly the attempted candidate. Never rotate the previous backup. */
StoreStatus profile_restore_runtime_activation(
    const char *path, const ProfileData *previous,
    const ProfileData *candidate);
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
