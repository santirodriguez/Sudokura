#include "game_library.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum { RECORD_HEADER_BYTES = 18, RECORD_PAYLOAD_VERSION = 1 };
enum {
  RECORD_RETAINED = 1u << 0,
  RECORD_PRACTICE = 1u << 1,
  RECORD_ORIGINAL_DAILY = 1u << 2,
  RECORD_KNOWN_FLAGS = RECORD_RETAINED | RECORD_PRACTICE |
                       RECORD_ORIGINAL_DAILY
};
static const unsigned char record_magic[8] = {
    'S', 'U', 'D', 'O', 'G', 'A', 'M', 'E'};

typedef struct {
  unsigned char *data;
  size_t capacity, pos;
  bool ok;
} Writer;

typedef struct {
  const unsigned char *data;
  size_t length, pos;
  bool ok;
} Reader;

static void w8(Writer *w, uint8_t value) {
  if (!w->ok || w->pos >= w->capacity) { w->ok = false; return; }
  w->data[w->pos++] = value;
}
static void w16(Writer *w, uint16_t value) {
  w8(w, (uint8_t)value);
  w8(w, (uint8_t)(value >> 8));
}
static void w32(Writer *w, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    w8(w, (uint8_t)(value >> shift));
}
static void w64(Writer *w, uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8)
    w8(w, (uint8_t)(value >> shift));
}
static void wbytes(Writer *w, const unsigned char *src, size_t count) {
  if (!w->ok || count > w->capacity - w->pos) { w->ok = false; return; }
  memcpy(w->data + w->pos, src, count);
  w->pos += count;
}
static uint8_t r8(Reader *r) {
  if (!r->ok || r->pos >= r->length) { r->ok = false; return 0; }
  return r->data[r->pos++];
}
static uint16_t r16(Reader *r) {
  uint16_t result = r8(r);
  return (uint16_t)(result | (uint16_t)((uint16_t)r8(r) << 8));
}
static uint32_t r32(Reader *r) {
  uint32_t result = 0;
  for (unsigned shift = 0; shift < 32; shift += 8)
    result |= (uint32_t)r8(r) << shift;
  return result;
}
static uint64_t r64(Reader *r) {
  uint64_t result = 0;
  for (unsigned shift = 0; shift < 64; shift += 8)
    result |= (uint64_t)r8(r) << shift;
  return result;
}
static void rbytes(Reader *r, unsigned char *target, size_t count) {
  if (!r->ok || count > r->length - r->pos) { r->ok = false; return; }
  memcpy(target, r->data + r->pos, count);
  r->pos += count;
}
static uint16_t load16(const unsigned char *data) {
  return (uint16_t)(data[0] | (uint16_t)((uint16_t)data[1] << 8));
}
static uint32_t load32(const unsigned char *data) {
  uint32_t result = 0;
  for (unsigned shift = 0; shift < 32; shift += 8)
    result |= (uint32_t)data[shift / 8] << shift;
  return result;
}
static void store16(unsigned char *data, uint16_t value) {
  data[0] = (unsigned char)value;
  data[1] = (unsigned char)(value >> 8);
}
static void store32(unsigned char *data, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    data[shift / 8] = (unsigned char)(value >> shift);
}
static uint32_t checksum(const unsigned char *data, size_t length) {
  uint32_t crc = UINT32_C(0xffffffff);
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (unsigned j = 0; j < 8; ++j)
      crc = (crc >> 1) ^ (UINT32_C(0xedb88320) &
                           (0u - (crc & 1u)));
  }
  return ~crc;
}

bool game_attempt_id_valid(GameAttemptId id) {
  return id.high != 0 || id.low != 0;
}
bool game_attempt_id_equal(GameAttemptId a, GameAttemptId b) {
  return a.high == b.high && a.low == b.low;
}
void game_attempt_id_hex(GameAttemptId id, char out[33]) {
  if (!out) return;
  (void)snprintf(out, 33, "%016" PRIx64 "%016" PRIx64,
                 id.high, id.low);
}
static int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
bool game_attempt_id_parse(const char *hex, GameAttemptId *id) {
  if (!hex || !id) return false;
  GameAttemptId value = {0, 0};
  for (unsigned i = 0; i < 32; ++i) {
    int part = nibble(hex[i]);
    if (hex[i] == '\0' || part < 0) return false;
    if (i < 16) value.high = (value.high << 4) | (uint64_t)part;
    else value.low = (value.low << 4) | (uint64_t)part;
  }
  if (hex[32] != '\0' || !game_attempt_id_valid(value)) return false;
  *id = value;
  return true;
}

