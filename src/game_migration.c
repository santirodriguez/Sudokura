#include "game_migration.h"

#include "game_library.h"
#include "store_io.h"

#include <stdio.h>
#include <string.h>

enum { SOURCE_MAX = 16384, SOURCE_SUFFIX_LIMIT = 16 };

static bool join(char out[SUDOKURA_STORE_PATH_CAPACITY],
                 const char *root, const char *leaf) {
  if (!out || !root || !root[0] || !leaf) return false;
  size_t size = strlen(root);
  int n = snprintf(out, SUDOKURA_STORE_PATH_CAPACITY, "%s%s%s", root,
                   root[size - 1] == '/' || root[size - 1] == '\\' ? "" : "/",
                   leaf);
  return n >= 0 && (size_t)n < SUDOKURA_STORE_PATH_CAPACITY;
}

static bool source_path(char out[SUDOKURA_STORE_PATH_CAPACITY],
                        const char *root, uint8_t suffix) {
  char name[80];
  if (suffix >= SOURCE_SUFFIX_LIMIT) return false;
  int n = suffix == 0
              ? snprintf(name, sizeof(name), "profile.dat.pre-v1.3.7.bak")
              : snprintf(name, sizeof(name), "profile.dat.pre-v1.3.7.bak.%u",
                         (unsigned)suffix);
  return n > 0 && (size_t)n < sizeof(name) && join(out, root, name);
}

static uint32_t crc32_bytes(const unsigned char *bytes, size_t count) {
  uint32_t crc = UINT32_C(0xffffffff);
  for (size_t i = 0; i < count; ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0u - (crc & 1u)));
  }
  return ~crc;
}

static StoreStatus read_source(const char *path,
                               unsigned char bytes[SOURCE_MAX],
                               size_t *count) {
  if (!path || !bytes || !count) return STORE_IO_ERROR;
  return store_read_file(path, bytes, SOURCE_MAX, count);
}

static bool any_entry(const char *name, void *context) {
  (void)name;
  bool *found = (bool *)context;
  *found = true;
  return false;
}

static bool profile_evidence_entry(const char *name, void *context) {
  /* Do not initialize defaults over orphan recovery copies or interrupted
     profile writes. profile.lock by itself is not retained user data. */
  static const char prefix[] = "profile.dat.";
  if (strncmp(name, prefix, sizeof(prefix) - 1u) != 0) return true;
  bool *found = (bool *)context;
  *found = true;
  return false;
}

bool game_migration_has_artifacts(const char *root) {
  if (!root) return true;
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!join(path, root, "profile.dat.v1.3.7-import.dat"))
    return true;
  if (store_file_exists(path)) return true;
  for (uint8_t i = 0; i < SOURCE_SUFFIX_LIMIT; ++i) {
    if (!source_path(path, root, i)) return true;
    if (store_file_exists(path)) return true;
  }
  bool found = false;
  StoreStatus status = store_list_directory(root, profile_evidence_entry,
                                           &found);
  if (found || (status != STORE_OK && status != STORE_NOT_FOUND))
    return true;
  if (!join(path, root, "games")) return true;
  found = false;
  status = store_list_directory(path, any_entry, &found);
  return found || (status != STORE_OK && status != STORE_NOT_FOUND);
}

static bool same_event(const ProfileResult *a, const ProfileSlot *slot) {
  if (!a || !slot || !slot->present) return false;
  const SessionState *s = &slot->value.session;
  return s->status != SESSION_ACTIVE &&
         a->seed == s->game.seed &&
         a->generator_revision == s->game.generator_revision &&
         a->difficulty == s->game.difficulty && a->mode == s->mode &&
         a->status == s->status && a->assisted == slot->value.assisted &&
         a->is_daily == s->is_daily && a->elapsed_ms == s->elapsed_ms &&
         a->mistakes == (uint32_t)s->mistakes &&
         a->strikes == (uint32_t)s->strikes &&
         a->daily_year == (uint16_t)s->daily_year &&
         a->daily_month == (uint8_t)s->daily_month &&
         a->daily_day == (uint8_t)s->daily_day;
}

