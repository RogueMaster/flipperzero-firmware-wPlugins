#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_callsign_gen.h"

static uint32_t queue_values[256];
static size_t queue_count;
static size_t queue_at;
static unsigned checks;

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

void mf_rx_rng_init(MfRxRng* rng, uint32_t seed) {
    if(rng != NULL) rng->state = seed;
}

uint32_t mf_rx_rng_next(MfRxRng* rng) {
    return rng == NULL ? 0U : ++rng->state;
}

uint32_t mf_rx_rng_bounded(MfRxRng* rng, uint32_t bound) {
    (void)rng;
    assert(queue_at < queue_count);
    assert(queue_values[queue_at] < bound);
    return queue_values[queue_at++];
}

static void set_queue(const uint32_t* values, size_t count) {
    assert(count <= sizeof(queue_values) / sizeof(queue_values[0]));
    memcpy(queue_values, values, count * sizeof(values[0]));
    queue_count = count;
    queue_at = 0U;
}

static void check_one_entity(
    MfCallsignEntity entity,
    uint32_t entity_roll,
    const uint32_t* tail,
    size_t tail_count) {
    MfCallsignGen gen = {0};
    MfCallsign call;
    MfRxRng rng = {1U};
    uint32_t values[16];

    values[0] = entity_roll;
    memcpy(&values[1], tail, tail_count * sizeof(tail[0]));
    set_queue(values, tail_count + 1U);
    CHECK(mf_callsign_generate(&gen, &rng, 4U, &call));
    CHECK(call.entity == entity);
    CHECK(mf_callsign_valid(&call, 4U));
    CHECK(queue_at == queue_count);
}

static void test_entity_constructors(void) {
    static const uint32_t us[] = {0U, 0U, 1U, 2U, 3U};
    static const uint32_t de[] = {0U, 1U, 2U};
    static const uint32_t it[] = {0U, 1U, 2U};
    static const uint32_t ca[] = {0U, 1U, 2U, 3U};
    static const uint32_t ro[] = {0U, 1U, 2U};

    check_one_entity(MfCallsignEntityUs, 0U, us, sizeof(us) / sizeof(us[0]));
    check_one_entity(MfCallsignEntityGermany, 42U, de, sizeof(de) / sizeof(de[0]));
    check_one_entity(MfCallsignEntityItaly, 61U, it, sizeof(it) / sizeof(it[0]));
    check_one_entity(MfCallsignEntityCanada, 76U, ca, sizeof(ca) / sizeof(ca[0]));
    check_one_entity(MfCallsignEntityRomania, 91U, ro, sizeof(ro) / sizeof(ro[0]));
}

static void test_collision_fallback(void) {
    MfCallsignGen gen = {.last_prefix = "K", .last_prefix_len = 1U};
    MfCallsign call;
    MfRxRng rng = {1U};
    uint32_t values[128];
    size_t count = 0U;

    for(unsigned attempt = 0U; attempt < 16U; attempt++) {
        values[count++] = 0U; /* US */
        values[count++] = 0U; /* one-letter row */
        values[count++] = 0U; /* K */
        values[count++] = attempt % 10U;
        values[count++] = attempt % 26U;
        values[count++] = (attempt + 1U) % 26U;
        values[count++] = (attempt + 2U) % 26U;
    }
    values[count++] = 0U; /* K fallback digit */
    values[count++] = 0U;
    values[count++] = 1U;
    values[count++] = 2U;
    values[count++] = 1U; /* DL fallback digit */
    values[count++] = 2U;
    values[count++] = 3U;
    set_queue(values, count);
    CHECK(mf_callsign_generate(&gen, &rng, 5U, &call));
    CHECK(strcmp(call.prefix, "DL") == 0);
    CHECK(mf_callsign_valid(&call, 5U));
    CHECK(queue_at == queue_count);
}

static void test_validation_and_failed_publish(void) {
    MfCallsign call = {
        .text = "9A1BC",
        .prefix = "9A",
        .text_len = 5U,
        .prefix_len = 2U,
        .entity = MfCallsignEntityUs,
    };
    MfCallsignGen gen = {.last_prefix = "DL", .last_prefix_len = 2U};
    MfRxRng rng = {1U};

    CHECK(mf_callsign_valid(&call, 5U));
    call.prefix[1] = 'B';
    CHECK(!mf_callsign_valid(&call, 5U));
    CHECK(!mf_callsign_generate(&gen, &rng, 3U, &call));
    CHECK(gen.last_prefix_len == 2U && memcmp(gen.last_prefix, "DL", 2U) == 0);
}

int main(void) {
    test_entity_constructors();
    test_collision_fallback();
    test_validation_and_failed_publish();
    printf("test_rx_callsign_controlled: %u checks passed\n", checks);
    return 0;
}
