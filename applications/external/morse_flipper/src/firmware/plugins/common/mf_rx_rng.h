#pragma once

#include <stdint.h>

typedef struct {
    uint32_t state;
} MfRxRng;

void mf_rx_rng_init(MfRxRng* rng, uint32_t seed);
uint32_t mf_rx_rng_next(MfRxRng* rng);
uint32_t mf_rx_rng_bounded(MfRxRng* rng, uint32_t bound);