static bool contains_id(const GameAttemptId *ids, size_t count,
                        GameAttemptId id) {
  for (size_t i = 0; i < count; ++i)
    if (game_attempt_id_equal(ids[i], id)) return true;
  return false;
}

static bool allocate_ids(const char *root, ProfileData *pending) {
  GameAttemptId ids[2 + SUDOKURA_RESULT_LIMIT];
  size_t count = 0;
  ProfileSlot *slots[2] = {&pending->normal, &pending->daily};
  for (unsigned index = 0; index < 2; ++index)
    if (slots[index]->present &&
        game_attempt_id_valid(slots[index]->attempt_id) &&
        !contains_id(ids, count, slots[index]->attempt_id))
      ids[count++] = slots[index]->attempt_id;
  for (uint16_t i = 0; i < pending->result_count; ++i)
    if (game_attempt_id_valid(pending->results[i].attempt_id) &&
        !contains_id(ids, count, pending->results[i].attempt_id))
      ids[count++] = pending->results[i].attempt_id;

  for (unsigned index = 0; index < 2; ++index) {
    ProfileSlot *slot = slots[index];
    if (!slot->present) continue;
    if (!game_attempt_id_valid(slot->attempt_id)) {
      if (!game_library_new_id(root, ids, count, &slot->attempt_id))
        return false;
      ids[count++] = slot->attempt_id;
      slot->revision = 1;
    }
    slot->retained = true;
  }
  for (uint16_t i = 0; i < pending->result_count; ++i) {
    ProfileResult *r = &pending->results[i];
    if (game_attempt_id_valid(r->attempt_id)) continue;
    for (unsigned index = 0; index < 2; ++index) {
      if (r->status == SESSION_WON && same_event(r, slots[index])) {
        /* Exactly one legacy result may describe the terminal slot. */
        bool already_linked = false;
        for (uint16_t j = 0; j < i; ++j)
          if (game_attempt_id_equal(pending->results[j].attempt_id,
                                    slots[index]->attempt_id)) {
            already_linked = true;
            break;
          }
        if (!already_linked) {
          r->attempt_id = slots[index]->attempt_id;
          break;
        }
      }
    }
    if (!game_attempt_id_valid(r->attempt_id)) {
      if (!game_library_new_id(root, ids, count, &r->attempt_id))
        return false;
      ids[count++] = r->attempt_id;
    }
  }
  return profile_validate(pending);
}

static StoreStatus ensure_source(const char *root, const char *profile_path,
                                 const unsigned char *bytes, size_t count,
                                 uint8_t *suffix_out) {
  if (!root || !profile_path || !bytes || !suffix_out)
    return STORE_IO_ERROR;
  for (uint8_t suffix = 0; suffix < SOURCE_SUFFIX_LIMIT; ++suffix) {
    char path[SUDOKURA_STORE_PATH_CAPACITY];
    if (!source_path(path, root, suffix)) return STORE_IO_ERROR;
    unsigned char existing[SOURCE_MAX];
    size_t length = 0;
    StoreStatus status = read_source(path, existing, &length);
    if (status == STORE_OK) {
      if (length == count && memcmp(existing, bytes, count) == 0) {
        *suffix_out = suffix;
        return STORE_OK;
      }
      continue; /* Collision: keep the different immutable copy untouched. */
    }
    if (status != STORE_NOT_FOUND) return status;
    status = store_copy_once(profile_path, path);
    if (status != STORE_OK) return status;
    status = read_source(path, existing, &length);
    if (status != STORE_OK) return status;
    if (length != count || memcmp(existing, bytes, count) != 0)
      return STORE_CORRUPT;
    *suffix_out = suffix;
    return STORE_OK;
  }
  return STORE_IO_ERROR;
}

