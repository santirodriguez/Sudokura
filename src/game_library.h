#ifndef SUDOKURA_GAME_LIBRARY_H
#define SUDOKURA_GAME_LIBRARY_H

#include "profile.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SUDOKURA_GAME_RECORD_MAX_BYTES 8192u
#define SUDOKURA_GAME_RECORD_VERSION 1u

typedef struct {
  uint64_t high;
  uint64_t low;
} GameAttemptId;

typedef enum {
  GAME_RECORD_EXACT = 1,
  GAME_RECORD_LEGACY_SUMMARY = 2
} GameRecordKind;

typedef struct {
  GameAttemptId id;
  GameAttemptId parent;
  uint64_t revision;
  uint64_t created_utc;
  uint64_t updated_utc;
  uint64_t finished_utc;
  bool retained;
  bool practice;
  bool original_daily;
  uint16_t original_daily_year;
  uint8_t original_daily_month;
  uint8_t original_daily_day;
  GameRecordKind kind;
  ProfileResult result;
  ProfileSession exact;
  uint8_t original[81];
  uint8_t solution[81];
} GameLibraryRecord;

typedef struct {
  GameAttemptId id;
  StoreStatus status;
  bool only_backup;
  GameRecordKind kind;
  ProfileResult result;
  uint64_t revision;
} GameLibraryEntry;

/* Stop a scan by returning false; cancellation is reported as STORE_CANCELLED. */
typedef bool (*GameLibraryScanFn)(const GameLibraryEntry *entry, void *context);

bool game_attempt_id_valid(GameAttemptId id);
bool game_attempt_id_equal(GameAttemptId a, GameAttemptId b);
void game_attempt_id_hex(GameAttemptId id, char out[33]);
bool game_attempt_id_parse(const char *hex, GameAttemptId *id);
bool game_library_new_id(const char *root, const GameAttemptId *reserved,
                         size_t reserved_count, GameAttemptId *out);

bool game_library_record_validate(const GameLibraryRecord *record);
bool game_library_make_exact(GameLibraryRecord *out, GameAttemptId id,
                             uint64_t revision, const ProfileSession *session,
                             bool retained);
bool game_library_make_legacy(GameLibraryRecord *out, GameAttemptId id,
                              const ProfileResult *result);

bool game_library_encode(const GameLibraryRecord *record,
                         unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES],
                         size_t *size);
StoreStatus game_library_decode(const unsigned char *bytes, size_t size,
                                GameLibraryRecord *record);
/* Validate the envelope and bounded metadata without reconstructing a puzzle. */
StoreStatus game_library_peek(const unsigned char *bytes, size_t size,
                              GameLibraryEntry *entry);
StoreStatus game_library_load(const char *root, GameAttemptId id,
                              GameLibraryRecord *record);
StoreStatus game_library_save(const char *root,
                              const GameLibraryRecord *record);
StoreStatus game_library_recover_previous(const char *root, GameAttemptId id,
                                          GameLibraryRecord *record);
StoreStatus game_library_receipt_status(const char *root, GameAttemptId id);
StoreStatus game_library_delete(const char *root, GameAttemptId id);
StoreStatus game_library_scan(const char *root, GameLibraryScanFn callback,
                              void *context);

#endif
