#include "seed.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
  uint64_t direct = 0;
  assert(seed_system_random_u64(&direct));
  assert(!seed_system_random_u64(NULL));

  uint64_t first = seed_random_u64();
  bool differs = false;
  for (int i = 0; i < 32; ++i) {
    if (seed_random_u64() != first) differs = true;
  }
  assert(differs);
  puts("system seed tests passed");
  return 0;
}