static StoreStatus verify_source(const char *root,
                                 const ProfileData *intent,
                                 unsigned char bytes[SOURCE_MAX],
                                 size_t *size) {
  char path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!source_path(path, root, intent->migration_source_suffix))
    return STORE_IO_ERROR;
  StoreStatus status = read_source(path, bytes, size);
  if (status != STORE_OK) return status;
  if (*size != intent->migration_source_length ||
      crc32_bytes(bytes, *size) != intent->migration_source_crc)
    return STORE_CORRUPT;
  ProfileData checked;
  status = profile_load_file(path, &checked);
  return status;
}

static bool same_puzzle(const GameLibraryRecord *saved,
                        const GameLibraryRecord *source) {
  const ProfileResult *a = &saved->result;
  const ProfileResult *b = &source->result;
  return saved->kind == GAME_RECORD_EXACT &&
         a->seed == b->seed &&
         a->generator_revision == b->generator_revision &&
         a->difficulty == b->difficulty && a->mode == b->mode &&
         a->is_daily == b->is_daily &&
         a->daily_year == b->daily_year &&
         a->daily_month == b->daily_month &&
         a->daily_day == b->daily_day &&
         memcmp(saved->original, source->original, 81) == 0 &&
         memcmp(saved->solution, source->solution, 81) == 0;
}

static StoreStatus migrate_slot(const char *root, ProfileSlot *slot) {
  if (!slot->present || !game_attempt_id_valid(slot->attempt_id))
    return STORE_OK;
  StoreStatus receipt = game_library_receipt_status(root, slot->attempt_id);
  if (receipt == STORE_OK) {
    memset(slot, 0, sizeof(*slot));
    return STORE_OK;
  }
  if (receipt != STORE_NOT_FOUND) return receipt;

  GameLibraryRecord expected;
  if (!game_library_make_exact(&expected, slot->attempt_id, slot->revision,
                               &slot->value, true))
    return STORE_CORRUPT;
  GameLibraryRecord on_disk;
  StoreStatus status = game_library_load(root, slot->attempt_id, &on_disk);
  if (status == STORE_OK) {
    if (!same_puzzle(&on_disk, &expected)) return STORE_CORRUPT;
    if (on_disk.revision > expected.revision) {
      slot->value = on_disk.exact;
      slot->revision = on_disk.revision;
      slot->retained = true;
      return STORE_OK;
    }
  } else if (status != STORE_NOT_FOUND) return status;

  status = game_library_save(root, &expected);
  if (status != STORE_OK) return status;
  return game_library_load(root, expected.id, &on_disk);
}

static bool same_result_metadata(const ProfileResult *a,
                                 const ProfileResult *b);

static StoreStatus migrate_results(const char *root, ProfileData *pending) {
  for (uint16_t i = 0; i < pending->result_count;) {
    ProfileResult *r = &pending->results[i];
    StoreStatus receipt = game_library_receipt_status(root, r->attempt_id);
    if (receipt == STORE_OK) {
      memmove(r, r + 1,
              (pending->result_count - i - 1u) * sizeof(*r));
      --pending->result_count;
      continue;
    }
    if (receipt != STORE_NOT_FOUND) return receipt;
    if (r->status == SESSION_WON) {
      bool attached = (pending->normal.present &&
                       game_attempt_id_equal(
                           pending->normal.attempt_id, r->attempt_id)) ||
                      (pending->daily.present &&
                       game_attempt_id_equal(
                           pending->daily.attempt_id, r->attempt_id));
      if (!attached) {
        GameLibraryRecord expected;
        if (!game_library_make_legacy(&expected, r->attempt_id, r))
          return STORE_CORRUPT;
        GameLibraryRecord existing;
        StoreStatus status = game_library_load(root, r->attempt_id, &existing);
        if (status == STORE_OK) {
          if (!same_result_metadata(&existing.result, &expected.result) ||
              existing.practice || !existing.retained)
            return STORE_CORRUPT;
        } else if (status == STORE_NOT_FOUND) {
          status = game_library_save(root, &expected);
          if (status != STORE_OK) return status;
          status = game_library_load(root, r->attempt_id, &existing);
          if (status != STORE_OK) return status;
        } else return status;
      }
    }
    ++i;
  }
  return STORE_OK;
}

