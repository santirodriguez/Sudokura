#ifndef SUDOKURA_GAME_LIBRARY_H
#define SUDOKURA_GAME_LIBRARY_H

#include "profile.h"
#include "store_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SUDOKURA_GAME_RECORD_MAX_BYTES 8192u
#define SUDOKURA_GAME_RECORD_VERSION 1u

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
  /* Metadata/CRC status of .bak; STORE_OK still requires full canonical
     validation before an explicit recovery. */
  StoreStatus previous_status;
  GameRecordKind kind;
  ProfileResult result;
  uint64_t revision;
  uint64_t created_utc;
  uint64_t updated_utc;
  uint64_t finished_utc;
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
/* Only for a trusted in-memory board, never a decoded file. */
bool game_library_record_validate_runtime(const GameLibraryRecord *record);
bool game_library_make_exact(GameLibraryRecord *out, GameAttemptId id,
                             uint64_t revision, const ProfileSession *session,
                             bool retained);
bool game_library_make_exact_runtime(
    GameLibraryRecord *out, GameAttemptId id, uint64_t revision,
    const ProfileSession *session, bool retained);
bool game_library_make_legacy(GameLibraryRecord *out, GameAttemptId id,
                              const ProfileResult *result);

bool game_library_encode(const GameLibraryRecord *record,
                         unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES],
                         size_t *size);
bool game_library_encode_runtime(
    const GameLibraryRecord *record,
    unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES], size_t *size);
StoreStatus game_library_decode(const unsigned char *bytes, size_t size,
                                GameLibraryRecord *record);
/* Validate the envelope and bounded metadata without reconstructing a puzzle. */
StoreStatus game_library_peek(const unsigned char *bytes, size_t size,
                              GameLibraryEntry *entry);
StoreStatus game_library_load(const char *root, GameAttemptId id,
                              GameLibraryRecord *record);
/* Decide a same-attempt Load without rolling back a newer hot revision.
   The record is a canonically validated exact record from game_library_load.
   On a conflict, callers must leave both profile and live board untouched. */
StoreStatus game_library_resolve_hot_record(const ProfileSlot *hot,
                                            const GameLibraryRecord *record,
                                            bool *prefer_record);
StoreStatus game_library_save(const char *root,
                              const GameLibraryRecord *record);

/* Per-live-slot validated on-disk bytes; initialized to zero by the owner.
   Cold/mismatched state falls back to the full canonical disk validator. */
typedef struct {
  bool valid;
  GameAttemptId id;
  uint64_t revision;
  SessionStatus status;
  uint64_t created_utc;
  uint64_t updated_utc;
  uint64_t finished_utc;
  size_t length;
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
} GameLibraryHotCache;

/* Optimizes a trusted live session without changing canonical load/recovery. */
StoreStatus game_library_save_runtime(const char *root,
                                      const GameLibraryRecord *record,
                                      GameLibraryHotCache *cache);
/* Deterministic UTC injection for tests; 0 indicates unavailable clock. */
StoreStatus game_library_save_runtime_at(const char *root,
                                         const GameLibraryRecord *record,
                                         GameLibraryHotCache *cache,
                                         uint64_t utc_seconds);
/* First Save/Keep of an unretained hot slot: record before hot pointer.
   retry_pending allows only this writer's interrupted first retention.
   The output slot is unchanged on any failure. */
/* Verify a successful explicit Save against the durable full record.
   Equality includes entered digits, pencil notes, selection and Undo/Redo. */
StoreStatus game_library_verify_retained_snapshot(
    const char *root, const ProfileSlot *slot);

StoreStatus game_library_retain_hot_slot_runtime(
    const char *root, const ProfileSlot *hot, GameLibraryHotCache *cache,
    bool retry_pending, ProfileSlot *out);
StoreStatus game_library_recover_previous(const char *root, GameAttemptId id,
                                          GameLibraryRecord *record);
