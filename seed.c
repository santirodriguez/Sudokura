#include "seed.h"

#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#else
#include <errno.h>
#include <sys/random.h>
#endif

static uint64_t mix64(uint64_t value) {
  value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31);
}

bool seed_system_random_u64(uint64_t *value_out) {
  if (!value_out) return false;
#if defined(_WIN32)
  typedef LONG (WINAPI *BCryptGenRandomFn)(void *, unsigned char *,
                                           unsigned long, unsigned long);
  HMODULE bcrypt = LoadLibraryA("bcrypt.dll");
  if (!bcrypt) return false;
  FARPROC procedure = GetProcAddress(bcrypt, "BCryptGenRandom");
  BCryptGenRandomFn generate = NULL;
  _Static_assert(sizeof(generate) == sizeof(procedure),
                 "Windows function pointers must have matching sizes");
  memcpy(&generate, &procedure, sizeof(generate));
  bool ok = false;
  if (generate) {
    enum { BCRYPT_USE_SYSTEM_PREFERRED_RNG_LOCAL = 0x00000002 };
    ok = generate(NULL, (unsigned char *)value_out,
                  (unsigned long)sizeof(*value_out),
                  BCRYPT_USE_SYSTEM_PREFERRED_RNG_LOCAL) >= 0;
  }
  FreeLibrary(bcrypt);
  return ok;
#elif defined(__APPLE__)
  arc4random_buf(value_out, sizeof(*value_out));
  return true;
#else
  unsigned char *output = (unsigned char *)value_out;
  size_t offset = 0;
  while (offset < sizeof(*value_out)) {
    ssize_t count = getrandom(output + offset, sizeof(*value_out) - offset, 0);
    if (count > 0) {
      offset += (size_t)count;
      continue;
    }
    if (count < 0 && errno == EINTR) continue;
    return false;
  }
  return true;
#endif
}

uint64_t seed_random_u64(void) {
  uint64_t value = 0;
  if (seed_system_random_u64(&value)) return value;

  static uint64_t sequence = UINT64_C(0x9e3779b97f4a7c15);
  sequence += UINT64_C(0x9e3779b97f4a7c15);
  uint64_t wall = (uint64_t)time(NULL);
  uint64_t ticks = (uint64_t)clock();
  uint64_t address = (uint64_t)(uintptr_t)&sequence;
  return mix64((wall << 32) ^ ticks ^ sequence ^ address);
}