static bool needs_upgrade(const ProfileData *profile) {
  if (profile->codec_version == SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY ||
      profile->migration_pending) return true;
  if ((profile->normal.present &&
       !game_attempt_id_valid(profile->normal.attempt_id)) ||
      (profile->daily.present &&
       !game_attempt_id_valid(profile->daily.attempt_id)))
    return true;
  for (uint16_t i = 0; i < profile->result_count; ++i)
    if (!game_attempt_id_valid(profile->results[i].attempt_id))
      return true;
  return false;
}

StoreStatus game_migration_upgrade(const char *root, const char *profile_path,
                                   const char *backup_path, ProfileData *profile,
                                   bool *upgraded) {
  if (upgraded) *upgraded = false;
  if (!root || !profile_path || !backup_path || !profile ||
      !profile_validate(profile))
    return STORE_IO_ERROR;

  char intent_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!join(intent_path, root, "profile.dat.v1.3.7-import.dat"))
    return STORE_IO_ERROR;
  bool intent_exists = store_file_exists(intent_path);
  if (!intent_exists && !needs_upgrade(profile)) return STORE_OK;
  if (intent_exists && !needs_upgrade(profile)) {
    /* The active profile has already passed the pending-clear checkpoint. */
    ProfileData old_intent;
    StoreStatus status = profile_load_file(intent_path, &old_intent);
    if (status != STORE_OK) return status;
    return old_intent.migration_pending ? STORE_OK : STORE_CORRUPT;
  }

  unsigned char current[SOURCE_MAX];
  size_t current_count = 0;
  StoreStatus status = read_source(profile_path, current, &current_count);
  if (status != STORE_OK) return status;

  ProfileData pending;
  if (intent_exists) {
    status = profile_load_file(intent_path, &pending);
    if (status != STORE_OK) return status;
    if (!pending.migration_pending ||
        pending.codec_version != SUDOKURA_PROFILE_CONTENT_VERSION)
      return STORE_CORRUPT;
    unsigned char source[SOURCE_MAX];
    size_t source_count = 0;
    status = verify_source(root, &pending, source, &source_count);
    if (status != STORE_OK) return status;
    if (profile->codec_version == SUDOKURA_PROFILE_CONTENT_VERSION_LEGACY &&
        (source_count != current_count ||
         memcmp(source, current, source_count) != 0))
      return STORE_CORRUPT;
    if (profile->migration_pending &&
        (profile->migration_source_crc != pending.migration_source_crc ||
         profile->migration_source_length != pending.migration_source_length ||
         profile->migration_source_suffix != pending.migration_source_suffix))
      return STORE_CORRUPT;
  } else {
    pending = *profile;
    pending.codec_version = SUDOKURA_PROFILE_CONTENT_VERSION;
    pending.migration_pending = true;
    pending.migration_source_length = (uint32_t)current_count;
    pending.migration_source_crc = crc32_bytes(current, current_count);
    status = ensure_source(root, profile_path, current, current_count,
                           &pending.migration_source_suffix);
    if (status != STORE_OK) return status;
    if (!allocate_ids(root, &pending)) return STORE_IO_ERROR;
    status = profile_save_file(intent_path, NULL, &pending);
    if (status != STORE_OK) return status;
    ProfileData verified;
    status = profile_load_file(intent_path, &verified);
    if (status != STORE_OK) return status;
    if (!profile_encoded_equal(&pending, &verified)) return STORE_CORRUPT;
  }

  /* The immutable intent fixes identity before any record or hot write. */
  if (!profile->migration_pending ||
      !profile_encoded_equal(profile, &pending)) {
    status = profile_save_file(profile_path, backup_path, &pending);
    if (status != STORE_OK) return status;
  }
  ProfileData readback;
  status = profile_load_file(profile_path, &readback);
  if (status != STORE_OK) return status;
  if (!profile_encoded_equal(&pending, &readback)) return STORE_CORRUPT;

  ProfileData complete = pending;
  status = migrate_slot(root, &complete.normal);
  if (status != STORE_OK) return status;
  status = migrate_slot(root, &complete.daily);
  if (status != STORE_OK) return status;
  status = migrate_results(root, &complete);
  if (status != STORE_OK) return status;

  complete.migration_pending = false;
  if (!profile_validate(&complete)) return STORE_CORRUPT;
  status = profile_save_file(profile_path, backup_path, &complete);
  if (status != STORE_OK) return status;
  status = profile_load_file(profile_path, &readback);
  if (status != STORE_OK) return status;
  if (!profile_encoded_equal(&complete, &readback)) return STORE_CORRUPT;
  *profile = readback;
  if (upgraded) *upgraded = true;
  return STORE_OK;
}

