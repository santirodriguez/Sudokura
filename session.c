#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "session.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#define STORE_HEADER_SIZE 18u
#define STORE_MAX_FILE_SIZE 4096u
#define SESSION_PAYLOAD_SIZE 365u
#define PREFERENCES_PAYLOAD_SIZE 5u
#define NOTES_VALID_MASK UINT16_C(0x03fe)
#define SESSION_MAX_COUNTER 1000000

static const unsigned char session_magic[8] = {'S','U','D','O','S','A','V','E'};
static const unsigned char preferences_magic[8] = {'S','U','D','O','P','R','E','F'};

typedef struct {
  unsigned char *data;
  size_t size;
  size_t position;
  bool ok;
} ByteWriter;

typedef struct {
  const unsigned char *data;
  size_t size;
  size_t position;
  bool ok;
} ByteReader;

static bool valid_mode(GameMode mode) {
  return mode >= MODE_CLASSIC && mode <= MODE_TIME;
}

static bool valid_difficulty(GameDifficulty difficulty) {
  return difficulty >= DIFFICULTY_EASY && difficulty < DIFFICULTY_COUNT;
}

static bool valid_status(SessionStatus status) {
  return status >= SESSION_ACTIVE && status <= SESSION_LOST;
}

static void writer_u8(ByteWriter *writer, uint8_t value) {
  if (!writer || !writer->ok || writer->position >= writer->size) {
    if (writer) writer->ok = false;
    return;
  }
  writer->data[writer->position++] = value;
}

static void writer_u16(ByteWriter *writer, uint16_t value) {
  writer_u8(writer, (uint8_t)(value & 0xffu));
  writer_u8(writer, (uint8_t)((value >> 8) & 0xffu));
}

static void writer_u32(ByteWriter *writer, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    writer_u8(writer, (uint8_t)((value >> shift) & 0xffu));
  }
}

static void writer_u64(ByteWriter *writer, uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) {
    writer_u8(writer, (uint8_t)((value >> shift) & UINT64_C(0xff)));
  }
}

static uint8_t reader_u8(ByteReader *reader) {
  if (!reader || !reader->ok || reader->position >= reader->size) {
    if (reader) reader->ok = false;
    return 0;
  }
  return reader->data[reader->position++];
}

static uint16_t reader_u16(ByteReader *reader) {
  uint16_t value = reader_u8(reader);
  value |= (uint16_t)((uint16_t)reader_u8(reader) << 8);
  return value;
}

static uint32_t reader_u32(ByteReader *reader) {
  uint32_t value = 0;
  for (unsigned shift = 0; shift < 32; shift += 8) {
    value |= (uint32_t)reader_u8(reader) << shift;
  }
  return value;
}

static uint64_t reader_u64(ByteReader *reader) {
  uint64_t value = 0;
  for (unsigned shift = 0; shift < 64; shift += 8) {
    value |= (uint64_t)reader_u8(reader) << shift;
  }
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
  for (unsigned shift = 0; shift < 32; shift += 8) {
    data[shift / 8] = (unsigned char)((value >> shift) & 0xffu);
  }
}

