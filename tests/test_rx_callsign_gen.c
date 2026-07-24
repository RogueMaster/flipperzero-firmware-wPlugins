#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_rx_rng.h"
#include "mf_callsign_gen.h"

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
    mf_rx_rng_init(&rng, 1U);
    CHECK(mf_rx_rng_bounded(&rng, 0x80000001UL) == 499951812U);
    CHECK(rng.state == 2647435461U);
    for(uint32_t bound = 2U; bound < 100U; bound++)
        CHECK(mf_rx_rng_bounded(&rng, bound) < bound);
    unsigned lengths[7] = {0};
    mf_rx_rng_init(&rng, 0x12345678U);
    for(unsigned i = 0U; i < 20000U; i++) lengths[mf_callsign_pick_length(&rng)]++;
    CHECK(lengths[4] > 4500U && lengths[4] < 5500U);
    CHECK(lengths[5] > 9000U && lengths[5] < 11000U);
    CHECK(lengths[6] > 4500U && lengths[6] < 5500U);
    MfCallsignGen gen;
    MfCallsign call;
    mf_callsign_gen_init(&gen);
    mf_rx_rng_init(&rng, 123U);
    for(uint8_t len = 4U; len <= 6U; len++) {
        for(unsigned i = 0U; i < 10000U; i++) {
            char previous[MF_CALLSIGN_PREFIX_MAX + 1U];
            uint8_t previous_len = gen.last_prefix_len;
            for(uint8_t j = 0U; j <= previous_len; j++) previous[j] = gen.last_prefix[j];
            CHECK(mf_callsign_generate(&gen, &rng, len, &call));
            CHECK(mf_callsign_valid(&call, len));
            CHECK(call.prefix_len != previous_len ||
                  previous_len == 0U ||
                  memcmp(call.prefix, previous, previous_len) != 0);
        }
    }
    printf("test_rx_callsign_gen: %u checks passed\n", checks);
    return 0;
}