static bool same_result_metadata(const ProfileResult *a,
                                 const ProfileResult *b) {
  return a->seed == b->seed &&
         a->generator_revision == b->generator_revision &&
         a->difficulty == b->difficulty &&
         a->mode == b->mode && a->status == b->status &&
         a->assisted == b->assisted && a->is_daily == b->is_daily &&
         a->elapsed_ms == b->elapsed_ms &&
         a->mistakes == b->mistakes && a->strikes == b->strikes &&
         a->daily_year == b->daily_year &&
         a->daily_month == b->daily_month &&
         a->daily_day == b->daily_day;
}

static bool same_serialized_session(const ProfileSession *a,
                                    const ProfileSession *b) {
  unsigned char left[4096], right[4096];
  size_t a_len = 0, b_len = 0;
  return profile_session_encode_blob(a, left, sizeof(left), &a_len) &&
         profile_session_encode_blob(b, right, sizeof(right), &b_len) &&
         a_len == b_len && memcmp(left, right, a_len) == 0;
}

StoreStatus game_migration_reconcile(const char *root,
                                     const char *profile_path,
                                     const char *backup_path,
                                     ProfileData *profile) {
  if (!root || !profile_path || !backup_path || !profile ||
      !profile_validate(profile) || profile->migration_pending)
    return STORE_IO_ERROR;
  ProfileData staged = *profile;
  bool changed = false;
  ProfileSlot *slots[2] = {&staged.normal, &staged.daily};
  for (unsigned index = 0; index < 2; ++index) {
    ProfileSlot *slot = slots[index];
    if (!slot->present || !slot->retained) continue;
    if (!game_attempt_id_valid(slot->attempt_id)) return STORE_CORRUPT;
    StoreStatus receipt = game_library_receipt_status(root, slot->attempt_id);
    if (receipt == STORE_OK) {
      memset(slot, 0, sizeof(*slot));
      changed = true;
      continue;
    }
    if (receipt != STORE_NOT_FOUND) return receipt;
    GameLibraryRecord disk;
    StoreStatus status = game_library_load(root, slot->attempt_id, &disk);
    if (status != STORE_OK) return status;
    if (disk.kind != GAME_RECORD_EXACT ||
        disk.result.is_daily != (index == 1) ||
        disk.result.seed != slot->value.session.game.seed ||
        disk.result.generator_revision !=
            slot->value.session.game.generator_revision ||
        disk.result.difficulty != slot->value.session.game.difficulty ||
        disk.result.mode != slot->value.session.mode)
      return STORE_CORRUPT;
    if (disk.revision < slot->revision) return STORE_IO_ERROR;
    if (disk.revision == slot->revision) {
      if (!same_serialized_session(&disk.exact, &slot->value))
        return STORE_CORRUPT;
    } else {
      slot->value = disk.exact;
      slot->revision = disk.revision;
      changed = true;
    }
    if (disk.result.status != SESSION_ACTIVE) {
      ProfileResult terminal = disk.result;
      terminal.attempt_id = disk.id;
      terminal.practice = disk.practice;
      bool inserted = false;
      if (!profile_record_result(&staged, &terminal, &inserted))
        return STORE_CORRUPT;
      if (inserted) changed = true;
    }
  }
  for (uint16_t i = 0; i < staged.result_count;) {
    StoreStatus receipt = game_attempt_id_valid(staged.results[i].attempt_id)
        ? game_library_receipt_status(root, staged.results[i].attempt_id)
        : STORE_NOT_FOUND;
    if (receipt == STORE_OK) {
      memmove(&staged.results[i], &staged.results[i + 1],
              (staged.result_count - i - 1u) * sizeof(staged.results[0]));
      --staged.result_count;
      changed = true;
      continue;
    }
    if (receipt != STORE_NOT_FOUND) return receipt;
    ++i;
  }
  if (!changed) return STORE_OK;
  if (!profile_validate(&staged)) return STORE_CORRUPT;
  StoreStatus status = profile_save_file(profile_path, backup_path, &staged);
  if (status != STORE_OK) return status;
  ProfileData loaded;
  status = profile_load_file(profile_path, &loaded);
  if (status != STORE_OK) return status;
  if (!profile_encoded_equal(&loaded, &staged)) return STORE_CORRUPT;
  *profile = loaded;
  return STORE_OK;
}

