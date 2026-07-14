// https://www.pcg-random.org

#include <furi_hal.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "include/pcg32.h"

void pcg32_seed(pcg32_t* rng) {
    uint8_t buffer[16];
    furi_hal_random_fill_buf(buffer, sizeof(buffer));

    memcpy(&rng->state, &buffer[0], 8);
    memcpy(&rng->inc, &buffer[8], 8);

    rng->inc |= 1;
}

uint32_t pcg32_random(pcg32_t* rng) {
    uint64_t old_state = rng->state;
    rng->state = old_state * 6364136223846793005ull + (rng->inc | 1);
    uint32_t xorshifted = ((old_state >> 18u) ^ old_state) >> 27u;
    uint32_t rot = old_state >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 32));
}

uint32_t pcg32_random_bound(pcg32_t* rng, uint32_t bound) {
    uint32_t threshold = -bound % bound;
    while(true) {
        uint32_t r = pcg32_random(rng);
        if(r >= threshold) return r % bound;
    }
}
