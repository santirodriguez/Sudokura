#ifndef SUDOKURA_STORE_STATUS_H
#define SUDOKURA_STORE_STATUS_H

#include <stdbool.h>

/* Optional owner-thread cancellation between complete, checked operations.
   A callback must not enter another writer. Cancellation retains durable
   checkpoints; resume through the same canonical load/migration functions. */
typedef bool (*StoreContinue)(void *context);

typedef enum {
  STORE_OK = 0,
  STORE_NOT_FOUND,
  STORE_CORRUPT,
  STORE_INCOMPATIBLE,
  STORE_IO_ERROR,
  STORE_CANCELLED
} StoreStatus;

#endif