static StoreStatus missing_hot_pair(const char *primary, const char *backup) {
  if (!primary || !backup) return STORE_IO_ERROR;
  unsigned char sentinel = 0;
  StoreStatus a = store_read_prefix(primary, &sentinel, 1u, NULL);
  StoreStatus b = store_read_prefix(backup, &sentinel, 1u, NULL);
  if (a != STORE_NOT_FOUND || b != STORE_NOT_FOUND)
    return a != STORE_NOT_FOUND && a != STORE_OK ? a
         : b != STORE_NOT_FOUND && b != STORE_OK ? b
         : STORE_IO_ERROR;
  return STORE_OK;
}

typedef struct {
  const char *root;
  ProfileData staged;
  GameMigrationRecoverySummary summary;
} RecoveryScan;

static bool inspect_recovery_entry(const GameLibraryEntry *entry, void *ctx) {
  RecoveryScan *scan = (RecoveryScan *)ctx;
  if (entry->only_backup) {
    ++scan->summary.previous_only_records;
    return true; /* Earlier copy needs explicit validation and confirmation. */
  }
  if (entry->status != STORE_OK) {
    ++scan->summary.unavailable_records;
    return true;
  }
  ++scan->summary.compatible_records;
  if (entry->kind != GAME_RECORD_EXACT ||
      entry->result.status != SESSION_ACTIVE)
    return true;
  /* Full canonical decode only for active candidates, not 1000 won items. */
  GameLibraryRecord record;
  if (game_library_load(scan->root, entry->id, &record) != STORE_OK) {
    --scan->summary.compatible_records;
    ++scan->summary.unavailable_records;
    return true;
  }
  if (!record.retained || record.kind != GAME_RECORD_EXACT)
    return true;

  bool daily = record.exact.session.is_daily;
  size_t *candidates = daily ? &scan->summary.daily_active_candidates
                             : &scan->summary.normal_active_candidates;
  ProfileSlot *slot = daily ? &scan->staged.daily : &scan->staged.normal;
  ++*candidates;
  if (*candidates == 1u) {
    /* Record was already canonically validated by game_library_load. */
    slot->present = true;
    slot->retained = true;
    slot->attempt_id = record.id;
    slot->revision = record.revision;
    slot->value = record.exact;
  } else {
    /* Revision numbers are per attempt: never guess which attempt is newer. */
    memset(slot, 0, sizeof(*slot));
  }
  return true;
}