static uint16_t get_u16(const unsigned char *data) {
  return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t get_u32(const unsigned char *data) {
  uint32_t value = 0;
  for (unsigned shift = 0; shift < 32; shift += 8) {
    value |= (uint32_t)data[shift / 8] << shift;
  }
  return value;
}

static bool sync_file(FILE *file) {
  if (!file || fflush(file) != 0) {
    return false;
  }
#if defined(_WIN32)
  return _commit(_fileno(file)) == 0;
#else
  return fsync(fileno(file)) == 0;
#endif
}

static bool atomic_replace(const char *temporary, const char *path) {
#if defined(_WIN32)
  return MoveFileExA(temporary, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return rename(temporary, path) == 0;
#endif
}

static bool write_container(const char *path, const unsigned char magic[8],
                            const unsigned char *payload, size_t payload_size) {
  if (!path || !path[0] || !magic || !payload || payload_size > UINT32_MAX) {
    return false;
  }

  size_t path_length = strlen(path);
  if (path_length > STORE_MAX_FILE_SIZE - 16) {
    return false;
  }
  char temporary[STORE_MAX_FILE_SIZE];
  int written = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
  if (written < 0 || (size_t)written >= sizeof(temporary)) {
    return false;
  }

  unsigned char header[STORE_HEADER_SIZE];
  memcpy(header, magic, 8);
  put_u16(header + 8, (uint16_t)SUDOKURA_SAVE_FORMAT_VERSION);
  put_u32(header + 10, (uint32_t)payload_size);
  put_u32(header + 14, crc32_bytes(payload, payload_size));

  FILE *file = fopen(temporary, "wb");
  if (!file) {
    return false;
  }
  bool ok = fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
            fwrite(payload, 1, payload_size, file) == payload_size &&
            sync_file(file);
  if (fclose(file) != 0) {
    ok = false;
  }
  if (ok) {
    ok = atomic_replace(temporary, path);
  }
  if (!ok) {
    remove(temporary);
  }
  return ok;
}

static StoreStatus read_container(const char *path,
                                  const unsigned char expected_magic[8],
                                  unsigned char *payload,
                                  size_t expected_payload_size) {
  if (!path || !path[0] || !expected_magic || !payload) {
    return STORE_IO_ERROR;
  }
  errno = 0;
  FILE *file = fopen(path, "rb");
  if (!file) {
    return errno == ENOENT ? STORE_NOT_FOUND : STORE_IO_ERROR;
  }

  unsigned char data[STORE_MAX_FILE_SIZE];
  size_t size = fread(data, 1, sizeof(data), file);
  bool read_error = ferror(file) != 0;
  int extra = 0;
  if (!read_error && size == sizeof(data)) {
    extra = fgetc(file);
  }
  if (fclose(file) != 0 || read_error) {
    return STORE_IO_ERROR;
  }
  if (size == sizeof(data) && extra != EOF) {
    return STORE_CORRUPT;
  }
  if (size < STORE_HEADER_SIZE || memcmp(data, expected_magic, 8) != 0) {
    return STORE_CORRUPT;
  }
  uint16_t version = get_u16(data + 8);
  uint32_t payload_size = get_u32(data + 10);
  uint32_t stored_crc = get_u32(data + 14);
  if (version != SUDOKURA_SAVE_FORMAT_VERSION ||
      payload_size != expected_payload_size ||
      size != STORE_HEADER_SIZE + (size_t)payload_size) {
    return STORE_CORRUPT;
  }
  const unsigned char *source = data + STORE_HEADER_SIZE;
  if (crc32_bytes(source, payload_size) != stored_crc) {
    return STORE_CORRUPT;
  }
  memcpy(payload, source, payload_size);
  return STORE_OK;
}

void preferences_defaults(Preferences *preferences) {
  if (!preferences) {
    return;
  }
  preferences->language = LANG_EN;
  preferences->dark_theme = true;
  preferences->strict_mode = false;
  preferences->mode = MODE_CLASSIC;
  preferences->difficulty = DIFFICULTY_MEDIUM;
}

bool preferences_validate(const Preferences *preferences) {
  return preferences && preferences->language >= LANG_EN &&
         preferences->language < LANG_COUNT && valid_mode(preferences->mode) &&
         valid_difficulty(preferences->difficulty);
}

static bool game_matches_canonical(const Game *game, const Game *canonical) {
  return game && canonical && game->seed == canonical->seed &&
         game->generator_revision == canonical->generator_revision &&
         game->difficulty == canonical->difficulty &&
         game->difficulty_score == canonical->difficulty_score &&
         memcmp(game->solution, canonical->solution, sizeof(game->solution)) == 0 &&
         memcmp(game->initial, canonical->initial, sizeof(game->initial)) == 0 &&
         memcmp(game->fixed, canonical->fixed, sizeof(game->fixed)) == 0;
}

bool session_validate(const SessionState *session) {
  if (!session || !valid_mode(session->mode) ||
      !valid_difficulty(session->game.difficulty) ||
      !valid_status(session->status) || session->selected_row < 0 ||
      session->selected_row >= 9 || session->selected_column < 0 ||
      session->selected_column >= 9 || session->mistakes < 0 ||
      session->mistakes > SESSION_MAX_COUNTER || session->strikes < 0 ||
      session->strikes > SESSION_MAX_COUNTER ||
      session->game.generator_revision != SUDOKURA_GENERATOR_REVISION) {
    return false;
  }

  Game canonical;
  game_new_difficulty(&canonical, session->game.seed, session->game.difficulty);
  if (!game_matches_canonical(&session->game, &canonical)) {
    return false;
  }

  for (int i = 0; i < 81; ++i) {
    int value = session->game.puzzle[i];
    uint16_t notes = session->game.notes[i];
    if (value < 0 || value > 9 || session->game.hinted[i] > 1 ||
        (notes & (uint16_t)~NOTES_VALID_MASK) != 0) {
      return false;
    }
    if (canonical.fixed[i]) {
      if (value != canonical.initial[i] || session->game.hinted[i] || notes) {
        return false;
      }
    } else if (session->game.hinted[i]) {
      if (value != canonical.solution[i] || notes) {
        return false;
      }
    } else if (value != 0 && notes != 0) {
      return false;
    }
  }

  if (session->is_daily) {
    uint64_t daily_seed = 0;
    if (session->mode != MODE_CLASSIC ||
        session->game.difficulty != DIFFICULTY_MEDIUM ||
        !game_daily_seed(session->daily_year, session->daily_month,
                         session->daily_day, &daily_seed) ||
        daily_seed != session->game.seed) {
      return false;
    }
  } else if (session->daily_year != 0 || session->daily_month != 0 ||
             session->daily_day != 0) {
    return false;
  }

  bool solved = game_is_solved(&session->game);
  bool lost = game_mode_lost(session->mode, session->strikes, 3,
                             (double)session->elapsed_ms / 1000.0,
                             session->mode == MODE_TIME ? 600.0 : 0.0);
  if (session->status == SESSION_ACTIVE) {
    return !solved && !lost;
  }
  if (session->status == SESSION_WON) {
    return solved;
  }
  return !solved && lost;
}

bool preferences_save_file(const char *path, const Preferences *preferences) {
  if (!preferences_validate(preferences)) {
    return false;
  }
  unsigned char payload[PREFERENCES_PAYLOAD_SIZE];
  ByteWriter writer = {payload, sizeof(payload), 0, true};
  writer_u8(&writer, (uint8_t)preferences->language);
  writer_u8(&writer, preferences->dark_theme ? 1u : 0u);
  writer_u8(&writer, preferences->strict_mode ? 1u : 0u);
  writer_u8(&writer, (uint8_t)preferences->mode);
  writer_u8(&writer, (uint8_t)preferences->difficulty);
  return writer.ok && writer.position == sizeof(payload) &&
         write_container(path, preferences_magic, payload, sizeof(payload));
}

StoreStatus preferences_load_file(const char *path, Preferences *preferences) {
  if (!preferences) {
    return STORE_IO_ERROR;
  }
  unsigned char payload[PREFERENCES_PAYLOAD_SIZE];
  StoreStatus status = read_container(path, preferences_magic, payload,
                                      sizeof(payload));
  if (status != STORE_OK) {
    return status;
  }

  ByteReader reader = {payload, sizeof(payload), 0, true};
  Preferences loaded;
  loaded.language = (Language)reader_u8(&reader);
  uint8_t dark = reader_u8(&reader);
  uint8_t strict = reader_u8(&reader);
  loaded.mode = (GameMode)reader_u8(&reader);
  loaded.difficulty = (GameDifficulty)reader_u8(&reader);
  loaded.dark_theme = dark != 0;
  loaded.strict_mode = strict != 0;
  if (!reader.ok || reader.position != sizeof(payload) || dark > 1 ||
      strict > 1 || !preferences_validate(&loaded)) {
    return STORE_CORRUPT;
  }
  *preferences = loaded;
  return STORE_OK;
}

bool session_save_file(const char *path, const SessionState *session) {
  if (!session_validate(session)) {
    return false;
  }

  unsigned char payload[SESSION_PAYLOAD_SIZE];
  ByteWriter writer = {payload, sizeof(payload), 0, true};
  writer_u32(&writer, session->game.generator_revision);
  writer_u64(&writer, session->game.seed);
  writer_u8(&writer, (uint8_t)session->game.difficulty);
  writer_u8(&writer, (uint8_t)session->mode);
  writer_u8(&writer, (uint8_t)session->selected_row);
  writer_u8(&writer, (uint8_t)session->selected_column);
  writer_u8(&writer, session->notes_mode ? 1u : 0u);
  writer_u8(&writer, session->strict_mode ? 1u : 0u);
  writer_u8(&writer, session->manual_paused ? 1u : 0u);
  writer_u8(&writer, (uint8_t)session->status);
  writer_u32(&writer, (uint32_t)session->mistakes);
  writer_u32(&writer, (uint32_t)session->strikes);
  writer_u64(&writer, session->elapsed_ms);
  writer_u8(&writer, session->is_daily ? 1u : 0u);
  writer_u16(&writer, (uint16_t)session->daily_year);
  writer_u8(&writer, (uint8_t)session->daily_month);
  writer_u8(&writer, (uint8_t)session->daily_day);
  for (int i = 0; i < 81; ++i) writer_u8(&writer, (uint8_t)session->game.puzzle[i]);
  for (int i = 0; i < 81; ++i) writer_u8(&writer, session->game.hinted[i]);
  for (int i = 0; i < 81; ++i) writer_u16(&writer, session->game.notes[i]);

  return writer.ok && writer.position == sizeof(payload) &&
         write_container(path, session_magic, payload, sizeof(payload));
}

StoreStatus session_load_file(const char *path, SessionState *session) {
  if (!session) {
    return STORE_IO_ERROR;
  }
  unsigned char payload[SESSION_PAYLOAD_SIZE];
  StoreStatus status = read_container(path, session_magic, payload,
                                      sizeof(payload));
  if (status != STORE_OK) {
    return status;
  }

  ByteReader reader = {payload, sizeof(payload), 0, true};
  uint32_t generator_revision = reader_u32(&reader);
  uint64_t seed = reader_u64(&reader);
  GameDifficulty difficulty = (GameDifficulty)reader_u8(&reader);
  GameMode mode = (GameMode)reader_u8(&reader);
  int selected_row = reader_u8(&reader);
  int selected_column = reader_u8(&reader);
  uint8_t notes_mode = reader_u8(&reader);
  uint8_t strict_mode = reader_u8(&reader);
  uint8_t manual_paused = reader_u8(&reader);
  SessionStatus session_status = (SessionStatus)reader_u8(&reader);
  uint32_t mistakes = reader_u32(&reader);
  uint32_t strikes = reader_u32(&reader);
  uint64_t elapsed_ms = reader_u64(&reader);
  uint8_t is_daily = reader_u8(&reader);
  int daily_year = reader_u16(&reader);
  int daily_month = reader_u8(&reader);
  int daily_day = reader_u8(&reader);

  if (!reader.ok || generator_revision != SUDOKURA_GENERATOR_REVISION ||
      !valid_difficulty(difficulty) || !valid_mode(mode) ||
      notes_mode > 1 || strict_mode > 1 || manual_paused > 1 ||
      is_daily > 1 || mistakes > SESSION_MAX_COUNTER ||
      strikes > SESSION_MAX_COUNTER) {
    return STORE_CORRUPT;
  }

  SessionState loaded;
  memset(&loaded, 0, sizeof(loaded));
  game_new_difficulty(&loaded.game, seed, difficulty);
  loaded.mode = mode;
  loaded.selected_row = selected_row;
  loaded.selected_column = selected_column;
  loaded.notes_mode = notes_mode != 0;
  loaded.strict_mode = strict_mode != 0;
  loaded.manual_paused = manual_paused != 0;
  loaded.status = session_status;
  loaded.mistakes = (int)mistakes;
  loaded.strikes = (int)strikes;
  loaded.elapsed_ms = elapsed_ms;
  loaded.is_daily = is_daily != 0;
  loaded.daily_year = daily_year;
  loaded.daily_month = daily_month;
  loaded.daily_day = daily_day;

  for (int i = 0; i < 81; ++i) loaded.game.puzzle[i] = reader_u8(&reader);
  for (int i = 0; i < 81; ++i) loaded.game.hinted[i] = reader_u8(&reader);
  for (int i = 0; i < 81; ++i) loaded.game.notes[i] = reader_u16(&reader);

  if (!reader.ok || reader.position != sizeof(payload) ||
      !session_validate(&loaded)) {
    return STORE_CORRUPT;
  }
  *session = loaded;
  return STORE_OK;
}

static bool file_exists(const char *path) {
#if defined(_WIN32)
  DWORD attributes = GetFileAttributesA(path);
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
  return access(path, F_OK) == 0;
#endif
}

static bool move_without_replace(const char *source, const char *destination) {
#if defined(_WIN32)
  return MoveFileExA(source, destination, MOVEFILE_WRITE_THROUGH) != 0;
#else
  return rename(source, destination) == 0;
#endif
}

bool store_quarantine_corrupt(const char *path) {
  if (!path || !path[0] || !file_exists(path)) {
    return false;
  }
  char destination[STORE_MAX_FILE_SIZE];
  for (int suffix = 0; suffix < 100; ++suffix) {
    int written = suffix == 0
                      ? snprintf(destination, sizeof(destination), "%s.corrupt", path)
                      : snprintf(destination, sizeof(destination), "%s.corrupt.%d",
                                 path, suffix);
    if (written < 0 || (size_t)written >= sizeof(destination)) {
      return false;
    }
    if (!file_exists(destination) && move_without_replace(path, destination)) {
      return true;
    }
  }
  return false;
}