StoreStatus game_library_receipt_status(const char *root, GameAttemptId id);
StoreStatus game_library_delete(const char *root, GameAttemptId id);
/* True when the receipt has been durably acknowledged. A later cleanup
   failure may still be reported by the returned status. */
StoreStatus game_library_delete_with_receipt(const char *root,
                                             GameAttemptId id,
                                             bool *receipt_committed);
/* One small durable Replay operation per writer, independent of any hot
   profile pointer. A finished/abandoned intent permits a distinct Replay.
   An unresolved prepared intent must keep its original attempt ID. */
typedef enum {
  GAME_REPLAY_PREPARED = 1,
  GAME_REPLAY_COMMITTED = 2,
  GAME_REPLAY_ABANDONED = 3
} GameReplayIntentState;

typedef struct {
  GameAttemptId source_id;
  GameAttemptId attempt_id;
  GameReplayIntentState state;
} GameReplayIntent;

StoreStatus game_library_replay_intent_load(const char *root,
                                            GameReplayIntent *intent);
StoreStatus game_library_replay_intent_write(const char *root,
                                             const GameReplayIntent *intent);
/* Require that a prepared attempt is still the untouched original Replay,
   not an unrelated or progressed practice game with the same parent ID. */
bool game_library_replay_pending_matches(const GameLibraryRecord *source,
                                         const GameLibraryRecord *candidate);
/* Resolve an already linked Prepared Replay without changing any state.
   Resume only if the selected source is the source of that Replay; a
   different selected source may begin its own attempt after finalization.
   Conflicting or newer unlinked records must be reconciled separately. */
StoreStatus game_library_replay_linked_selection(
    const GameReplayIntent *intent, GameAttemptId selected_source,
    const ProfileSlot *hot, const GameLibraryRecord *record,
    bool *resume_selected);


/* Build a separately identified, retained practice attempt from an immutable
   exact terminal board. Never modifies source or creates a hot Daily slot. */
bool game_library_prepare_replay(GameLibraryRecord *out,
                                 const GameLibraryRecord *source,
                                 GameAttemptId fresh_id);
/* Remove only references belonging to a deleted attempt. Pure, all-or-nothing. */
bool game_library_forget_attempt(ProfileData *out, const ProfileData *source,
                                 GameAttemptId id);

/* Match validated catalogue metadata without generating a puzzle or
   consulting the wall clock; the caller supplies the frozen local date. */
bool game_library_is_original_daily_on(const GameLibraryEntry *entry,
                                       int year, int month, int day);

/* Only damaged/missing primaries with validated earlier metadata offer
   explicit recovery. Future/incompatible and I/O-error files stay blocked. */
bool game_library_entry_recoverable(const GameLibraryEntry *entry);

/* Metadata-only lookup result. Saturate at two to flag conflicting originals. */
typedef struct {
  GameLibraryEntry match;
  unsigned matching_attempts;
  bool uncertain;
} GameDailyLookup;

/* Process one validated catalogue item without loading full boards. */
void game_library_daily_lookup_consider(GameDailyLookup *lookup,
                                        const GameLibraryEntry *entry,
                                        int year, int month, int day);

StoreStatus game_library_scan(const char *root, GameLibraryScanFn callback,
                              void *context);

/* Incremental catalogue scan: one open iterator and bounded names/frame.
   The selected callback is still responsible for bounded metadata storage.
   Cursor must be initialized to zero and closed on cancellation. */
typedef struct {
  StoreDirectoryCursor directory;
  char root[SUDOKURA_STORE_PATH_CAPACITY];
  bool finished;
} GameLibraryScanCursor;
StoreStatus game_library_scan_begin(const char *root, GameLibraryScanCursor *scan);
StoreStatus game_library_scan_step(GameLibraryScanCursor *scan, size_t name_budget,
                                   GameLibraryScanFn callback, void *context,
                                   bool *finished);
void game_library_scan_close(GameLibraryScanCursor *scan);

#endif
