#define SUDOKURA_STORE_TESTING 1
#include "store_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static const char *active_path = ".sudokura-storage-test.dat";
static const char *backup_path = ".sudokura-storage-test.dat.bak";
static char executable_unicode_path[SUDOKURA_STORE_PATH_CAPACITY];
static const char *profile_directory = ".sudokura-profile-path-test";
static const char *profile_unicode_path =
    ".sudokura-profile-path-test/profile-\xc3\xb1.dat";
#if !defined(_WIN32)
static const char *readonly_directory = ".sudokura-readonly-test";
static const char *readonly_path = ".sudokura-readonly-test/data.dat";
#endif

static void cleanup(void) {
  (void)store_remove_file(active_path);
  (void)store_remove_file(backup_path);
  if (executable_unicode_path[0])
    (void)store_remove_file(executable_unicode_path);
  (void)store_remove_file(profile_unicode_path);
#if defined(_WIN32)
  (void)_rmdir(profile_directory);
#else
  (void)rmdir(profile_directory);
  (void)chmod(readonly_directory, 0700);
  (void)store_remove_file(readonly_path);
  (void)rmdir(readonly_directory);
#endif
  (void)store_remove_file("profile.lock");
}

static bool executable_sibling_path(
    char out[SUDOKURA_STORE_PATH_CAPACITY], const char *argv0,
    const char *leaf) {
  if (!out || !argv0 || !argv0[0] || !leaf || !leaf[0]) return false;
  const char *slash = strrchr(argv0, '/');
  const char *backslash = strrchr(argv0, '\\');
  const char *separator = slash;
  if (backslash && (!separator || backslash > separator)) separator = backslash;
  size_t prefix = separator ? (size_t)(separator - argv0) + 1u : 0u;
  size_t leaf_length = strlen(leaf);
  if (prefix + leaf_length + 1u > SUDOKURA_STORE_PATH_CAPACITY)
    return false;
  if (prefix) memcpy(out, argv0, prefix);
  memcpy(out + prefix, leaf, leaf_length + 1u);
  return true;
}

static void assert_contents(const char *path, const char *expected) {
  unsigned char data[128];
  size_t size = 0;
  assert(store_read_file(path, data, sizeof(data), &size) == STORE_OK);
  assert(size == strlen(expected));
  assert(memcmp(data, expected, size) == 0);
}

static void test_atomic_backup(void) {
  const unsigned char old_data[] = "old-state";
  const unsigned char new_data[] = "new-state";
  assert(store_atomic_write(active_path, backup_path, old_data,
                            sizeof(old_data) - 1) == STORE_OK);
  assert(!store_file_exists(backup_path));

  assert(store_atomic_write(active_path, backup_path, new_data,
                            sizeof(new_data) - 1) == STORE_OK);
  assert_contents(active_path, "new-state");
  assert_contents(backup_path, "old-state");
}

static void test_faults_preserve_active(void) {
  const unsigned char base[] = "known-good";
  const unsigned char update[] = "replacement";
  assert(store_atomic_write(active_path, backup_path, base,
                            sizeof(base) - 1) == STORE_OK);

  store_test_set_fault(STORE_TEST_FAULT_DURING_WRITE);
  assert(store_atomic_write(active_path, backup_path, update,
                            sizeof(update) - 1) == STORE_IO_ERROR);
  assert_contents(active_path, "known-good");

  store_test_set_fault(STORE_TEST_FAULT_SYNC);
  assert(store_atomic_write(active_path, backup_path, update,
                            sizeof(update) - 1) == STORE_IO_ERROR);
  assert_contents(active_path, "known-good");

  store_test_set_fault(STORE_TEST_FAULT_REPLACE);
  assert(store_atomic_write(active_path, backup_path, update,
                            sizeof(update) - 1) == STORE_IO_ERROR);
  assert_contents(active_path, "known-good");

  store_test_set_fault(STORE_TEST_FAULT_NONE);
}

static void test_utf8_path(void) {
  const unsigned char value[] = "utf8-path";
  assert(executable_unicode_path[0]);
  assert(store_atomic_write(executable_unicode_path, NULL, value,
                            sizeof(value) - 1) == STORE_OK);
  assert_contents(executable_unicode_path, "utf8-path");

#if defined(_WIN32)
  assert(_mkdir(profile_directory) == 0);
#else
  assert(mkdir(profile_directory, 0700) == 0);
#endif
  assert(store_atomic_write(profile_unicode_path, NULL, value,
                            sizeof(value) - 1) == STORE_OK);
  assert_contents(profile_unicode_path, "utf8-path");
}

#if !defined(_WIN32)
static void test_permission_failure(void) {
  const unsigned char value[] = "blocked";
  assert(mkdir(readonly_directory, 0700) == 0);
  assert(chmod(readonly_directory, 0500) == 0);
  assert(store_atomic_write(readonly_path, NULL, value, sizeof(value) - 1) ==
         STORE_IO_ERROR);
  assert(!store_file_exists(readonly_path));
  assert(chmod(readonly_directory, 0700) == 0);
  assert(rmdir(readonly_directory) == 0);
}
#endif

static void test_writer_lock(void) {
  StoreWriterLock first;
  StoreWriterLock second;
  assert(store_writer_lock_acquire(".", &first) == STORE_LOCK_ACQUIRED);
  assert(store_writer_lock_acquire(".", &second) == STORE_LOCK_BUSY);
  store_writer_lock_release(&first);
  assert(store_writer_lock_acquire(".", &second) == STORE_LOCK_ACQUIRED);
  store_writer_lock_release(&second);
}

int main(int argc, char **argv) {
  assert(argc > 0 && argv && argv[0]);
  assert(executable_sibling_path(executable_unicode_path, argv[0],
                                 "storage-\xc3\xb1-executable.dat"));
  cleanup();
  test_atomic_backup();
  test_faults_preserve_active();
  test_utf8_path();
#if !defined(_WIN32)
  test_permission_failure();
#endif
  test_writer_lock();
  cleanup();
  puts("recoverable storage, UTF-8 paths, fault injection, and writer lock passed");
  return 0;
}
