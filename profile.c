#include "profile.h"

#include "store_io.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROFILE_HEADER_SIZE 18u
#define PROFILE_MAX_FILE_SIZE 16384u
#define PROFILE_NOTES_MASK UINT16_C(0x03fe)
#define PROFILE_MAX_COUNTER UINT32_C(1000000)

static const unsigned char profile_magic[8] = {
    'S', 'U', 'D', 'O', 'P', 'R', 'F', '3'};

typedef struct {
  unsigned char *data;
  size_t size;
  size_t position;
  bool ok;
} ProfileWriter;

typedef struct {
  const unsigned char *data;
  size_t size;
  size_t position;
  bool ok;
} ProfileReader;

static void writer_u8(ProfileWriter *writer, uint8_t value) {
  if (!writer || !writer->ok || writer->position >= writer->size) {
    if (writer) writer->ok = false;
    return;
  }
  writer->data[writer->position++] = value;
}

static void writer_u16(ProfileWriter *writer, uint16_t value) {
  writer_u8(writer, (uint8_t)(value & 0xffu));
  writer_u8(writer, (uint8_t)((value >> 8) & 0xffu));
}

static void writer_u32(ProfileWriter *writer, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    writer_u8(writer, (uint8_t)((value >> shift) & 0xffu));
}

static void writer_i32(ProfileWriter *writer, int32_t value) {
  writer_u32(writer, (uint32_t)value);
}

static void writer_u64(ProfileWriter *writer, uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8)
    writer_u8(writer, (uint8_t)((value >> shift) & UINT64_C(0xff)));
}

static uint8_t reader_u8(ProfileReader *reader) {
  if (!reader || !reader->ok || reader->position >= reader->size) {
    if (reader) reader->ok = false;
    return 0;
  }
  return reader->data[reader->position++];
}

static uint16_t reader_u16(ProfileReader *reader) {
  uint16_t value = reader_u8(reader);
  value |= (uint16_t)((uint16_t)reader_u8(reader) << 8);
  return value;
}

static uint32_t reader_u32(ProfileReader *reader) {
  uint32_t value = 0;
  for (unsigned shift = 0; shift < 32; shift += 8)
    value |= (uint32_t)reader_u8(reader) << shift;
  return value;
}

static int32_t reader_i32(ProfileReader *reader) {
  return (int32_t)reader_u32(reader);
}

static uint64_t reader_u64(ProfileReader *reader) {
  uint64_t value = 0;
  for (unsigned shift = 0; shift < 64; shift += 8)
    value |= (uint64_t)reader_u8(reader) << shift;
  return value;
}

static uint32_t crc32_bytes(const unsigned char *data, size_t size) {
  uint32_t crc = UINT32_C(0xffffffff);
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      uint32_t mask = (uint32_t)(0u - (crc & 1u));
      crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
    }
  }
  return ~crc;
}

static void put_u16(unsigned char *data, uint16_t value) {
  data[0] = (unsigned char)(value & 0xffu);
  data[1] = (unsigned char)((value >> 8) & 0xffu);
}

static void put_u32(unsigned char *data, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    data[shift / 8] = (unsigned char)((value >> shift) & 0xffu);
}

