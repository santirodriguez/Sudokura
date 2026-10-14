#include "game_library.h"

#include "seed.h"
#include "store_io.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

enum { RECEIPT_BYTES = 34u };
static const unsigned char receipt_magic[8] = {
    'S', 'U', 'D', 'O', 'D', 'E', 'L', '1'};

static bool games_directory(char out[SUDOKURA_STORE_PATH_CAPACITY],
                            const char *root) {
  if (!out || !root || !root[0]) return false;
  size_t len = strlen(root);
  int n = snprintf(out, SUDOKURA_STORE_PATH_CAPACITY, "%s%sgames", root,
                   root[len - 1] == '/' || root[len - 1] == '\\' ? "" : "/");
  return n >= 0 && (size_t)n < SUDOKURA_STORE_PATH_CAPACITY;
}

static bool game_path(char out[SUDOKURA_STORE_PATH_CAPACITY],
                      const char *root, GameAttemptId id,
                      const char *suffix) {
  if (!out || !suffix || !game_attempt_id_valid(id)) return false;
  char folder[SUDOKURA_STORE_PATH_CAPACITY];
  if (!games_directory(folder, root)) return false;
  char hex[33];
  game_attempt_id_hex(id, hex);
  size_t folder_len = strlen(folder);
  size_t suffix_len = strlen(suffix);
  /* Separator + 32 hex digits + terminating NUL require 34 bytes. */
  if (folder_len > SUDOKURA_STORE_PATH_CAPACITY - 34u ||
      suffix_len > SUDOKURA_STORE_PATH_CAPACITY - folder_len - 34u)
    return false;
  memcpy(out, folder, folder_len);
  out[folder_len] = '/';
  memcpy(out + folder_len + 1u, hex, 32u);
  memcpy(out + folder_len + 33u, suffix, suffix_len + 1u);
  return true;
}

static uint32_t checksum(const unsigned char *data, size_t count) {
  uint32_t crc = UINT32_C(0xffffffff);
  for (size_t i = 0; i < count; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (UINT32_C(0xedb88320) &
                            (0u - (crc & 1u)));
  }
  return ~crc;
}
static void write16(unsigned char *bytes, uint16_t value) {
  bytes[0] = (unsigned char)value;
  bytes[1] = (unsigned char)(value >> 8);
}
static void write32(unsigned char *bytes, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[shift / 8] = (unsigned char)(value >> shift);
}
static uint16_t read16(const unsigned char *bytes) {
  return (uint16_t)(bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8));
}
static uint32_t read32(const unsigned char *bytes) {
  uint32_t value = 0;
  for (unsigned shift = 0; shift < 32; shift += 8)
    value |= (uint32_t)bytes[shift / 8] << shift;
  return value;
}
static void write64(unsigned char *bytes, uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8)
    bytes[shift / 8] = (unsigned char)(value >> shift);
}
static uint64_t read64(const unsigned char *bytes) {
  uint64_t value = 0;
  for (unsigned shift = 0; shift < 64; shift += 8)
    value |= (uint64_t)bytes[shift / 8] << shift;
  return value;
}

StoreStatus game_library_receipt_status(const char *root, GameAttemptId id) {
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!game_path(path, root, id, ".deleted")) return STORE_IO_ERROR;
  unsigned char bytes[RECEIPT_BYTES];
  size_t count = 0;
  StoreStatus status = store_read_file(path, bytes, sizeof(bytes), &count);
  if (status == STORE_CORRUPT) {
    /* An oversized receipt may be an unsupported future version, not a
       damaged v1 receipt. Probe only the bounded magic/version prefix. */
    unsigned char prefix[10];
    size_t got = 0;
    status = store_read_prefix(path, prefix, sizeof(prefix), &got);
    if (status != STORE_OK) return status;
    if (got >= sizeof(prefix) &&
        memcmp(prefix, receipt_magic, sizeof(receipt_magic)) == 0 &&
        read16(prefix + 8) != 1)
      return STORE_INCOMPATIBLE;
    return STORE_CORRUPT;
  }
  if (status != STORE_OK) return status;
  if (count >= 10u &&
      memcmp(bytes, receipt_magic, sizeof(receipt_magic)) == 0 &&
      read16(bytes + 8) != 1)
    return STORE_INCOMPATIBLE;
  if (count != sizeof(bytes) ||
      memcmp(bytes, receipt_magic, sizeof(receipt_magic)) != 0)
    return STORE_CORRUPT;
  if (read32(bytes + 10) != 16 ||
      read32(bytes + 14) != checksum(bytes + 18, 16) ||
      read64(bytes + 18) != id.high ||
      read64(bytes + 26) != id.low)
    return STORE_CORRUPT;
  return STORE_OK;
}

static StoreStatus read_record_file(const char *path,
                                    unsigned char *bytes, size_t capacity,
                                    size_t *size_out) {
  StoreStatus status = store_read_file(path, bytes, capacity, size_out);
  if (status != STORE_CORRUPT) return status;

  /* Detect future container/payload versions before the current 8 KiB cap. */
  unsigned char prefix[22];
  size_t got = 0;
  status = store_read_prefix(path, prefix, sizeof(prefix), &got);
  if (status != STORE_OK) return status;
  if (got >= 10u && memcmp(prefix, "SUDOGAME", 8u) == 0) {
    uint16_t version = (uint16_t)(prefix[8] |
                                 (uint16_t)((uint16_t)prefix[9] << 8));
    if (version != SUDOKURA_GAME_RECORD_VERSION)
      return STORE_INCOMPATIBLE;
    if (got >= sizeof(prefix)) {
      uint16_t payload_version = (uint16_t)(prefix[20] |
                               (uint16_t)((uint16_t)prefix[21] << 8));
      if (payload_version != 1u) return STORE_INCOMPATIBLE;
    }
  }
  return STORE_CORRUPT;
}

