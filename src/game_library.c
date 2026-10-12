#include "game_library.h"

#include "seed.h"
#include "store_io.h"

#include <stdio.h>
#include <string.h>

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
  if (status != STORE_OK) return status;
  if (count != sizeof(bytes) ||
      memcmp(bytes, receipt_magic, sizeof(receipt_magic)) != 0)
    return STORE_CORRUPT;
  if (read16(bytes + 8) != 1) return STORE_INCOMPATIBLE;
  if (read32(bytes + 10) != 16 ||
      read32(bytes + 14) != checksum(bytes + 18, 16) ||
      read64(bytes + 18) != id.high ||
      read64(bytes + 26) != id.low)
    return STORE_CORRUPT;
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
  StoreStatus status = store_read_file(path, bytes, sizeof(bytes), &count);
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

bool game_library_new_id(const char *root, const GameAttemptId *reserved,
                         size_t reserved_count, GameAttemptId *out) {
  if (!root || !out || (reserved_count && !reserved)) return false;
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    GameAttemptId value;
    if (!seed_system_random_u64(&value.high))
      value.high = seed_random_u64();
    if (!seed_system_random_u64(&value.low))
      value.low = seed_random_u64();
    if (!game_attempt_id_valid(value)) continue;
    bool collision = false;
    for (size_t i = 0; i < reserved_count; ++i)
      if (game_attempt_id_equal(reserved[i], value)) collision = true;
    if (collision) continue;
    const char *suffixes[] = {".dat", ".dat.bak", ".deleted"};
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
    /* A backup-only record must be explicitly recovered, never overwritten. */
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

StoreStatus game_library_delete(const char *root, GameAttemptId id) {
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
  }
  /* The receipt is durable before either copy may be removed. */
  if (store_file_exists(primary) && !store_remove_file(primary))
    return STORE_IO_ERROR;
  if (store_file_exists(backup) && !store_remove_file(backup))
    return STORE_IO_ERROR;
  return STORE_OK;
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
  if (receipt != STORE_NOT_FOUND) {
    item.status = receipt;
  } else {
    char file[SUDOKURA_STORE_PATH_CAPACITY];
    if (!game_path(file, scan->root, id,
                   backup ? ".dat.bak" : ".dat")) return false;
    unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES];
    size_t size = 0;
    item.status = store_read_file(file, bytes, sizeof(bytes), &size);
    if (item.status == STORE_OK) {
      GameLibraryEntry decoded;
      item.status = game_library_peek(bytes, size, &decoded);
      if (item.status == STORE_OK) {
        if (!game_attempt_id_equal(decoded.id, id))
          item.status = STORE_CORRUPT;
        else {
          item.kind = decoded.kind;
          item.result = decoded.result;
          item.revision = decoded.revision;
          if (backup) item.status = STORE_NOT_FOUND;
        }
      }
    }
  }
  return scan->callback(&item, scan->context);
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
