#include "mf_rx_rng.h"

#include <stddef.h>

#define MF_RX_RNG_FALLBACK_SEED 0xA341316CUL

void mf_rx_rng_init(MfRxRng* rng, uint32_t seed) {
    if(rng != NULL) rng->state = seed == 0U ? MF_RX_RNG_FALLBACK_SEED : seed;
}

uint32_t mf_rx_rng_next(MfRxRng* rng) {
    uint32_t value;
    if(rng == NULL) return 0U;
    value = rng->state;
    if(value == 0U) value = MF_RX_RNG_FALLBACK_SEED;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    rng->state = value;
    return value;
}

uint32_t mf_rx_rng_bounded(MfRxRng* rng, uint32_t bound) {
    uint32_t value;
    uint32_t threshold;
    if(rng == NULL || bound == 0U) return 0U;
    threshold = (uint32_t)(0U - bound) % bound;
    do {
        value = mf_rx_rng_next(rng);
    } while(value < threshold);
    return value % bound;
}