static StoreStatus inspect_record_file(const char *path, GameAttemptId id,
                                       GameLibraryEntry *entry) {
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t size = 0;
  StoreStatus status = read_record_file(path, bytes, sizeof(bytes), &size);
  if (status != STORE_OK) return status;
  GameLibraryEntry candidate;
  status = game_library_peek(bytes, size, &candidate);
  if (status != STORE_OK) return status;
  if (!game_attempt_id_equal(candidate.id, id)) return STORE_CORRUPT;
  if (entry) *entry = candidate;
  return STORE_OK;
}

static StoreStatus load_from_suffix(const char *root, GameAttemptId id,
                                    const char *suffix,
                                    GameLibraryRecord *record) {
  if (!record) return STORE_IO_ERROR;
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!game_path(path, root, id, suffix)) return STORE_IO_ERROR;
  unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t count = 0;
  StoreStatus status = read_record_file(path, bytes, sizeof(bytes), &count);
  if (status != STORE_OK) return status;
  GameLibraryRecord loaded;
  status = game_library_decode(bytes, count, &loaded);
  if (status != STORE_OK) return status;
  if (!game_attempt_id_equal(loaded.id, id)) return STORE_CORRUPT;
  *record = loaded;
  return STORE_OK;
}

StoreStatus game_library_load(const char *root, GameAttemptId id,
                              GameLibraryRecord *record) {
  if (!record || !root || !game_attempt_id_valid(id))
    return STORE_IO_ERROR;
  StoreStatus receipt = game_library_receipt_status(root, id);
  if (receipt == STORE_OK) return STORE_NOT_FOUND;
  if (receipt != STORE_NOT_FOUND) return receipt;
  return load_from_suffix(root, id, ".dat", record);
}

/* This runs only for a user-selected, canonically loaded full record,
   never during catalogue enumeration or autosave. */
StoreStatus game_library_resolve_hot_record(const ProfileSlot *hot,
                                            const GameLibraryRecord *record,
                                            bool *prefer_record) {
  if (!hot || !record || !prefer_record) return STORE_IO_ERROR;
  *prefer_record = false;
  if (!hot->present || !game_attempt_id_valid(hot->attempt_id) ||
      !game_attempt_id_equal(hot->attempt_id, record->id) ||
      record->kind != GAME_RECORD_EXACT || hot->revision == 0 ||
      record->revision == 0)
    return STORE_CORRUPT;

  const SessionState *live = &hot->value.session;
  const SessionState *disk = &record->exact.session;
  if (live->game.seed != disk->game.seed ||
      live->game.generator_revision != disk->game.generator_revision ||
      live->game.difficulty != disk->game.difficulty ||
      live->mode != disk->mode || live->is_daily != disk->is_daily ||
      live->daily_year != disk->daily_year ||
      live->daily_month != disk->daily_month ||
      live->daily_day != disk->daily_day)
    return STORE_CORRUPT;

  /* A finished attempt is immutable; never reactivate it from an older
     active record, nor silently change its terminal outcome. */
  if (live->status != SESSION_ACTIVE &&
      disk->status != live->status)
    return STORE_CORRUPT;
  if (record->revision < hot->revision)
    return hot->retained ? STORE_IO_ERROR : STORE_OK;
  if (record->revision > hot->revision) {
    *prefer_record = true;
    return STORE_OK;
  }

  /* Equal revisions must represent exactly the same serialized state,
     including notes, elapsed time and Undo/Redo—not merely the same seed. */
  unsigned char hot_bytes[4096], disk_bytes[4096];
  size_t hot_size = 0, disk_size = 0;
  if (!profile_session_encode_blob(&hot->value, hot_bytes,
                                   sizeof(hot_bytes), &hot_size) ||
      !profile_session_encode_blob(&record->exact, disk_bytes,
                                   sizeof(disk_bytes), &disk_size) ||
      hot_size != disk_size || memcmp(hot_bytes, disk_bytes, hot_size) != 0)
    return STORE_CORRUPT;
  if (hot->retained && !record->retained) return STORE_CORRUPT;
  *prefer_record = record->retained && !hot->retained;
  return STORE_OK;
}

bool game_library_new_id(const char *root, const GameAttemptId *reserved,
                         size_t reserved_count, GameAttemptId *out) {
  if (!root || !out || (reserved_count && !reserved)) return false;
  GameReplayIntent reserved_replay = {0};
  bool intent_owns_id =
      game_library_replay_intent_load(root, &reserved_replay) == STORE_OK;
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    GameAttemptId value;
    if (!seed_system_random_u64(&value.high))
      value.high = seed_random_u64();
    if (!seed_system_random_u64(&value.low))
      value.low = seed_random_u64();
    if (!game_attempt_id_valid(value)) continue;
    bool collision = intent_owns_id &&
        game_attempt_id_equal(value, reserved_replay.attempt_id);
    for (size_t i = 0; i < reserved_count; ++i)
      if (game_attempt_id_equal(reserved[i], value)) collision = true;
    if (collision) continue;
    const char *suffixes[] = {
        ".dat", ".dat.bak", ".deleted", ".dat.pre-recovery"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
      char path[SUDOKURA_STORE_PATH_CAPACITY];
      if (!game_path(path, root, value, suffixes[i])) return false;
      if (store_file_exists(path)) { collision = true; break; }
    }
    if (!collision) { *out = value; return true; }
  }
  return false;
}