static bool valid_summary(const ProfileResult *result) {
  if (!result || !game_generator_revision_supported(
                     result->generator_revision) ||
      result->difficulty < DIFFICULTY_EASY ||
      result->difficulty >= DIFFICULTY_COUNT ||
      result->mode < MODE_CLASSIC || result->mode > MODE_TIME ||
      result->status < SESSION_ACTIVE || result->status > SESSION_LOST ||
      result->elapsed_ms > SUDOKURA_SESSION_MAX_ELAPSED_MS ||
      result->mistakes > UINT32_C(1000000) ||
      result->strikes > UINT32_C(1000000))
    return false;
  if (result->is_daily) {
    uint64_t seed = 0;
    if (result->mode != MODE_CLASSIC ||
        result->difficulty != DIFFICULTY_MEDIUM ||
        !game_daily_seed_revision(result->daily_year, result->daily_month,
                                  result->daily_day,
                                  result->generator_revision, &seed) ||
        seed != result->seed)
      return false;
  } else if (result->daily_year != 0 || result->daily_month != 0 ||
             result->daily_day != 0) {
    return false;
  }
  return true;
}

static bool summary_matches_session(const GameLibraryRecord *record) {
  const SessionState *s = &record->exact.session;
  const ProfileResult *r = &record->result;
  if (r->seed != s->game.seed ||
      r->generator_revision != s->game.generator_revision ||
      r->difficulty != s->game.difficulty || r->mode != s->mode ||
      r->status != s->status || r->assisted != record->exact.assisted ||
      r->is_daily != s->is_daily || r->elapsed_ms != s->elapsed_ms ||
      r->mistakes != (uint32_t)s->mistakes ||
      r->strikes != (uint32_t)s->strikes ||
      r->daily_year != (uint16_t)s->daily_year ||
      r->daily_month != (uint8_t)s->daily_month ||
      r->daily_day != (uint8_t)s->daily_day)
    return false;
  for (size_t i = 0; i < 81; ++i)
    if (record->original[i] != s->game.initial[i] ||
        record->solution[i] != s->game.solution[i])
      return false;
  return true;
}

