#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "store_io.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static unsigned long store_temp_counter = 0;

#ifdef SUDOKURA_STORE_TESTING
static StoreTestFault store_test_fault = STORE_TEST_FAULT_NONE;

void store_test_set_fault(StoreTestFault fault) {
  store_test_fault = fault;
}
#endif

static bool store_fault_is(int fault) {
#ifdef SUDOKURA_STORE_TESTING
  return (int)store_test_fault == fault;
#else
  (void)fault;
  return false;
#endif
}

#if defined(_WIN32)
static bool utf8_to_wide(const char *input, wchar_t *output,
                         size_t output_count) {
  if (!input || !output || output_count == 0) return false;
  int required =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, NULL, 0);
  if (required <= 0 || (size_t)required > output_count) return false;
  return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output,
                             required) == required;
}

static FILE *store_fopen(const char *path, const wchar_t *mode) {
  wchar_t wide_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!utf8_to_wide(path, wide_path,
                    sizeof(wide_path) / sizeof(wide_path[0])))
    return NULL;
  return _wfopen(wide_path, mode);
}
#else
static FILE *store_fopen(const char *path, const char *mode) {
  return fopen(path, mode);
}
#endif

static bool sync_file(FILE *file) {
  if (!file || fflush(file) != 0) return false;
#ifdef SUDOKURA_STORE_TESTING
  if (store_fault_is(STORE_TEST_FAULT_SYNC)) return false;
#endif
#if defined(_WIN32)
  return _commit(_fileno(file)) == 0;
#else
  return fsync(fileno(file)) == 0;
#endif
}

static bool build_child_path(char out[SUDOKURA_STORE_PATH_CAPACITY],
                             const char *directory, const char *leaf) {
  if (!out || !directory || !leaf) return false;
  size_t length = strlen(directory);
  bool separator =
      length > 0 && directory[length - 1] != '/' && directory[length - 1] != '\\';
  int written = snprintf(out, SUDOKURA_STORE_PATH_CAPACITY, "%s%s%s", directory,
                         separator ? "/" : "", leaf);
  return written >= 0 && (size_t)written < SUDOKURA_STORE_PATH_CAPACITY;
}

static bool build_temporary_path(char out[SUDOKURA_STORE_PATH_CAPACITY],
                                 const char *path) {
  if (!out || !path) return false;
#if defined(_WIN32)
  unsigned long process_id = (unsigned long)GetCurrentProcessId();
#else
  unsigned long process_id = (unsigned long)getpid();
#endif
  ++store_temp_counter;
  int written = snprintf(out, SUDOKURA_STORE_PATH_CAPACITY, "%s.tmp.%lu.%lu",
                         path, process_id, store_temp_counter);
  return written >= 0 && (size_t)written < SUDOKURA_STORE_PATH_CAPACITY;
}

static bool sync_parent_directory(const char *path) {
#if defined(_WIN32)
  (void)path;
  return true;
#else
  if (!path || !path[0]) return false;
  char directory[SUDOKURA_STORE_PATH_CAPACITY];
  size_t length = strlen(path);
  if (length >= sizeof(directory)) return false;
  memcpy(directory, path, length + 1);
  char *slash = strrchr(directory, '/');
  if (slash) {
    if (slash == directory)
      slash[1] = '\0';
    else
      *slash = '\0';
  } else {
    memcpy(directory, ".", 2);
  }
#ifdef O_DIRECTORY
  int fd = open(directory, O_RDONLY | O_DIRECTORY);
#else
  int fd = open(directory, O_RDONLY);
#endif
  if (fd < 0) return false;
  bool ok = fsync(fd) == 0;
  close(fd);
  return ok;
#endif
}

static bool replace_file(const char *temporary, const char *path) {
#ifdef SUDOKURA_STORE_TESTING
  if (store_fault_is(STORE_TEST_FAULT_REPLACE)) return false;
#endif
#if defined(_WIN32)
  wchar_t wide_temporary[SUDOKURA_STORE_PATH_CAPACITY];
  wchar_t wide_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!utf8_to_wide(temporary, wide_temporary,
                    sizeof(wide_temporary) / sizeof(wide_temporary[0])) ||
      !utf8_to_wide(path, wide_path,
                    sizeof(wide_path) / sizeof(wide_path[0])))
    return false;
  return MoveFileExW(wide_temporary, wide_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return rename(temporary, path) == 0;
#endif
}

bool store_file_exists(const char *path) {
  if (!path || !path[0]) return false;
#if defined(_WIN32)
  wchar_t wide_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!utf8_to_wide(path, wide_path,
                    sizeof(wide_path) / sizeof(wide_path[0])))
    return false;
  DWORD attributes = GetFileAttributesW(wide_path);
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
  struct stat info;
  return stat(path, &info) == 0 && S_ISREG(info.st_mode);
#endif
}

bool store_remove_file(const char *path) {
  if (!path || !path[0]) return false;
#if defined(_WIN32)
  wchar_t wide_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!utf8_to_wide(path, wide_path,
                    sizeof(wide_path) / sizeof(wide_path[0])))
    return false;
  return DeleteFileW(wide_path) != 0;
#else
  return unlink(path) == 0;
#endif
}

StoreStatus store_read_file(const char *path, unsigned char *data,
                            size_t capacity, size_t *size_out) {
  if (!path || !path[0] || !data || capacity == 0)
    return STORE_IO_ERROR;

  errno = 0;
#if defined(_WIN32)
  wchar_t wide_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!utf8_to_wide(path, wide_path,
                    sizeof(wide_path) / sizeof(wide_path[0])))
    return STORE_IO_ERROR;
  DWORD attributes = GetFileAttributesW(wide_path);
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
               ? STORE_NOT_FOUND
               : STORE_IO_ERROR;
  }
  FILE *file = store_fopen(path, L"rb");
