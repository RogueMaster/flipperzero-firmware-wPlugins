#pragma once

#include <stdint.h>

/* Deterministic generator so test failures can be reproduced */
uint32_t furi_hal_random_get(void);
void shim_random_seed(uint32_t seed);