static bool record_validate(const GameLibraryRecord *record,
                            bool canonical) {
  if (!record || !game_attempt_id_valid(record->id) ||
      record->revision == 0 || !valid_summary(&record->result))
    return false;
  if ((record->created_utc && record->updated_utc &&
       record->updated_utc < record->created_utc) ||
      (record->finished_utc && record->result.status == SESSION_ACTIVE) ||
      (record->finished_utc && record->updated_utc &&
       record->finished_utc > record->updated_utc) ||
      (record->finished_utc && record->created_utc &&
       record->finished_utc < record->created_utc))
    return false;
  if (record->practice) {
    if (!game_attempt_id_valid(record->parent) ||
        game_attempt_id_equal(record->parent, record->id))
      return false;
  } else if (game_attempt_id_valid(record->parent)) {
    return false;
  }
  if (record->original_daily) {
    uint64_t original_seed = 0;
    if (!game_daily_seed_revision(record->original_daily_year,
                                  record->original_daily_month,
                                  record->original_daily_day,
                                  record->result.generator_revision,
                                  &original_seed) ||
        original_seed != record->result.seed)
      return false;
  } else if (record->original_daily_year || record->original_daily_month ||
             record->original_daily_day || record->result.is_daily)
    return false;
  if (record->result.is_daily != (record->original_daily &&
                                   !record->practice))
    return false;

  if (record->kind == GAME_RECORD_LEGACY_SUMMARY)
    return record->retained && !record->practice &&
           record->result.status == SESSION_WON &&
           profile_result_is_valid(&record->result);
  if (record->kind != GAME_RECORD_EXACT ||
      (!record->retained && record->result.status != SESSION_WON))
    return false;
  unsigned char serialized[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t written = 0;
  bool encoded = canonical
      ? profile_session_encode_blob(&record->exact, serialized,
                                    sizeof(serialized), &written)
      : profile_session_encode_blob_runtime(&record->exact, serialized,
                                            sizeof(serialized), &written);
  return encoded && written > 0 && summary_matches_session(record);
}

bool game_library_record_validate(const GameLibraryRecord *record) {
  return record_validate(record, true);
}

bool game_library_record_validate_runtime(const GameLibraryRecord *record) {
  return record_validate(record, false);
}

static bool make_exact(GameLibraryRecord *out, GameAttemptId id,
                       uint64_t revision, const ProfileSession *session,
                       bool retained, bool canonical) {
  if (!out || !session || !game_attempt_id_valid(id) || revision == 0)
    return false;
  GameLibraryRecord value;
  memset(&value, 0, sizeof(value));
  value.kind = GAME_RECORD_EXACT;
  value.id = id;
  value.revision = revision;
  value.exact = *session;
  value.retained = retained || session->session.status == SESSION_WON;
  const SessionState *s = &session->session;
  value.result.seed = s->game.seed;
  value.result.generator_revision = s->game.generator_revision;
  value.result.difficulty = s->game.difficulty;
  value.result.mode = s->mode;
  value.result.status = s->status;
  value.result.assisted = session->assisted;
  value.result.is_daily = s->is_daily;
  value.result.elapsed_ms = s->elapsed_ms;
  value.result.mistakes = (uint32_t)s->mistakes;
  value.result.strikes = (uint32_t)s->strikes;
  value.result.daily_year = (uint16_t)s->daily_year;
  value.result.daily_month = (uint8_t)s->daily_month;
  value.result.daily_day = (uint8_t)s->daily_day;
  value.original_daily = s->is_daily;
  value.original_daily_year = (uint16_t)s->daily_year;
  value.original_daily_month = (uint8_t)s->daily_month;
  value.original_daily_day = (uint8_t)s->daily_day;
  for (size_t i = 0; i < 81; ++i) {
    value.original[i] = (uint8_t)s->game.initial[i];
    value.solution[i] = (uint8_t)s->game.solution[i];
  }
  if (!record_validate(&value, canonical)) return false;
  *out = value;
  return true;
}

bool game_library_make_exact(GameLibraryRecord *out, GameAttemptId id,
                             uint64_t revision, const ProfileSession *session,
                             bool retained) {
  return make_exact(out, id, revision, session, retained, true);
}

bool game_library_make_exact_runtime(GameLibraryRecord *out, GameAttemptId id,
                                     uint64_t revision,
                                     const ProfileSession *session,
                                     bool retained) {
  return make_exact(out, id, revision, session, retained, false);
}

bool game_library_make_legacy(GameLibraryRecord *out, GameAttemptId id,
                              const ProfileResult *result) {
  if (!out || !result || !game_attempt_id_valid(id) ||
      result->status != SESSION_WON || !profile_result_is_valid(result))
    return false;
  GameLibraryRecord value;
  memset(&value, 0, sizeof(value));
  value.kind = GAME_RECORD_LEGACY_SUMMARY;
  value.id = id;
  value.revision = 1;
  value.retained = true;
  value.result = *result;
  value.original_daily = result->is_daily;
  value.original_daily_year = result->daily_year;
  value.original_daily_month = result->daily_month;
  value.original_daily_day = result->daily_day;
  if (!game_library_record_validate(&value)) return false;
  *out = value;
  return true;
}

static void encode_result(Writer *w, const ProfileResult *r) {
  w64(w, r->seed);
  w32(w, r->generator_revision);
  w8(w, (uint8_t)r->difficulty);
  w8(w, (uint8_t)r->mode);
  w8(w, (uint8_t)r->status);
  w8(w, r->assisted ? 1u : 0u);
  w8(w, r->is_daily ? 1u : 0u);
  w64(w, r->elapsed_ms);
  w32(w, r->mistakes);
  w32(w, r->strikes);
  w16(w, r->daily_year);
  w8(w, r->daily_month);
  w8(w, r->daily_day);
}

static void decode_result(Reader *r, ProfileResult *result) {
  result->seed = r64(r);
  result->generator_revision = r32(r);
  result->difficulty = (GameDifficulty)r8(r);
  result->mode = (GameMode)r8(r);
  result->status = (SessionStatus)r8(r);
  uint8_t assisted = r8(r);
  uint8_t daily = r8(r);
  result->elapsed_ms = r64(r);
  result->mistakes = r32(r);
  result->strikes = r32(r);
  result->daily_year = r16(r);
  result->daily_month = r8(r);
  result->daily_day = r8(r);
  if (assisted > 1 || daily > 1) r->ok = false;
  result->assisted = assisted == 1;
  result->is_daily = daily == 1;
}

static bool encode_record(const GameLibraryRecord *record,
                          unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES],
                          size_t *size, bool canonical) {
  if (!bytes || !size || !record_validate(record, canonical))
    return false;
  unsigned char session_blob[SUDOKURA_GAME_RECORD_MAX_BYTES];
  size_t session_size = 0;
  if (record->kind == GAME_RECORD_EXACT) {
    bool valid = canonical
        ? profile_session_encode_blob(&record->exact, session_blob,
                                      sizeof(session_blob), &session_size)
        : profile_session_encode_blob_runtime(&record->exact, session_blob,
                                              sizeof(session_blob),
                                              &session_size);
    if (!valid || session_size > UINT16_MAX) return false;
  }

  Writer w = {bytes + RECORD_HEADER_BYTES,
              SUDOKURA_GAME_RECORD_MAX_BYTES - RECORD_HEADER_BYTES, 0, true};
  w8(&w, (uint8_t)record->kind);
  uint8_t flags = (record->retained ? RECORD_RETAINED : 0) |
                  (record->practice ? RECORD_PRACTICE : 0) |
                  (record->original_daily ? RECORD_ORIGINAL_DAILY : 0);
  w8(&w, flags);
  w16(&w, RECORD_PAYLOAD_VERSION);
  w64(&w, record->id.high);
  w64(&w, record->id.low);
  w64(&w, record->parent.high);
  w64(&w, record->parent.low);
  w64(&w, record->revision);
  w64(&w, record->created_utc);
  w64(&w, record->updated_utc);
  w64(&w, record->finished_utc);
  w16(&w, record->original_daily_year);
  w8(&w, record->original_daily_month);
  w8(&w, record->original_daily_day);
  encode_result(&w, &record->result);
  w16(&w, (uint16_t)session_size);
  if (record->kind == GAME_RECORD_EXACT) {
    wbytes(&w, record->original, 81);
    wbytes(&w, record->solution, 81);
    wbytes(&w, session_blob, session_size);
  }
  if (!w.ok) return false;
  memcpy(bytes, record_magic, sizeof(record_magic));
  store16(bytes + 8, SUDOKURA_GAME_RECORD_VERSION);
  store32(bytes + 10, (uint32_t)w.pos);
  store32(bytes + 14, checksum(bytes + RECORD_HEADER_BYTES, w.pos));
  *size = RECORD_HEADER_BYTES + w.pos;
  return true;
}

