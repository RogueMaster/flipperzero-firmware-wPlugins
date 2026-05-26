#pragma once

#include "include/domain/anti_repeat.h"
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t (*RngFn)(void *ctx);

/* Picks a random id in [0, count) that is not in `seen`. If every id is seen,
 * resets `seen` and picks again (sets *reset_happened to true).
 * Returns false if count == 0. */
bool question_pool_next(uint32_t count, AntiRepeat *seen, RngFn rng, void *rng_ctx,
                        uint32_t *id_out, bool *reset_happened);
