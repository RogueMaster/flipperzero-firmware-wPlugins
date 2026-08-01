#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mf_callsign_gen.h"
#include "mf_rx_rng.h"

#define DISTRIBUTION_SAMPLES 120000U

static unsigned checks;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)

static const uint8_t entity_weights[MfCallsignEntityCount] = {
    40U, 18U, 14U, 14U, 8U, 16U, 13U, 13U, 12U, 12U, 12U, 7U, 11U, 11U, 10U, 10U, 10U, 7U, 9U, 9U,
};

static const uint8_t entity_lengths[MfCallsignEntityCount] = {
    7U, 7U, 7U, 7U, 7U, 7U, 7U, 4U, 7U, 7U, 7U, 7U, 3U, 7U, 7U, 7U, 7U, 7U, 7U, 7U,
};

static uint64_t difference(uint64_t a, uint64_t b) {
    return a > b ? a - b : b - a;
}

static void check_ratio(
    uint32_t part,
    uint32_t total,
    uint32_t numerator,
    uint32_t denominator,
    uint32_t tolerance_points) {
    CHECK(total != 0U);
    uint64_t delta = difference((uint64_t)part * denominator, (uint64_t)total * numerator);
    CHECK(delta * 100U <= (uint64_t)total * denominator * tolerance_points);
}

static void test_rng_and_length_distribution(void) {
    MfRxRng rng;
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
    for(unsigned i = 0U; i < 20000U; i++)
        lengths[mf_callsign_pick_length(&rng)]++;
    CHECK(lengths[4] > 4500U && lengths[4] < 5500U);
    CHECK(lengths[5] > 9000U && lengths[5] < 11000U);
    CHECK(lengths[6] > 4500U && lengths[6] < 5500U);
}

static void check_generated_call(MfCallsignGen* gen, MfRxRng* rng, uint8_t len, MfCallsign* call) {
    char previous[MF_CALLSIGN_PREFIX_MAX + 1U];
    uint8_t previous_len = gen->last_prefix_len;
    memcpy(previous, gen->last_prefix, sizeof(previous));
    CHECK(mf_callsign_generate(gen, rng, len, call));
    CHECK(call->text_len == len && strlen(call->text) == len);
    CHECK(call->prefix_len != 0U && call->prefix_len <= MF_CALLSIGN_PREFIX_MAX);
    CHECK(memcmp(call->text, call->prefix, call->prefix_len) == 0);
    CHECK(mf_callsign_valid(call, len));
    CHECK(
        call->prefix_len != previous_len || previous_len == 0U ||
        memcmp(call->prefix, previous, previous_len) != 0);
}

static void test_large_properties_and_distribution(void) {
    uint32_t family_total[6] = {0};
    uint32_t family_match[6] = {0};

    for(uint8_t len = 4U; len <= 6U; len++) {
        uint32_t counts[MfCallsignEntityCount] = {0};
        uint8_t length_bit = (uint8_t)(1U << (len - 4U));
        uint32_t total_weight = 0U;
        MfCallsignGen gen;
        MfCallsign call;
        MfRxRng rng;
        mf_callsign_gen_init(&gen);
        mf_rx_rng_init(&rng, 0x510e527fU + len);

        for(uint8_t entity = 0U; entity < MfCallsignEntityCount; entity++)
            if((entity_lengths[entity] & length_bit) != 0U) total_weight += entity_weights[entity];

        for(uint32_t i = 0U; i < DISTRIBUTION_SAMPLES; i++) {
            check_generated_call(&gen, &rng, len, &call);
            counts[call.entity]++;
            if(len == 4U && call.entity == MfCallsignEntityUs) {
                family_total[0]++;
                if(call.prefix_len == 1U) family_match[0]++;
            } else if(len == 5U && call.entity == MfCallsignEntityGermany) {
                family_total[1]++;
                if(strcmp(call.prefix, "DL") == 0) family_match[1]++;
            } else if(len == 5U && call.entity == MfCallsignEntitySpain) {
                family_total[2]++;
                if(strcmp(call.prefix, "EA") == 0) family_match[2]++;
            } else if(len == 5U && call.entity == MfCallsignEntityEngland) {
                family_total[3]++;
                if(strcmp(call.prefix, "G") == 0) family_match[3]++;
            } else if(len == 5U && call.entity == MfCallsignEntityBrazil) {
                family_total[4]++;
                if(strcmp(call.prefix, "PY") == 0) family_match[4]++;
            } else if(len == 6U && call.entity == MfCallsignEntityJapan) {
                family_total[5]++;
                if(strcmp(call.prefix, "JA") == 0) family_match[5]++;
            }
        }

        for(uint8_t entity = 0U; entity < MfCallsignEntityCount; entity++) {
            bool eligible = (entity_lengths[entity] & length_bit) != 0U;
            if(!eligible) {
                CHECK(counts[entity] == 0U);
                continue;
            }
            CHECK(counts[entity] != 0U);
            uint64_t delta = difference(
                (uint64_t)counts[entity] * total_weight,
                (uint64_t)DISTRIBUTION_SAMPLES * entity_weights[entity]);
            CHECK(delta * 100U <= (uint64_t)DISTRIBUTION_SAMPLES * total_weight * 3U);
        }
    }

    check_ratio(family_match[0], family_total[0], 22U, 38U, 8U);
    check_ratio(family_match[1], family_total[1], 35U, 83U, 8U);
    check_ratio(family_match[2], family_total[2], 85U, 100U, 8U);
    check_ratio(family_match[3], family_total[3], 56U, 100U, 8U);
    check_ratio(family_match[4], family_total[4], 12U, 18U, 8U);
    check_ratio(family_match[5], family_total[5], 26U, 94U, 8U);
}

static void test_deterministic_seed(void) {
    MfCallsignGen first_gen;
    MfCallsignGen second_gen;
    MfCallsign first;
    MfCallsign second;
    MfRxRng first_rng;
    MfRxRng second_rng;
    mf_callsign_gen_init(&first_gen);
    mf_callsign_gen_init(&second_gen);
    mf_rx_rng_init(&first_rng, 0xc001d00dU);
    mf_rx_rng_init(&second_rng, 0xc001d00dU);
    for(uint32_t i = 0U; i < 3000U; i++) {
        uint8_t len = (uint8_t)(4U + i % 3U);
        CHECK(mf_callsign_generate(&first_gen, &first_rng, len, &first));
        CHECK(mf_callsign_generate(&second_gen, &second_rng, len, &second));
        CHECK(memcmp(&first, &second, sizeof(first)) == 0);
        CHECK(memcmp(&first_gen, &second_gen, sizeof(first_gen)) == 0);
        CHECK(first_rng.state == second_rng.state);
    }
}

int main(void) {
    CHECK(MfCallsignEntityCount == 20U);
    test_rng_and_length_distribution();
    test_large_properties_and_distribution();
    test_deterministic_seed();
    printf("test_rx_callsign_gen: %u checks passed\n", checks);
    return 0;
}