bool game_library_encode(const GameLibraryRecord *record,
                         unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES],
                         size_t *size) {
  return encode_record(record, bytes, size, true);
}

bool game_library_encode_runtime(
    const GameLibraryRecord *record,
    unsigned char bytes[SUDOKURA_GAME_RECORD_MAX_BYTES], size_t *size) {
  return encode_record(record, bytes, size, false);
}

static StoreStatus decode_record(const unsigned char *bytes, size_t size,
                                 GameLibraryRecord *record,
                                 GameLibraryEntry *entry, bool full) {
  if (!bytes || (full && !record) || (!full && !entry))
    return STORE_IO_ERROR;
  if (size < RECORD_HEADER_BYTES ||
      size > SUDOKURA_GAME_RECORD_MAX_BYTES ||
      memcmp(bytes, record_magic, sizeof(record_magic)) != 0)
    return STORE_CORRUPT;
  if (load16(bytes + 8) != SUDOKURA_GAME_RECORD_VERSION)
    return STORE_INCOMPATIBLE;
  uint32_t length = load32(bytes + 10);
  if (length > SUDOKURA_GAME_RECORD_MAX_BYTES - RECORD_HEADER_BYTES ||
      length != size - RECORD_HEADER_BYTES)
    return STORE_CORRUPT;
  if (checksum(bytes + RECORD_HEADER_BYTES, length) != load32(bytes + 14))
    return STORE_CORRUPT;

  Reader r = {bytes + RECORD_HEADER_BYTES, length, 0, true};
  GameLibraryRecord candidate;
  memset(&candidate, 0, sizeof(candidate));
  candidate.kind = (GameRecordKind)r8(&r);
  uint8_t flags = r8(&r);
  uint16_t payload_version = r16(&r);
  if (!r.ok) return STORE_CORRUPT;
  if (payload_version != RECORD_PAYLOAD_VERSION) return STORE_INCOMPATIBLE;
  if ((flags & ~RECORD_KNOWN_FLAGS) != 0 ||
      (candidate.kind != GAME_RECORD_EXACT &&
       candidate.kind != GAME_RECORD_LEGACY_SUMMARY))
    return STORE_CORRUPT;
  candidate.retained = (flags & RECORD_RETAINED) != 0;
  candidate.practice = (flags & RECORD_PRACTICE) != 0;
  candidate.original_daily = (flags & RECORD_ORIGINAL_DAILY) != 0;
  candidate.id.high = r64(&r);
  candidate.id.low = r64(&r);
  candidate.parent.high = r64(&r);
  candidate.parent.low = r64(&r);
  candidate.revision = r64(&r);
  candidate.created_utc = r64(&r);
  candidate.updated_utc = r64(&r);
  candidate.finished_utc = r64(&r);
  candidate.original_daily_year = r16(&r);
  candidate.original_daily_month = r8(&r);
  candidate.original_daily_day = r8(&r);
  decode_result(&r, &candidate.result);
  uint16_t session_size = r16(&r);
  if (!r.ok || !game_attempt_id_valid(candidate.id) ||
      candidate.revision == 0)
    return STORE_CORRUPT;
  if (!game_generator_revision_supported(candidate.result.generator_revision))
    return STORE_INCOMPATIBLE;
  if (!valid_summary(&candidate.result))
    return STORE_CORRUPT;
  if (candidate.kind == GAME_RECORD_EXACT) {
    if (!r.ok || r.length - r.pos < 162u + session_size ||
        session_size == 0)
      return STORE_CORRUPT;
    rbytes(&r, candidate.original, 81);
    rbytes(&r, candidate.solution, 81);
    if (full) {
      StoreStatus status = profile_session_decode_blob(
          r.data + r.pos, session_size,
          candidate.result.is_daily, &candidate.exact);
      if (status != STORE_OK) return status;
    }
    r.pos += session_size;
  } else if (session_size != 0)
    return STORE_CORRUPT;
  if (!r.ok || r.pos != r.length) return STORE_CORRUPT;
  if (full) {
    if (!game_library_record_validate(&candidate)) return STORE_CORRUPT;
    *record = candidate;
  } else {
    /* A listing checks metadata and CRC only; full history validates on Load. */
    if (!candidate.retained && candidate.result.status != SESSION_WON)
      return STORE_CORRUPT;
    GameLibraryEntry metadata;
    memset(&metadata, 0, sizeof(metadata));
    metadata.id = candidate.id;
    metadata.status = STORE_OK;
    metadata.kind = candidate.kind;
    metadata.result = candidate.result;
    metadata.revision = candidate.revision;
    metadata.created_utc = candidate.created_utc;
    metadata.updated_utc = candidate.updated_utc;
    metadata.finished_utc = candidate.finished_utc;
    *entry = metadata;
  }
  return STORE_OK;
}

StoreStatus game_library_decode(const unsigned char *bytes, size_t size,
                                GameLibraryRecord *record) {
  return decode_record(bytes, size, record, NULL, true);
}
StoreStatus game_library_peek(const unsigned char *bytes, size_t size,
                              GameLibraryEntry *entry) {
  return decode_record(bytes, size, NULL, entry, false);
}
