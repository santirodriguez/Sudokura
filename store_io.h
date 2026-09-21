#ifndef SUDOKURA_STORE_IO_H
#define SUDOKURA_STORE_IO_H

#include "store_status.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SUDOKURA_STORE_PATH_CAPACITY 4096u
#define SUDOKURA_STORE_IO_MAX_FILE_SIZE 65536u

typedef enum {
  STORE_LOCK_ACQUIRED = 0,
  STORE_LOCK_BUSY,
  STORE_LOCK_ERROR
} StoreLockStatus;

typedef struct {
  intptr_t native_handle;
  bool held;
} StoreWriterLock;

StoreStatus store_read_file(const char *path, unsigned char *data,
                            size_t capacity, size_t *size_out);
StoreStatus store_atomic_write(const char *path, const char *backup_path,
                               const unsigned char *data, size_t size);
StoreStatus store_copy_once(const char *source, const char *destination);
bool store_file_exists(const char *path);
bool store_remove_file(const char *path);

StoreLockStatus store_writer_lock_acquire(const char *directory,
                                          StoreWriterLock *lock);
void store_writer_lock_release(StoreWriterLock *lock);

#ifdef SUDOKURA_STORE_TESTING
typedef enum {
  STORE_TEST_FAULT_NONE = 0,
  STORE_TEST_FAULT_OPEN,
  STORE_TEST_FAULT_DURING_WRITE,
  STORE_TEST_FAULT_NO_SPACE,
  STORE_TEST_FAULT_SYNC,
  STORE_TEST_FAULT_REPLACE,
  STORE_TEST_FAULT_CRASH_AFTER_SYNC
} StoreTestFault;

void store_test_set_fault(StoreTestFault fault);
#endif

#endif