static uint16_t get_u16(const unsigned char *data) {
  return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t get_u32(const unsigned char *data) {
  uint32_t value = 0;
  for (unsigned shift = 0; shift < 32; shift += 8)
    value |= (uint32_t)data[shift / 8] << shift;
  return value;
}

static bool valid_edit(const ProfileEdit *edit) {
  return edit && edit->row < 9 && edit->column < 9 &&
         edit->before_value <= 9 && edit->after_value <= 9 &&
         (edit->before_notes & (uint16_t)~PROFILE_NOTES_MASK) == 0 &&
         (edit->after_notes & (uint16_t)~PROFILE_NOTES_MASK) == 0 &&
         edit->before_hinted <= 1 && edit->after_hinted <= 1;
}

static bool valid_profile_session(const ProfileSession *slot, bool daily) {
  if (!slot || !session_validate(&slot->session) ||
      slot->session.is_daily != daily ||
      slot->undo_count > SUDOKURA_HISTORY_LIMIT ||
      slot->redo_count > SUDOKURA_HISTORY_LIMIT)
    return false;

  bool hinted = false;
  for (int i = 0; i < 81; ++i)
    hinted = hinted || slot->session.game.hinted[i] != 0;
  if (hinted && !slot->assisted) return false;

  for (uint16_t i = 0; i < slot->undo_count; ++i)
    if (!valid_edit(&slot->undo[i])) return false;
  for (uint16_t i = 0; i < slot->redo_count; ++i)
    if (!valid_edit(&slot->redo[i])) return false;
  return true;
}

static bool valid_result(const ProfileResult *result) {
  if (!result || result->generator_revision != SUDOKURA_GENERATOR_REVISION ||
      result->difficulty < DIFFICULTY_EASY ||
      result->difficulty >= DIFFICULTY_COUNT ||
      result->mode < MODE_CLASSIC || result->mode > MODE_TIME ||
      result->status < SESSION_WON || result->status > SESSION_LOST ||
      result->elapsed_ms > SUDOKURA_SESSION_MAX_ELAPSED_MS ||
      result->mistakes > PROFILE_MAX_COUNTER ||
      result->strikes > PROFILE_MAX_COUNTER)
    return false;
  if (result->is_daily) {
    uint64_t seed = 0;
    if (result->mode != MODE_CLASSIC ||
        result->difficulty != DIFFICULTY_MEDIUM ||
        !game_daily_seed(result->daily_year, result->daily_month,
                         result->daily_day, &seed) ||
        seed != result->seed)
      return false;
  } else if (result->daily_year || result->daily_month || result->daily_day) {
    return false;
  }
  return true;
}

void profile_defaults(ProfileData *profile) {
  if (!profile) return;
  memset(profile, 0, sizeof(*profile));
  preferences_defaults(&profile->preferences);
}

bool profile_validate(const ProfileData *profile) {
  if (!profile || !preferences_validate(&profile->preferences) ||
      profile->result_count > SUDOKURA_RESULT_LIMIT)
    return false;
  if (profile->normal.present &&
      !valid_profile_session(&profile->normal.value, false))
    return false;
  if (profile->daily.present &&
      !valid_profile_session(&profile->daily.value, true))
    return false;
  for (uint16_t i = 0; i < profile->result_count; ++i)
    if (!valid_result(&profile->results[i])) return false;
  return true;
}

bool profile_slot_set(ProfileSlot *slot, const SessionState *session,
                      bool assisted) {
  if (!slot || !session || !session_validate(session)) return false;
  memset(slot, 0, sizeof(*slot));
  slot->present = true;
  slot->value.session = *session;
  slot->value.assisted = assisted;
  for (int i = 0; i < 81; ++i)
    if (session->game.hinted[i]) slot->value.assisted = true;
  return valid_profile_session(&slot->value, session->is_daily);
}

const SessionState *profile_slot_session(const ProfileSlot *slot) {
  return slot && slot->present ? &slot->value.session : NULL;
}

static void encode_preferences(ProfileWriter *writer,
                               const Preferences *preferences) {
  writer_u16(writer, SUDOKURA_PREFERENCES_CONTENT_VERSION);
  writer_u8(writer, (uint8_t)preferences->language);
  writer_u8(writer, preferences->dark_theme ? 1u : 0u);
  writer_u8(writer, preferences->strict_mode ? 1u : 0u);
  writer_u8(writer, (uint8_t)preferences->mode);
  writer_u8(writer, (uint8_t)preferences->difficulty);
  writer_u8(writer, preferences->audio_enabled ? 1u : 0u);
  writer_u8(writer, preferences->music_volume);
  writer_u8(writer, preferences->fx_volume);
  writer_u8(writer, preferences->reduced_motion ? 1u : 0u);
  writer_i32(writer, preferences->window_x);
  writer_i32(writer, preferences->window_y);
  writer_u32(writer, preferences->window_width);
  writer_u32(writer, preferences->window_height);
  writer_u8(writer, preferences->window_maximized ? 1u : 0u);
}

static StoreStatus decode_preferences(ProfileReader *reader,
                                      Preferences *preferences) {
  uint16_t version = reader_u16(reader);
  if (!reader->ok) return STORE_CORRUPT;
  if (version != SUDOKURA_PREFERENCES_CONTENT_VERSION)
    return STORE_INCOMPATIBLE;

  Preferences loaded;
  preferences_defaults(&loaded);
  loaded.language = (Language)reader_u8(reader);
  uint8_t dark = reader_u8(reader);
  uint8_t strict = reader_u8(reader);
  loaded.mode = (GameMode)reader_u8(reader);
  loaded.difficulty = (GameDifficulty)reader_u8(reader);
  uint8_t audio = reader_u8(reader);
  loaded.music_volume = reader_u8(reader);
  loaded.fx_volume = reader_u8(reader);
  uint8_t reduced = reader_u8(reader);
  loaded.window_x = reader_i32(reader);
  loaded.window_y = reader_i32(reader);
  loaded.window_width = reader_u32(reader);
  loaded.window_height = reader_u32(reader);
  uint8_t maximized = reader_u8(reader);
  loaded.dark_theme = dark != 0;
  loaded.strict_mode = strict != 0;
  loaded.audio_enabled = audio != 0;
  loaded.reduced_motion = reduced != 0;
  loaded.window_maximized = maximized != 0;

  if (!reader->ok || dark > 1 || strict > 1 || audio > 1 || reduced > 1 ||
      maximized > 1 || !preferences_validate(&loaded))
    return STORE_CORRUPT;
  *preferences = loaded;
  return STORE_OK;
}

static void encode_edit(ProfileWriter *writer, const ProfileEdit *edit) {
  writer_u8(writer, edit->row);
  writer_u8(writer, edit->column);
  writer_u8(writer, edit->before_value);
  writer_u8(writer, edit->after_value);
  writer_u16(writer, edit->before_notes);
  writer_u16(writer, edit->after_notes);
  writer_u8(writer, edit->before_hinted);
  writer_u8(writer, edit->after_hinted);
}

static ProfileEdit decode_edit(ProfileReader *reader) {
  ProfileEdit edit;
  memset(&edit, 0, sizeof(edit));
  edit.row = reader_u8(reader);
  edit.column = reader_u8(reader);
  edit.before_value = reader_u8(reader);
  edit.after_value = reader_u8(reader);
  edit.before_notes = reader_u16(reader);
  edit.after_notes = reader_u16(reader);
  edit.before_hinted = reader_u8(reader);
  edit.after_hinted = reader_u8(reader);
  return edit;
}

static void encode_session(ProfileWriter *writer,
                           const ProfileSession *profile_session) {
  const SessionState *session = &profile_session->session;
  writer_u16(writer, SUDOKURA_SESSION_CONTENT_VERSION);
  writer_u32(writer, session->game.generator_revision);
  writer_u64(writer, session->game.seed);
  writer_u8(writer, (uint8_t)session->game.difficulty);
  writer_u8(writer, (uint8_t)session->mode);
  writer_u8(writer, (uint8_t)session->selected_row);
  writer_u8(writer, (uint8_t)session->selected_column);
  writer_u8(writer, session->notes_mode ? 1u : 0u);
  writer_u8(writer, session->strict_mode ? 1u : 0u);
  writer_u8(writer, session->manual_paused ? 1u : 0u);
  writer_u8(writer, (uint8_t)session->status);
  writer_u32(writer, (uint32_t)session->mistakes);
  writer_u32(writer, (uint32_t)session->strikes);
  writer_u64(writer, session->elapsed_ms);
  writer_u8(writer, session->is_daily ? 1u : 0u);
  writer_u16(writer, (uint16_t)session->daily_year);
  writer_u8(writer, (uint8_t)session->daily_month);
  writer_u8(writer, (uint8_t)session->daily_day);
  writer_u8(writer, profile_session->assisted ? 1u : 0u);
  for (int i = 0; i < 81; ++i)
    writer_u8(writer, (uint8_t)session->game.puzzle[i]);
  for (int i = 0; i < 81; ++i)
    writer_u8(writer, session->game.hinted[i]);
  for (int i = 0; i < 81; ++i)
    writer_u16(writer, session->game.notes[i]);
  writer_u16(writer, profile_session->undo_count);
  for (uint16_t i = 0; i < profile_session->undo_count; ++i)
    encode_edit(writer, &profile_session->undo[i]);
  writer_u16(writer, profile_session->redo_count);
  for (uint16_t i = 0; i < profile_session->redo_count; ++i)
    encode_edit(writer, &profile_session->redo[i]);
}

static StoreStatus decode_session(ProfileReader *reader, ProfileSession *out,
                                  bool expected_daily) {
  uint16_t version = reader_u16(reader);
  if (!reader->ok) return STORE_CORRUPT;
  if (version != SUDOKURA_SESSION_CONTENT_VERSION)
    return STORE_INCOMPATIBLE;

  uint32_t generator_revision = reader_u32(reader);
  uint64_t seed = reader_u64(reader);
  GameDifficulty difficulty = (GameDifficulty)reader_u8(reader);
  GameMode mode = (GameMode)reader_u8(reader);
  int selected_row = reader_u8(reader);
  int selected_column = reader_u8(reader);
  uint8_t notes_mode = reader_u8(reader);
  uint8_t strict_mode = reader_u8(reader);
  uint8_t manual_paused = reader_u8(reader);
  SessionStatus status = (SessionStatus)reader_u8(reader);
  uint32_t mistakes = reader_u32(reader);
  uint32_t strikes = reader_u32(reader);
  uint64_t elapsed_ms = reader_u64(reader);
  uint8_t is_daily = reader_u8(reader);
  int daily_year = reader_u16(reader);
  int daily_month = reader_u8(reader);
  int daily_day = reader_u8(reader);
  uint8_t assisted = reader_u8(reader);

  if (!reader->ok) return STORE_CORRUPT;
  if (generator_revision != SUDOKURA_GENERATOR_REVISION)
    return STORE_INCOMPATIBLE;
  if (difficulty < DIFFICULTY_EASY || difficulty >= DIFFICULTY_COUNT ||
      mode < MODE_CLASSIC || mode > MODE_TIME || notes_mode > 1 ||
      strict_mode > 1 || manual_paused > 1 || is_daily > 1 || assisted > 1 ||
      status < SESSION_ACTIVE || status > SESSION_LOST ||
      mistakes > PROFILE_MAX_COUNTER || strikes > PROFILE_MAX_COUNTER ||
      elapsed_ms > SUDOKURA_SESSION_MAX_ELAPSED_MS ||
      (is_daily != 0) != expected_daily)
    return STORE_CORRUPT;

  ProfileSession loaded;
  memset(&loaded, 0, sizeof(loaded));
  game_new_difficulty(&loaded.session.game, seed, difficulty);
  loaded.session.mode = mode;
  loaded.session.selected_row = selected_row;
  loaded.session.selected_column = selected_column;
  loaded.session.notes_mode = notes_mode != 0;
  loaded.session.strict_mode = strict_mode != 0;
  loaded.session.manual_paused = manual_paused != 0;
  loaded.session.status = status;
  loaded.session.mistakes = (int)mistakes;
  loaded.session.strikes = (int)strikes;
  loaded.session.elapsed_ms = elapsed_ms;
  loaded.session.is_daily = is_daily != 0;
  loaded.session.daily_year = daily_year;
  loaded.session.daily_month = daily_month;
  loaded.session.daily_day = daily_day;
  loaded.assisted = assisted != 0;

  for (int i = 0; i < 81; ++i)
    loaded.session.game.puzzle[i] = reader_u8(reader);
  for (int i = 0; i < 81; ++i)
    loaded.session.game.hinted[i] = reader_u8(reader);
  for (int i = 0; i < 81; ++i)
    loaded.session.game.notes[i] = reader_u16(reader);

  loaded.undo_count = reader_u16(reader);
  if (!reader->ok || loaded.undo_count > SUDOKURA_HISTORY_LIMIT)
    return STORE_CORRUPT;
  for (uint16_t i = 0; i < loaded.undo_count; ++i)
    loaded.undo[i] = decode_edit(reader);

  loaded.redo_count = reader_u16(reader);
  if (!reader->ok || loaded.redo_count > SUDOKURA_HISTORY_LIMIT)
    return STORE_CORRUPT;
  for (uint16_t i = 0; i < loaded.redo_count; ++i)
    loaded.redo[i] = decode_edit(reader);

  if (!reader->ok || !valid_profile_session(&loaded, expected_daily))
    return STORE_CORRUPT;
  *out = loaded;
  return STORE_OK;
}

static void encode_result(ProfileWriter *writer, const ProfileResult *result) {
  writer_u64(writer, result->seed);
  writer_u32(writer, result->generator_revision);
  writer_u8(writer, (uint8_t)result->difficulty);
  writer_u8(writer, (uint8_t)result->mode);
  writer_u8(writer, (uint8_t)result->status);
  writer_u8(writer, result->assisted ? 1u : 0u);
  writer_u8(writer, result->is_daily ? 1u : 0u);
  writer_u64(writer, result->elapsed_ms);
  writer_u32(writer, result->mistakes);
  writer_u32(writer, result->strikes);
  writer_u16(writer, result->daily_year);
  writer_u8(writer, result->daily_month);
  writer_u8(writer, result->daily_day);
}

static StoreStatus decode_result(ProfileReader *reader, ProfileResult *result) {
  ProfileResult loaded;
  memset(&loaded, 0, sizeof(loaded));
  loaded.seed = reader_u64(reader);
  loaded.generator_revision = reader_u32(reader);
  loaded.difficulty = (GameDifficulty)reader_u8(reader);
  loaded.mode = (GameMode)reader_u8(reader);
  loaded.status = (SessionStatus)reader_u8(reader);
  uint8_t assisted = reader_u8(reader);
  uint8_t is_daily = reader_u8(reader);
  loaded.elapsed_ms = reader_u64(reader);
  loaded.mistakes = reader_u32(reader);
  loaded.strikes = reader_u32(reader);
  loaded.daily_year = reader_u16(reader);
  loaded.daily_month = reader_u8(reader);
  loaded.daily_day = reader_u8(reader);
  loaded.assisted = assisted != 0;
  loaded.is_daily = is_daily != 0;
  if (!reader->ok || assisted > 1 || is_daily > 1)
    return STORE_CORRUPT;
  if (loaded.generator_revision != SUDOKURA_GENERATOR_REVISION)
    return STORE_INCOMPATIBLE;
  if (!valid_result(&loaded)) return STORE_CORRUPT;
  *result = loaded;
  return STORE_OK;
}

static bool encode_profile_payload(unsigned char *payload, size_t capacity,
                                   size_t *size_out,
                                   const ProfileData *profile) {
  ProfileWriter writer = {payload, capacity, 0, true};
  writer_u16(&writer, SUDOKURA_PROFILE_CONTENT_VERSION);

  size_t preferences_length_position = writer.position;
  writer_u32(&writer, 0);
  size_t preferences_start = writer.position;
  encode_preferences(&writer, &profile->preferences);
  size_t preferences_size = writer.position - preferences_start;
  if (preferences_size > UINT32_MAX) return false;
  put_u32(payload + preferences_length_position, (uint32_t)preferences_size);

  const ProfileSlot *slots[2] = {&profile->normal, &profile->daily};
  for (int slot_index = 0; slot_index < 2; ++slot_index) {
    const ProfileSlot *slot = slots[slot_index];
    writer_u8(&writer, slot->present ? 1u : 0u);
    if (!slot->present) continue;
    size_t length_position = writer.position;
    writer_u32(&writer, 0);
    size_t section_start = writer.position;
    encode_session(&writer, &slot->value);
    size_t section_size = writer.position - section_start;
    if (section_size > UINT32_MAX) return false;
    put_u32(payload + length_position, (uint32_t)section_size);
  }

  writer_u16(&writer, profile->result_count);
  for (uint16_t i = 0; i < profile->result_count; ++i)
    encode_result(&writer, &profile->results[i]);

  if (!writer.ok) return false;
  if (size_out) *size_out = writer.position;
  return true;
}

static StoreStatus decode_sized_preferences(ProfileReader *reader,
                                            Preferences *preferences) {
  uint32_t size = reader_u32(reader);
  if (!reader->ok || size > reader->size - reader->position)
    return STORE_CORRUPT;
  ProfileReader section = {reader->data + reader->position, size, 0, true};
  StoreStatus status = decode_preferences(&section, preferences);
  if (status != STORE_OK) return status;
  if (!section.ok || section.position != section.size) return STORE_CORRUPT;
  reader->position += size;
  return STORE_OK;
}

static StoreStatus decode_slot(ProfileReader *reader, ProfileSlot *slot,
                               bool expected_daily) {
  uint8_t present = reader_u8(reader);
  if (!reader->ok || present > 1) return STORE_CORRUPT;
  memset(slot, 0, sizeof(*slot));
  if (!present) return STORE_OK;

  uint32_t size = reader_u32(reader);
  if (!reader->ok || size > reader->size - reader->position)
    return STORE_CORRUPT;
  ProfileReader section = {reader->data + reader->position, size, 0, true};
  StoreStatus status = decode_session(&section, &slot->value, expected_daily);
  if (status != STORE_OK) return status;
  if (!section.ok || section.position != section.size) return STORE_CORRUPT;
  reader->position += size;
  slot->present = true;
  return STORE_OK;
}

StoreStatus profile_save_file(const char *path, const char *backup_path,
                              const ProfileData *profile) {
  if (!path || !profile || !profile_validate(profile)) return STORE_IO_ERROR;

  unsigned char data[PROFILE_MAX_FILE_SIZE];
  unsigned char *payload = data + PROFILE_HEADER_SIZE;
  size_t payload_capacity = sizeof(data) - PROFILE_HEADER_SIZE;
  size_t payload_size = 0;
  if (!encode_profile_payload(payload, payload_capacity, &payload_size,
                              profile))
    return STORE_IO_ERROR;

  memcpy(data, profile_magic, sizeof(profile_magic));
  put_u16(data + 8, SUDOKURA_PROFILE_CONTAINER_VERSION);
  put_u32(data + 10, (uint32_t)payload_size);
  put_u32(data + 14, crc32_bytes(payload, payload_size));
  return store_atomic_write(path, backup_path, data,
                            PROFILE_HEADER_SIZE + payload_size);
}

StoreStatus profile_load_file(const char *path, ProfileData *profile) {
  if (!path || !profile) return STORE_IO_ERROR;

  unsigned char data[PROFILE_MAX_FILE_SIZE];
  size_t size = 0;
  StoreStatus status = store_read_file(path, data, sizeof(data), &size);
  if (status != STORE_OK) return status;
  if (size < PROFILE_HEADER_SIZE ||
      memcmp(data, profile_magic, sizeof(profile_magic)) != 0)
    return STORE_CORRUPT;

  uint16_t container_version = get_u16(data + 8);
  uint32_t payload_size = get_u32(data + 10);
  uint32_t stored_crc = get_u32(data + 14);
  if (container_version != SUDOKURA_PROFILE_CONTAINER_VERSION)
    return STORE_INCOMPATIBLE;
  if (payload_size > sizeof(data) - PROFILE_HEADER_SIZE ||
      size != PROFILE_HEADER_SIZE + (size_t)payload_size)
    return STORE_CORRUPT;

  const unsigned char *payload = data + PROFILE_HEADER_SIZE;
  if (crc32_bytes(payload, payload_size) != stored_crc) return STORE_CORRUPT;

  ProfileReader reader = {payload, payload_size, 0, true};
  uint16_t content_version = reader_u16(&reader);
  if (!reader.ok) return STORE_CORRUPT;
  if (content_version != SUDOKURA_PROFILE_CONTENT_VERSION)
    return STORE_INCOMPATIBLE;

  ProfileData loaded;
  profile_defaults(&loaded);
  status = decode_sized_preferences(&reader, &loaded.preferences);
  if (status != STORE_OK) return status;
  status = decode_slot(&reader, &loaded.normal, false);
  if (status != STORE_OK) return status;
  status = decode_slot(&reader, &loaded.daily, true);
  if (status != STORE_OK) return status;

  loaded.result_count = reader_u16(&reader);
  if (!reader.ok || loaded.result_count > SUDOKURA_RESULT_LIMIT)
    return STORE_CORRUPT;
  for (uint16_t i = 0; i < loaded.result_count; ++i) {
    status = decode_result(&reader, &loaded.results[i]);
    if (status != STORE_OK) return status;
  }

  if (!reader.ok || reader.position != reader.size || !profile_validate(&loaded))
    return STORE_CORRUPT;
  *profile = loaded;
  return STORE_OK;
}

static bool parse_legacy_audio_levels(const char *path, uint8_t *music,
                                      uint8_t *fx) {
  if (!path || !music || !fx) return false;
  unsigned char bytes[128];
  size_t size = 0;
  if (store_read_file(path, bytes, sizeof(bytes) - 1, &size) != STORE_OK)
    return false;
  bytes[size] = '\0';
  char magic[16] = {0};
  int loaded_music = 0, loaded_fx = 0;
  if (sscanf((const char *)bytes, "%15s %d %d", magic, &loaded_music,
             &loaded_fx) != 3 ||
      strcmp(magic, "SUDOAUDIO1") != 0 || loaded_music < 0 ||
      loaded_music > 100 || loaded_fx < 0 || loaded_fx > 100)
    return false;
  *music = (uint8_t)loaded_music;
  *fx = (uint8_t)loaded_fx;
  return true;
}

static StoreStatus backup_legacy_file(const char *source, const char *backup) {
  if (!source || !backup || !store_file_exists(source)) return STORE_OK;
  StoreStatus status = store_copy_once(source, backup);
  return status == STORE_NOT_FOUND ? STORE_OK : status;
}

static bool profiles_equal(const ProfileData *a, const ProfileData *b) {
  if (!a || !b) return false;
  unsigned char left[PROFILE_MAX_FILE_SIZE];
  unsigned char right[PROFILE_MAX_FILE_SIZE];
  size_t left_size = 0, right_size = 0;
  return encode_profile_payload(left, sizeof(left), &left_size, a) &&
         encode_profile_payload(right, sizeof(right), &right_size, b) &&
         left_size == right_size && memcmp(left, right, left_size) == 0;
}

StoreStatus profile_load_or_migrate_v12(const char *profile_path,
                                        const char *profile_backup_path,
                                        const ProfileLegacyPaths *legacy,
                                        ProfileData *profile,
                                        bool *migrated) {
  if (!profile_path || !profile) return STORE_IO_ERROR;
  if (migrated) *migrated = false;

  StoreStatus status = profile_load_file(profile_path, profile);
  if (status != STORE_NOT_FOUND) return status;

  ProfileData candidate;
  profile_defaults(&candidate);
  bool imported_any = false;

  if (legacy) {
    status = backup_legacy_file(legacy->session_path,
                                legacy->session_backup_path);
    if (status != STORE_OK) return status;
    status = backup_legacy_file(legacy->preferences_path,
                                legacy->preferences_backup_path);
    if (status != STORE_OK) return status;
    status = backup_legacy_file(legacy->audio_levels_path,
                                legacy->audio_levels_backup_path);
    if (status != STORE_OK) return status;
  }

  const char *preferences_source =
      legacy && legacy->preferences_backup_path &&
              store_file_exists(legacy->preferences_backup_path)
          ? legacy->preferences_backup_path
          : legacy ? legacy->preferences_path : NULL;
  if (preferences_source && store_file_exists(preferences_source)) {
    Preferences preferences;
    preferences_defaults(&preferences);
    status = preferences_load_file(preferences_source, &preferences);
    if (status != STORE_OK) return status;
    candidate.preferences = preferences;
    imported_any = true;
  }

  const char *audio_source =
      legacy && legacy->audio_levels_backup_path &&
              store_file_exists(legacy->audio_levels_backup_path)
          ? legacy->audio_levels_backup_path
          : legacy ? legacy->audio_levels_path : NULL;
  if (audio_source && store_file_exists(audio_source)) {
    uint8_t music = candidate.preferences.music_volume;
    uint8_t fx = candidate.preferences.fx_volume;
    if (!parse_legacy_audio_levels(audio_source, &music, &fx))
      return STORE_CORRUPT;
    candidate.preferences.music_volume = music;
    candidate.preferences.fx_volume = fx;
    imported_any = true;
  }

  const char *session_source =
      legacy && legacy->session_backup_path &&
              store_file_exists(legacy->session_backup_path)
          ? legacy->session_backup_path
          : legacy ? legacy->session_path : NULL;
  if (session_source && store_file_exists(session_source)) {
    SessionState session;
    memset(&session, 0, sizeof(session));
    status = session_load_file(session_source, &session);
    if (status == STORE_INCOMPATIBLE || status == STORE_IO_ERROR)
      return status;
    if (status == STORE_CORRUPT) return status;
    if (status == STORE_OK) {
      ProfileSlot *slot =
          session.is_daily ? &candidate.daily : &candidate.normal;
      if (!profile_slot_set(slot, &session, false)) return STORE_CORRUPT;
      imported_any = true;
    }
  }

  if (!profile_validate(&candidate)) return STORE_CORRUPT;

  status = profile_save_file(profile_path, profile_backup_path, &candidate);
  if (status != STORE_OK) return status;

  ProfileData verified;
  status = profile_load_file(profile_path, &verified);
  if (status != STORE_OK) return status;
  if (!profiles_equal(&candidate, &verified)) return STORE_CORRUPT;

  *profile = verified;
  if (migrated) *migrated = imported_any;
  return STORE_OK;
}
