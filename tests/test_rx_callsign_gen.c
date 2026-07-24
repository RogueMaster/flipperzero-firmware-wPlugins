#include <assert.h>
#include <stdio.h>

#include "mf_rx_rng.h"

int main(void) {
    MfRxRng rng;
    unsigned checks = 0U;
#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)
    mf_rx_rng_init(&rng, 1U);
    CHECK(mf_rx_rng_next(&rng) == 270369U);
    CHECK(mf_rx_rng_next(&rng) == 67634689U);
    CHECK(mf_rx_rng_next(&rng) == 2647435461U);
    CHECK(mf_rx_rng_next(&rng) == 307599695U);
    CHECK(mf_rx_rng_next(&rng) == 2398689233U);
    mf_rx_rng_init(&rng, 0U);
    CHECK(rng.state != 0U);
    CHECK(mf_rx_rng_bounded(&rng, 0U) == 0U);
    CHECK(mf_rx_rng_bounded(&rng, 1U) == 0U);
    for(uint32_t bound = 2U; bound < 100U; bound++)
        CHECK(mf_rx_rng_bounded(&rng, bound) < bound);
    printf("test_rx_callsign_gen: %u checks passed\n", checks);
    return 0;
}