StoreStatus game_library_save(const char *root,
                              const GameLibraryRecord *record) {
  if (!root || !record || !game_library_record_validate(record))
    return STORE_IO_ERROR;
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  char backup[SUDOKURA_STORE_PATH_CAPACITY];
  char folder[SUDOKURA_STORE_PATH_CAPACITY];
  if (!game_path(path, root, record->id, ".dat") ||
      !game_path(backup, root, record->id, ".dat.bak") ||
      !games_directory(folder, root))
    return STORE_IO_ERROR;

  StoreStatus receipt = game_library_receipt_status(root, record->id);
  if (receipt != STORE_NOT_FOUND)
    return receipt == STORE_OK ? STORE_IO_ERROR : receipt;

  unsigned char encoded[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t length = 0;
  if (!game_library_encode(record, encoded, &length)) return STORE_IO_ERROR;

  GameLibraryRecord previous;
  StoreStatus status = game_library_load(root, record->id, &previous);
  if (status == STORE_OK) {
    unsigned char old_bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
    size_t old_length = 0;
    if (!game_library_encode(&previous, old_bytes, &old_length))
      return STORE_IO_ERROR;
    if (length == old_length &&
        memcmp(encoded, old_bytes, length) == 0)
      return STORE_OK; /* An identical retry does not rotate the backup. */
    if (previous.revision >= record->revision)
      return STORE_IO_ERROR;
    if (previous.kind == GAME_RECORD_EXACT &&
        previous.result.status != SESSION_ACTIVE)
      return STORE_IO_ERROR;
    if (previous.kind == GAME_RECORD_LEGACY_SUMMARY) {
      /* A legacy summary can only gain its matching exact terminal board. */
      const ProfileResult *a = &previous.result;
      const ProfileResult *b = &record->result;
      if (record->kind != GAME_RECORD_EXACT ||
          a->seed != b->seed ||
          a->generator_revision != b->generator_revision ||
          a->difficulty != b->difficulty || a->mode != b->mode ||
          a->status != b->status || a->assisted != b->assisted ||
          a->is_daily != b->is_daily || a->elapsed_ms != b->elapsed_ms ||
          a->mistakes != b->mistakes || a->strikes != b->strikes ||
          a->daily_year != b->daily_year ||
          a->daily_month != b->daily_month ||
          a->daily_day != b->daily_day)
        return STORE_IO_ERROR;
    }
  } else if (status != STORE_NOT_FOUND) {
    return status;
  } else if (store_file_exists(backup)) {
    /* New retention may follow many unretained hot-profile revisions. */
    return STORE_IO_ERROR;
  }

  status = store_ensure_directory(folder);
  if (status != STORE_OK) return status;
  status = store_atomic_write(path, backup, encoded, length);
  if (status != STORE_OK) return status;
  GameLibraryRecord verified;
  status = game_library_load(root, record->id, &verified);
  if (status != STORE_OK) return status;
  unsigned char readback[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t readback_length = 0;
  if (!game_library_encode(&verified, readback, &readback_length) ||
      readback_length != length ||
      memcmp(encoded, readback, length) != 0)
    return STORE_CORRUPT;
  return STORE_OK;
}

static void hot_cache_store(GameLibraryHotCache *cache,
                            const GameLibraryRecord *record,
                            const unsigned char *bytes, size_t length) {
  cache->valid = true;
  cache->id = record->id;
  cache->revision = record->revision;
  cache->status = record->result.status;
  cache->created_utc = record->created_utc;
  cache->updated_utc = record->updated_utc;
  cache->finished_utc = record->finished_utc;
  cache->length = length;
  memcpy(cache->bytes, bytes, length);
}

StoreStatus game_library_save_runtime_at(const char *root,
                                         const GameLibraryRecord *record,
                                         GameLibraryHotCache *cache,
                                         uint64_t utc_seconds) {
  if (!root || !record || !cache ||
      !game_library_record_validate_runtime(record) ||
      record->kind != GAME_RECORD_EXACT)
    return STORE_IO_ERROR;

  char path[SUDOKURA_STORE_PATH_CAPACITY];
  char backup[SUDOKURA_STORE_PATH_CAPACITY];
  char folder[SUDOKURA_STORE_PATH_CAPACITY];
  if (!game_path(path, root, record->id, ".dat") ||
      !game_path(backup, root, record->id, ".dat.bak") ||
      !games_directory(folder, root))
    return STORE_IO_ERROR;

  StoreStatus receipt = game_library_receipt_status(root, record->id);
  if (receipt != STORE_NOT_FOUND)
    return receipt == STORE_OK ? STORE_IO_ERROR : receipt;

  unsigned char prior[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t prior_length = 0;
  StoreStatus status = store_read_file(path, prior, sizeof(prior),
                                       &prior_length);
  bool trusted_cache = cache->valid &&
                       game_attempt_id_equal(cache->id, record->id);
  if (trusted_cache && (status != STORE_OK ||
      prior_length != cache->length ||
      memcmp(prior, cache->bytes, cache->length) != 0)) {
    cache->valid = false;
    /* A disappearing/changed on-disk record is not a new attempt. */
    if (status == STORE_NOT_FOUND) return STORE_IO_ERROR;
    if (status != STORE_OK) return status;
    trusted_cache = false;
  }

  GameLibraryRecord dated = *record;
  uint64_t previous_revision = 0;
  SessionStatus previous_status = SESSION_ACTIVE;
  uint64_t previous_updated = 0;
  uint64_t previous_finished = 0;
  if (status == STORE_OK) {
    if (trusted_cache) {
      previous_revision = cache->revision;
      previous_status = cache->status;
      dated.created_utc = cache->created_utc;
      previous_updated = cache->updated_utc;
      previous_finished = cache->finished_utc;
    } else {
      /* First access after startup or external mutation remains canonical. */
      GameLibraryRecord previous;
      status = game_library_load(root, record->id, &previous);
      if (status != STORE_OK) return status;
      if (previous.kind != GAME_RECORD_EXACT) return STORE_IO_ERROR;
      previous_revision = previous.revision;
      previous_status = previous.result.status;
      dated.created_utc = previous.created_utc;
      previous_updated = previous.updated_utc;
      previous_finished = previous.finished_utc;
    }
    if (record->revision == previous_revision) {
      /* An identical retry retains its original timestamps and backup. */
      dated.updated_utc = previous_updated;
      dated.finished_utc = previous_finished;
    } else {
      dated.updated_utc = utc_seconds > previous_updated
                              ? utc_seconds : previous_updated;
      dated.finished_utc = dated.result.status == SESSION_ACTIVE
          ? 0
          : (previous_status == SESSION_ACTIVE
              ? dated.updated_utc : previous_finished);
    }
  } else if (status == STORE_NOT_FOUND) {
    if (store_file_exists(backup))
      return STORE_IO_ERROR; /* Only an explicit recovery can reuse a backup. */
    dated.created_utc = utc_seconds;
    dated.updated_utc = utc_seconds;
    dated.finished_utc = dated.result.status == SESSION_ACTIVE
                             ? 0 : utc_seconds;
  } else {
    return status;
  }

  unsigned char encoded[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t length = 0;
  if (!game_library_encode_runtime(&dated, encoded, &length))
    return STORE_IO_ERROR;
  if (status == STORE_OK) {
    if (length == prior_length && memcmp(encoded, prior, length) == 0) {
      hot_cache_store(cache, &dated, encoded, length);
      return STORE_OK;
    }
    if (previous_revision >= record->revision ||
        previous_status != SESSION_ACTIVE)
      return STORE_IO_ERROR;
  }

  status = store_ensure_directory(folder);
  if (status != STORE_OK) return status;
  status = store_atomic_write(path, backup, encoded, length);
  if (status != STORE_OK) return status;

  unsigned char verified[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t verified_length = 0;
  status = store_read_file(path, verified, sizeof(verified),
                           &verified_length);
  if (status != STORE_OK) return status;
  if (verified_length != length ||
      memcmp(verified, encoded, length) != 0)
    return STORE_CORRUPT;
  hot_cache_store(cache, &dated, encoded, length);
  return STORE_OK;
}

StoreStatus game_library_save_runtime(const char *root,
                                      const GameLibraryRecord *record,
                                      GameLibraryHotCache *cache) {
  time_t now = time(NULL);
  return game_library_save_runtime_at(root, record, cache,
                                       now > 0 ? (uint64_t)now : 0);
}

/* Explicit previous-copy recovery must not destroy the existing primary,
   including incompatible/corrupt bytes that might matter to a future reader.
   Preserve it as opaque data before replacing it with validated backup bytes. */
static StoreStatus preserve_pre_recovery(const char *root, GameAttemptId id,
                                         const char *primary) {
  unsigned char original[SUDOKURA_STORE_IO_MAX_FILE_SIZE];
  size_t count = 0;
  StoreStatus status =
      store_read_file(primary, original, sizeof(original), &count);
  if (status == STORE_NOT_FOUND) return STORE_OK;
  if (status != STORE_OK) return status;
  /* A truncated-to-zero primary is still evidence: preserve it exactly. */
  for (unsigned index = 0; index < 16u; ++index) {
    char suffix[32];
    int n = index == 0
          ? snprintf(suffix, sizeof(suffix), ".dat.pre-recovery")
          : snprintf(suffix, sizeof(suffix), ".dat.pre-recovery.%u", index);
    if (n <= 0 || (size_t)n >= sizeof(suffix)) return STORE_IO_ERROR;
    char copy[SUDOKURA_STORE_PATH_CAPACITY];
    if (!game_path(copy, root, id, suffix)) return STORE_IO_ERROR;

    unsigned char existing[SUDOKURA_STORE_IO_MAX_FILE_SIZE];
    size_t existing_count = 0;
    status = store_read_file(copy, existing, sizeof(existing),
                             &existing_count);
    if (status == STORE_OK) {
      if (existing_count == count && memcmp(existing, original, count) == 0)
        return STORE_OK;
      continue; /* Never overwrite a different immutable recovery copy. */
    }
    if (status != STORE_NOT_FOUND) return status;
    status = store_copy_once_allow_empty(primary, copy);
    if (status != STORE_OK) return status;
    status = store_read_file(copy, existing, sizeof(existing),
                             &existing_count);
    if (status != STORE_OK) return status;
    if (existing_count != count || memcmp(existing, original, count) != 0)
      return STORE_CORRUPT;
    return STORE_OK;
  }
  return STORE_IO_ERROR; /* Preserve old primary when all slots are occupied. */
}

StoreStatus game_library_recover_previous(const char *root, GameAttemptId id,
                                          GameLibraryRecord *record) {
  if (!record || !root || !game_attempt_id_valid(id))
    return STORE_IO_ERROR;
  StoreStatus receipt = game_library_receipt_status(root, id);
  if (receipt != STORE_NOT_FOUND)
    return receipt == STORE_OK ? STORE_NOT_FOUND : receipt;

  GameLibraryRecord previous;
  StoreStatus status = load_from_suffix(root, id, ".dat.bak", &previous);
  if (status != STORE_OK) return status;
  unsigned char encoded[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t length = 0;
  if (!game_library_encode(&previous, encoded, &length))
    return STORE_IO_ERROR;
  char primary[SUDOKURA_STORE_PATH_CAPACITY];
  if (!game_path(primary, root, id, ".dat")) return STORE_IO_ERROR;
  status = preserve_pre_recovery(root, id, primary);
  if (status != STORE_OK) return status;
  status = store_atomic_write(primary, NULL, encoded, length);
  if (status != STORE_OK) return status;
  GameLibraryRecord verified;
  status = game_library_load(root, id, &verified);
  if (status != STORE_OK) return status;
  unsigned char readback[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t readback_length = 0;
  if (!game_library_encode(&verified, readback, &readback_length) ||
      readback_length != length ||
      memcmp(encoded, readback, length) != 0)
    return STORE_CORRUPT;
  *record = verified;
  return STORE_OK;
}

StoreStatus game_library_delete_with_receipt(const char *root,
                                             GameAttemptId id,
                                             bool *receipt_committed) {
  if (receipt_committed) *receipt_committed = false;
  if (!root || !game_attempt_id_valid(id)) return STORE_IO_ERROR;
  char receipt_path[SUDOKURA_STORE_PATH_CAPACITY];
  char primary[SUDOKURA_STORE_PATH_CAPACITY];
  char backup[SUDOKURA_STORE_PATH_CAPACITY];
  char folder[SUDOKURA_STORE_PATH_CAPACITY];
  if (!game_path(receipt_path, root, id, ".deleted") ||
      !game_path(primary, root, id, ".dat") ||
      !game_path(backup, root, id, ".dat.bak") ||
      !games_directory(folder, root))
    return STORE_IO_ERROR;
  StoreStatus status = game_library_receipt_status(root, id);
  if (status != STORE_NOT_FOUND && status != STORE_OK) return status;
  if (status == STORE_OK && receipt_committed) *receipt_committed = true;
  if (status == STORE_NOT_FOUND) {
    unsigned char data[RECEIPT_BYTES];
    memcpy(data, receipt_magic, sizeof(receipt_magic));
    write16(data + 8, 1);
    write32(data + 10, 16);
    write64(data + 18, id.high);
    write64(data + 26, id.low);
    write32(data + 14, checksum(data + 18, 16));
    status = store_ensure_directory(folder);
    if (status != STORE_OK) return status;
    status = store_atomic_write(receipt_path, NULL, data, sizeof(data));
    if (status != STORE_OK) return status;
    status = game_library_receipt_status(root, id);
    if (status != STORE_OK) return status;
    if (receipt_committed) *receipt_committed = true;
  }
  /* The receipt is durable before either copy may be removed. */
  if (store_file_exists(primary) && !store_remove_file(primary))
    return STORE_IO_ERROR;
  if (store_file_exists(backup) && !store_remove_file(backup))
    return STORE_IO_ERROR;
  return STORE_OK;
}

StoreStatus game_library_delete(const char *root, GameAttemptId id) {
  return game_library_delete_with_receipt(root, id, NULL);
}

bool game_library_prepare_replay(GameLibraryRecord *out,
                                 const GameLibraryRecord *source,
                                 GameAttemptId fresh_id) {
  if (!out || !source || out == source ||
      source->kind != GAME_RECORD_EXACT ||
      source->result.status == SESSION_ACTIVE ||
      !game_attempt_id_valid(fresh_id) ||
      game_attempt_id_equal(fresh_id, source->id) ||
      !game_library_record_validate(source))
    return false;
  ProfileSession fresh = source->exact;
  Game *board = &fresh.session.game;
  game_restart(board);
  fresh.session.status = SESSION_ACTIVE;
  fresh.session.elapsed_ms = 0;
  fresh.session.mistakes = 0;
  fresh.session.strikes = 0;
  fresh.session.selected_row = 4;
  fresh.session.selected_column = 4;
  fresh.session.notes_mode = false;
  fresh.session.manual_paused = false;
  fresh.session.is_daily = false;
  fresh.session.daily_year = 0;
  fresh.session.daily_month = 0;
  fresh.session.daily_day = 0;
  fresh.undo_count = 0;
  fresh.redo_count = 0;
  memset(fresh.undo, 0, sizeof(fresh.undo));
  memset(fresh.redo, 0, sizeof(fresh.redo));
  GameLibraryRecord candidate;
  if (!game_library_make_exact(&candidate, fresh_id, 1, &fresh, true))
    return false;
  candidate.practice = true;
  candidate.parent = source->id;
  candidate.result.practice = true;
  candidate.original_daily = source->original_daily;
  candidate.original_daily_year = source->original_daily_year;
  candidate.original_daily_month = source->original_daily_month;
  candidate.original_daily_day = source->original_daily_day;
  if (!game_library_record_validate(&candidate)) return false;
  *out = candidate;
  return true;
}

enum { REPLAY_INTENT_BYTES = 48u, REPLAY_INTENT_CRC_BYTES = 44u };
static const unsigned char replay_intent_magic[8] = {
    'S','U','D','O','R','P','L','Y'};

static bool replay_intent_path(
    char path[SUDOKURA_STORE_PATH_CAPACITY], const char *root) {
  char directory[SUDOKURA_STORE_PATH_CAPACITY];
  if (!games_directory(directory, root)) return false;
  const char suffix[] = "/replay.intent";
  size_t count = strlen(directory);
  if (count > SUDOKURA_STORE_PATH_CAPACITY - sizeof(suffix))
    return false;
  memcpy(path, directory, count);
  memcpy(path + count, suffix, sizeof(suffix));
  return true;
}

static bool replay_intent_valid(const GameReplayIntent *intent) {
  return intent && game_attempt_id_valid(intent->source_id) &&
         game_attempt_id_valid(intent->attempt_id) &&
         !game_attempt_id_equal(intent->source_id, intent->attempt_id) &&
         (intent->state == GAME_REPLAY_PREPARED ||
          intent->state == GAME_REPLAY_COMMITTED ||
          intent->state == GAME_REPLAY_ABANDONED);
}

static uint32_t replay_intent_crc(const unsigned char bytes[REPLAY_INTENT_BYTES]) {
  unsigned char covered[REPLAY_INTENT_CRC_BYTES];
  memcpy(covered, bytes, 12u);
  memcpy(covered + 12u, bytes + 16u, 32u);
  return checksum(covered, sizeof(covered));
}

StoreStatus game_library_replay_intent_load(const char *root,
                                            GameReplayIntent *intent) {
  if (!root || !intent) return STORE_IO_ERROR;
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!replay_intent_path(path, root)) return STORE_IO_ERROR;
  unsigned char bytes[REPLAY_INTENT_BYTES];
  size_t count = 0;
  StoreStatus status = store_read_file(path, bytes, sizeof(bytes), &count);
  if (status == STORE_CORRUPT) {
    /* The current fixed bound cannot turn a future valid version into
       a corrupt v1 record eligible for replacement. */
    unsigned char prefix[10];
    size_t got = 0;
    status = store_read_prefix(path, prefix, sizeof(prefix), &got);
    if (status != STORE_OK) return status;
    if (got >= sizeof(prefix) &&
        memcmp(prefix, replay_intent_magic, 8u) == 0 &&
        read16(prefix + 8u) != 1u)
      return STORE_INCOMPATIBLE;
    return STORE_CORRUPT;
  }
  if (status != STORE_OK) return status;
  if (count >= 10u &&
      memcmp(bytes, replay_intent_magic, 8u) == 0 &&
      read16(bytes + 8u) != 1u)
    return STORE_INCOMPATIBLE;
  if (count != sizeof(bytes) ||
      memcmp(bytes, replay_intent_magic, 8u) != 0 ||
      read16(bytes + 8u) != 1u ||
      bytes[11] != 0u ||
      read32(bytes + 12u) != replay_intent_crc(bytes))
    return STORE_CORRUPT;
  GameReplayIntent decoded = {
      .source_id = {read64(bytes + 16u), read64(bytes + 24u)},
      .attempt_id = {read64(bytes + 32u), read64(bytes + 40u)},
      .state = (GameReplayIntentState)bytes[10]};
  if (!replay_intent_valid(&decoded)) return STORE_CORRUPT;
  *intent = decoded;
  return STORE_OK;
}

StoreStatus game_library_replay_intent_write(const char *root,
                                             const GameReplayIntent *intent) {
  if (!root || !replay_intent_valid(intent)) return STORE_IO_ERROR;
  GameReplayIntent old;
  StoreStatus status = game_library_replay_intent_load(root, &old);
  if (status == STORE_OK) {
    bool same = game_attempt_id_equal(old.source_id, intent->source_id) &&
                game_attempt_id_equal(old.attempt_id, intent->attempt_id);
    /* Retry an identical marker through atomic write, not only a read:
       a preceding parent-directory sync may have failed after rename. */
    if (same && old.state == intent->state) {
      /* Valid idempotent re-publication; no state transition. */
    } else if (old.state == GAME_REPLAY_PREPARED) {
      if (!same || intent->state == GAME_REPLAY_PREPARED)
        return STORE_IO_ERROR;
    } else if (intent->state != GAME_REPLAY_PREPARED ||
               game_attempt_id_equal(old.attempt_id, intent->attempt_id)) {
      return STORE_IO_ERROR;
    }
  } else if (status != STORE_NOT_FOUND) {
    return status; /* Never overwrite corrupt or future-version intent data. */
  } else if (intent->state != GAME_REPLAY_PREPARED) {
    return STORE_IO_ERROR;
  }
  char directory[SUDOKURA_STORE_PATH_CAPACITY];
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!games_directory(directory, root) ||
      !replay_intent_path(path, root))
    return STORE_IO_ERROR;
  status = store_ensure_directory(directory);
  if (status != STORE_OK) return status;

  unsigned char bytes[REPLAY_INTENT_BYTES] = {0};
  memcpy(bytes, replay_intent_magic, sizeof(replay_intent_magic));
  write16(bytes + 8u, 1u);
  bytes[10] = (unsigned char)intent->state;
  write64(bytes + 16u, intent->source_id.high);
  write64(bytes + 24u, intent->source_id.low);
  write64(bytes + 32u, intent->attempt_id.high);
  write64(bytes + 40u, intent->attempt_id.low);
  write32(bytes + 12u, replay_intent_crc(bytes));
  status = store_atomic_write(path, NULL, bytes, sizeof(bytes));
  if (status != STORE_OK) return status;
  GameReplayIntent verified;
  status = game_library_replay_intent_load(root, &verified);
  if (status != STORE_OK) return status;
  return game_attempt_id_equal(verified.source_id, intent->source_id) &&
         game_attempt_id_equal(verified.attempt_id, intent->attempt_id) &&
         verified.state == intent->state ? STORE_OK : STORE_CORRUPT;
}

bool game_library_replay_pending_matches(const GameLibraryRecord *source,
                                         const GameLibraryRecord *candidate) {
  if (!source || !candidate ||
      !game_library_record_validate(source) ||
      !game_library_record_validate(candidate) ||
      candidate->kind != GAME_RECORD_EXACT ||
      candidate->revision != 1u || !candidate->retained ||
      !candidate->practice ||
      candidate->result.status != SESSION_ACTIVE ||
      !game_attempt_id_equal(candidate->parent, source->id))
    return false;
  GameLibraryRecord expected;
  if (!game_library_prepare_replay(&expected, source, candidate->id))
    return false;
  if (expected.original_daily != candidate->original_daily ||
      expected.original_daily_year != candidate->original_daily_year ||
      expected.original_daily_month != candidate->original_daily_month ||
      expected.original_daily_day != candidate->original_daily_day ||
      memcmp(expected.original, candidate->original,
             sizeof(expected.original)) != 0 ||
      memcmp(expected.solution, candidate->solution,
             sizeof(expected.solution)) != 0)
    return false;
  unsigned char left[4096], right[4096];
  size_t left_size = 0, right_size = 0;
  return profile_session_encode_blob(&expected.exact, left, sizeof(left),
                                     &left_size) &&
         profile_session_encode_blob(&candidate->exact, right, sizeof(right),
                                     &right_size) &&
         left_size == right_size &&
         memcmp(left, right, left_size) == 0;
}

StoreStatus game_library_replay_linked_selection(
    const GameReplayIntent *intent, GameAttemptId selected_source,
    const ProfileSlot *hot, const GameLibraryRecord *record,
    bool *resume_selected) {
  if (!intent || !hot || !record || !resume_selected)
    return STORE_IO_ERROR;
  if (intent->state != GAME_REPLAY_PREPARED ||
      !game_attempt_id_valid(selected_source) ||
      !hot->present || !hot->retained ||
      !game_attempt_id_equal(hot->attempt_id, intent->attempt_id) ||
      record->kind != GAME_RECORD_EXACT || !record->practice ||
      !game_attempt_id_equal(record->parent, intent->source_id) ||
      !game_library_record_validate(record))
    return STORE_CORRUPT;

  bool prefer_record = false;
  StoreStatus status = game_library_resolve_hot_record(
      hot, record, &prefer_record);
  if (status != STORE_OK) return status;
  /* A newer record needs its own durable Load reconciliation; never
     finalize the marker and resume an older hot snapshot instead. */
  if (prefer_record) return STORE_IO_ERROR;

  *resume_selected = game_attempt_id_equal(
      intent->source_id, selected_source);
  return STORE_OK;
}

bool game_library_forget_attempt(ProfileData *out, const ProfileData *source,
                                 GameAttemptId id) {
  if (!out || !source || out == source ||
      !game_attempt_id_valid(id) || !profile_validate(source) ||
      source->result_count > SUDOKURA_RESULT_LIMIT)
    return false;
  ProfileData candidate = *source;
  if (candidate.normal.present &&
      game_attempt_id_equal(candidate.normal.attempt_id, id))
    memset(&candidate.normal, 0, sizeof(candidate.normal));
  if (candidate.daily.present &&
      game_attempt_id_equal(candidate.daily.attempt_id, id))
    memset(&candidate.daily, 0, sizeof(candidate.daily));
  uint16_t write_index = 0;
  for (uint16_t i = 0; i < source->result_count; ++i) {
    if (game_attempt_id_equal(source->results[i].attempt_id, id)) continue;
    candidate.results[write_index++] = source->results[i];
  }
  for (uint16_t i = write_index; i < source->result_count; ++i)
    memset(&candidate.results[i], 0, sizeof(candidate.results[i]));
  candidate.result_count = write_index;
  if (!profile_validate(&candidate)) return false;
  *out = candidate;
  return true;
}

typedef struct {
  const char *root;
  GameLibraryScanFn callback;
  void *context;
} ScanContext;

static bool visit_game_name(const char *name, void *context) {
  ScanContext *scan = (ScanContext *)context;
  size_t length = strlen(name);
  bool backup = length == 40 && strcmp(name + 32, ".dat.bak") == 0;
  bool primary = length == 36 && strcmp(name + 32, ".dat") == 0;
  if (!backup && !primary) return true;

  char hex[33];
  memcpy(hex, name, 32);
  hex[32] = '\0';
  GameAttemptId id;
  if (!game_attempt_id_parse(hex, &id)) return true;
  char primary_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!game_path(primary_path, scan->root, id, ".dat"))
    return false;
  if (backup && store_file_exists(primary_path)) return true;

  StoreStatus receipt = game_library_receipt_status(scan->root, id);
  if (receipt == STORE_OK) return true;
  GameLibraryEntry item;
  memset(&item, 0, sizeof(item));
  item.id = id;
  item.only_backup = backup;
  item.previous_status = STORE_NOT_FOUND;
  if (receipt != STORE_NOT_FOUND) {
    item.status = receipt;
  } else {
    char file[SUDOKURA_STORE_PATH_CAPACITY];
    if (!game_path(file, scan->root, id,
                   backup ? ".dat.bak" : ".dat")) return false;
    GameLibraryEntry decoded;
    item.status = inspect_record_file(file, id, &decoded);
    if (item.status == STORE_OK) {
      item.kind = decoded.kind;
      item.result = decoded.result;
      item.revision = decoded.revision;
      item.created_utc = decoded.created_utc;
      item.updated_utc = decoded.updated_utc;
      item.finished_utc = decoded.finished_utc;
    }
    if (backup) {
      item.previous_status = item.status;
      if (item.status == STORE_OK) item.status = STORE_NOT_FOUND;
    } else {
      char previous[SUDOKURA_STORE_PATH_CAPACITY];
      if (!game_path(previous, scan->root, id, ".dat.bak"))
        return false;
      if (store_file_exists(previous)) {
        GameLibraryEntry previous_metadata;
        item.previous_status =
            inspect_record_file(previous, id, &previous_metadata);
        /* Expose validated earlier metadata without implying the primary
           is usable. The actual board still needs explicit recovery. */
        if (item.status != STORE_OK &&
            item.previous_status == STORE_OK) {
          item.kind = previous_metadata.kind;
          item.result = previous_metadata.result;
          item.revision = previous_metadata.revision;
          item.created_utc = previous_metadata.created_utc;
          item.updated_utc = previous_metadata.updated_utc;
          item.finished_utc = previous_metadata.finished_utc;
        }
      }
    }
  }
  return scan->callback(&item, scan->context);
}

bool game_library_entry_recoverable(const GameLibraryEntry *entry) {
  return entry && game_attempt_id_valid(entry->id) &&
         entry->previous_status == STORE_OK &&
         (entry->status == STORE_CORRUPT ||
          (entry->only_backup && entry->status == STORE_NOT_FOUND));
}

/* Metadata-only Daily lookup: the date and puzzle class are immutable.
   Practice records carry provenance but never count as original Daily. */
bool game_library_is_original_daily_on(const GameLibraryEntry *entry,
                                       int year, int month, int day) {
  if (!entry || year < 1 || year > UINT16_MAX ||
      month < 1 || month > 12 || day < 1 || day > 31)
    return false;
  if (entry->status != STORE_OK &&
      entry->previous_status != STORE_OK)
    return false;
  const ProfileResult *result = &entry->result;
  return (entry->kind == GAME_RECORD_EXACT ||
          entry->kind == GAME_RECORD_LEGACY_SUMMARY) &&
         result->is_daily && !result->practice &&
         result->mode == MODE_CLASSIC &&
         result->difficulty == DIFFICULTY_MEDIUM &&
         result->daily_year == (uint16_t)year &&
         result->daily_month == (uint8_t)month &&
         result->daily_day == (uint8_t)day;
}

void game_library_daily_lookup_consider(GameDailyLookup *lookup,
                                        const GameLibraryEntry *entry,
                                        int year, int month, int day) {
  if (!lookup || !entry) return;
  if (game_library_is_original_daily_on(entry, year, month, day)) {
    if (lookup->matching_attempts == 0) lookup->match = *entry;
    if (lookup->matching_attempts < 2u) ++lookup->matching_attempts;
  }
  /* An unreadable primary might contain today's original. A good
     backup-only entry is distinct: its validated metadata is known. */
  if (entry->status != STORE_OK &&
      !(entry->previous_status == STORE_OK &&
        game_library_is_original_daily_on(entry, year, month, day)))
    lookup->uncertain = true;
}

StoreStatus game_library_scan(const char *root, GameLibraryScanFn callback,
                              void *context) {
  if (!root || !callback) return STORE_IO_ERROR;
  char folder[SUDOKURA_STORE_PATH_CAPACITY];
  if (!games_directory(folder, root)) return STORE_IO_ERROR;
  ScanContext scan = {root, callback, context};
  StoreStatus status = store_list_directory(folder, visit_game_name, &scan);
  return status == STORE_NOT_FOUND ? STORE_OK : status;
}

StoreStatus game_library_scan_begin(const char *root, GameLibraryScanCursor *scan) {
  if (!root || !root[0] || !scan || scan->directory.state) return STORE_IO_ERROR;
  size_t len = strlen(root);
  if (len >= sizeof(scan->root)) return STORE_IO_ERROR;
  char folder[SUDOKURA_STORE_PATH_CAPACITY];
  if (!games_directory(folder, root)) return STORE_IO_ERROR;
  memcpy(scan->root, root, len + 1u);
  scan->finished = false;
  StoreStatus status = store_directory_open(folder, &scan->directory);
  if (status == STORE_NOT_FOUND) {
    scan->finished = true;
    return STORE_OK;
  }
  if (status != STORE_OK) scan->finished = true;
  return status;
}

StoreStatus game_library_scan_step(GameLibraryScanCursor *scan,
                                   size_t name_budget,
                                   GameLibraryScanFn callback, void *context,
                                   bool *finished) {
  if (!scan || !callback || !finished || !name_budget) return STORE_IO_ERROR;
  *finished = scan->finished;
  if (scan->finished) return STORE_OK;
  ScanContext scan_context = {scan->root, callback, context};
  for (size_t processed = 0; processed < name_budget; ++processed) {
    char name[SUDOKURA_STORE_PATH_CAPACITY];
    StoreStatus status = store_directory_next(&scan->directory, name);
    if (status == STORE_NOT_FOUND) {
      scan->finished = true;
      *finished = true;
      store_directory_close(&scan->directory);
      return STORE_OK;
    }
    if (status != STORE_OK || !visit_game_name(name, &scan_context)) {
      scan->finished = true;
      *finished = true;
      store_directory_close(&scan->directory);
      return status == STORE_OK ? STORE_CANCELLED : status;
    }
  }
  return STORE_OK;
}

void game_library_scan_close(GameLibraryScanCursor *scan) {
  if (!scan) return;
  store_directory_close(&scan->directory);
  scan->finished = true;
}