StoreStatus game_migration_preview_collection(
    const char *root, ProfileData *candidate,
    GameMigrationRecoverySummary *summary) {
  if (!root || !candidate || !summary) return STORE_IO_ERROR;
  if (!game_migration_has_artifacts(root)) return STORE_NOT_FOUND;
  RecoveryScan scan;
  memset(&scan, 0, sizeof(scan));
  scan.root = root;
  profile_defaults(&scan.staged);
  StoreStatus status = game_library_scan(root, inspect_recovery_entry, &scan);
  if (status != STORE_OK) return status;
  if (!profile_validate(&scan.staged)) return STORE_CORRUPT;
  *candidate = scan.staged;
  *summary = scan.summary;
  return STORE_OK;
}

StoreStatus game_migration_recover_collection(
    const char *root, const char *profile_path, const char *backup_path,
    ProfileData *profile, GameMigrationRecoverySummary *summary) {
  if (!root || !profile_path || !backup_path || !profile || !summary)
    return STORE_IO_ERROR;
  StoreStatus status = missing_hot_pair(profile_path, backup_path);
  if (status != STORE_OK) return status;
  char intent_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!join(intent_path, root, "profile.dat.v1.3.7-import.dat"))
    return STORE_IO_ERROR;
  unsigned char prefix = 0;
  StoreStatus intent_status =
      store_read_prefix(intent_path, &prefix, 1u, NULL);
  if (intent_status == STORE_OK)
    return STORE_IO_ERROR; /* Earlier-checkpoint recovery must be selected. */
  if (intent_status != STORE_NOT_FOUND) return intent_status;

  ProfileData rebuilt;
  GameMigrationRecoverySummary preview;
  status = game_migration_preview_collection(root, &rebuilt, &preview);
  if (status != STORE_OK) return status;
  if (preview.compatible_records == 0u) {
    /* Backup-only, corrupt, receipt-only and orphan-file collections have
       no validated record to rebuild. Never publish silent defaults. */
    return STORE_IO_ERROR;
  }
  status = profile_save_file(profile_path, NULL, &rebuilt);
  if (status != STORE_OK) return status;
  ProfileData checked;
  status = profile_load_file(profile_path, &checked);
  if (status != STORE_OK) return status;
  if (!profile_encoded_equal(&rebuilt, &checked)) return STORE_CORRUPT;
  *profile = checked;
  *summary = preview;
  return STORE_OK;
}

StoreStatus game_migration_recover_intent(
    const char *root, const char *profile_path, const char *backup_path,
    ProfileData *profile) {
  if (!root || !profile_path || !backup_path || !profile)
    return STORE_IO_ERROR;
  StoreStatus status = missing_hot_pair(profile_path, backup_path);
  if (status != STORE_OK) return status;
  char intent_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!join(intent_path, root, "profile.dat.v1.3.7-import.dat"))
    return STORE_IO_ERROR;
  ProfileData intent;
  status = profile_load_file(intent_path, &intent);
  if (status != STORE_OK) return status;
  if (!intent.migration_pending ||
      intent.codec_version != SUDOKURA_PROFILE_CONTENT_VERSION)
    return STORE_CORRUPT;

  unsigned char original[SOURCE_MAX];
  size_t source_length = 0;
  status = verify_source(root, &intent, original, &source_length);
  if (status != STORE_OK) return status;
  /* A verified source/intent is enough to rehydrate pending state once.
     Failure after publication leaves a resuming pending hot profile, not a
     fresh-default state and not a second allocation of attempt IDs. */
  status = profile_save_file(profile_path, NULL, &intent);
  if (status != STORE_OK) return status;
  ProfileData restored;
  status = profile_load_file(profile_path, &restored);
  if (status != STORE_OK) return status;
  if (!profile_encoded_equal(&intent, &restored)) return STORE_CORRUPT;
  status = game_migration_upgrade(root, profile_path, backup_path,
                                  &restored, NULL);
  if (status != STORE_OK) return status;
  status = game_migration_reconcile(root, profile_path, backup_path,
                                    &restored);
  if (status != STORE_OK) return status;
  *profile = restored;
  return STORE_OK;
}
