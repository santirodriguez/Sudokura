#ifndef SUDOKURA_SEED_H
#define SUDOKURA_SEED_H

#include <stdbool.h>
#include <stdint.h>

bool seed_system_random_u64(uint64_t *value_out);
uint64_t seed_random_u64(void);

#endif