#else
  FILE *file = store_fopen(path, "rb");
#endif
  if (!file) {
#if defined(_WIN32)
    return STORE_IO_ERROR;
#else
    return errno == ENOENT ? STORE_NOT_FOUND : STORE_IO_ERROR;
#endif
  }

  size_t size = fread(data, 1, capacity, file);
  bool read_error = ferror(file) != 0;
  int extra = EOF;
  if (!read_error && size == capacity) extra = fgetc(file);
  if (fclose(file) != 0 || read_error) return STORE_IO_ERROR;
  if (size == capacity && extra != EOF) return STORE_CORRUPT;
  if (size_out) *size_out = size;
  return STORE_OK;
}

static bool write_temporary(const char *temporary,
                            const unsigned char *data, size_t size) {
#if defined(_WIN32)
  wchar_t wide_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!utf8_to_wide(temporary, wide_path,
                    sizeof(wide_path) / sizeof(wide_path[0])))
    return false;
  int fd = _wopen(wide_path, _O_BINARY | _O_WRONLY | _O_CREAT | _O_EXCL,
                  _S_IREAD | _S_IWRITE);
  if (fd < 0) return false;
  FILE *file = _fdopen(fd, "wb");
#else
  int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return false;
  FILE *file = fdopen(fd, "wb");
#endif
  if (!file) {
#if defined(_WIN32)
    _close(fd);
#else
    close(fd);
#endif
    return false;
  }

  size_t target = size;
#ifdef SUDOKURA_STORE_TESTING
  if (store_fault_is(STORE_TEST_FAULT_DURING_WRITE) && target > 1)
    target /= 2;
#endif
  bool ok = fwrite(data, 1, target, file) == target;
#ifdef SUDOKURA_STORE_TESTING
  if (store_fault_is(STORE_TEST_FAULT_DURING_WRITE)) ok = false;
#endif
  if (ok) ok = sync_file(file);
  if (fclose(file) != 0) ok = false;
  return ok;
}

static StoreStatus write_atomic_internal(const char *path,
                                         const unsigned char *data,
                                         size_t size) {
  if (!path || !path[0] || !data || size == 0 ||
      size > SUDOKURA_STORE_IO_MAX_FILE_SIZE)
    return STORE_IO_ERROR;

  char temporary[SUDOKURA_STORE_PATH_CAPACITY];
  if (!build_temporary_path(temporary, path)) return STORE_IO_ERROR;

  if (!write_temporary(temporary, data, size)) {
    (void)store_remove_file(temporary);
    return STORE_IO_ERROR;
  }

  if (!replace_file(temporary, path)) {
    (void)store_remove_file(temporary);
    return STORE_IO_ERROR;
  }

  return sync_parent_directory(path) ? STORE_OK : STORE_IO_ERROR;
}

StoreStatus store_copy_once(const char *source, const char *destination) {
  if (!source || !destination) return STORE_IO_ERROR;
  if (store_file_exists(destination)) return STORE_OK;

  unsigned char data[SUDOKURA_STORE_IO_MAX_FILE_SIZE];
  size_t size = 0;
  StoreStatus status = store_read_file(source, data, sizeof(data), &size);
  if (status != STORE_OK) return status;
  if (size == 0) return STORE_CORRUPT;
  return write_atomic_internal(destination, data, size);
}

StoreStatus store_atomic_write(const char *path, const char *backup_path,
                               const unsigned char *data, size_t size) {
  if (!path || !data || size == 0) return STORE_IO_ERROR;

  if (backup_path && backup_path[0] && store_file_exists(path)) {
    unsigned char previous[SUDOKURA_STORE_IO_MAX_FILE_SIZE];
    size_t previous_size = 0;
    StoreStatus status =
        store_read_file(path, previous, sizeof(previous), &previous_size);
    if (status != STORE_OK || previous_size == 0) return STORE_IO_ERROR;
    status = write_atomic_internal(backup_path, previous, previous_size);
    if (status != STORE_OK) return status;
  }
  return write_atomic_internal(path, data, size);
}

StoreLockStatus store_writer_lock_acquire(const char *directory,
                                          StoreWriterLock *lock) {
  if (!directory || !directory[0] || !lock) return STORE_LOCK_ERROR;
  memset(lock, 0, sizeof(*lock));
  lock->native_handle = -1;

  char path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!build_child_path(path, directory, "profile.lock"))
    return STORE_LOCK_ERROR;

#if defined(_WIN32)
  wchar_t wide_path[SUDOKURA_STORE_PATH_CAPACITY];
  if (!utf8_to_wide(path, wide_path,
                    sizeof(wide_path) / sizeof(wide_path[0])))
    return STORE_LOCK_ERROR;
  HANDLE handle = CreateFileW(wide_path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    return error == ERROR_SHARING_VIOLATION ? STORE_LOCK_BUSY : STORE_LOCK_ERROR;
  }
  lock->native_handle = (intptr_t)handle;
#else
  int fd = open(path, O_RDWR | O_CREAT, 0600);
  if (fd < 0) return STORE_LOCK_ERROR;
  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    int error = errno;
    close(fd);
    return error == EWOULDBLOCK || error == EAGAIN ? STORE_LOCK_BUSY
                                                    : STORE_LOCK_ERROR;
  }
  lock->native_handle = (intptr_t)fd;
#endif

  lock->held = true;
  return STORE_LOCK_ACQUIRED;
}

void store_writer_lock_release(StoreWriterLock *lock) {
  if (!lock || !lock->held) return;
#if defined(_WIN32)
  CloseHandle((HANDLE)lock->native_handle);
#else
  close((int)lock->native_handle);
#endif
  lock->native_handle = -1;
  lock->held = false;
}
