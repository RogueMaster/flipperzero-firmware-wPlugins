#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_callsign_gen.h"

typedef struct {
    uint8_t weight;
    uint8_t lengths;
    uint8_t rule_totals[3];
} EntityFixture;

static const EntityFixture entities[MfCallsignEntityCount] = {
    {40U, 7U, {38U, 40U, 1U}},     {18U, 7U, {1U, 83U, 83U}},     {14U, 7U, {1U, 11U, 1U}},
    {14U, 7U, {4U, 45U, 56U}},     {8U, 7U, {1U, 1U, 1U}},        {16U, 7U, {100U, 100U, 100U}},
    {13U, 7U, {100U, 100U, 100U}}, {13U, 4U, {0U, 0U, 94U}},      {12U, 7U, {99U, 99U, 100U}},
    {12U, 7U, {7U, 100U, 1U}},     {12U, 7U, {12U, 18U, 20U}},    {7U, 7U, {2U, 1U, 1U}},
    {11U, 3U, {1U, 1U, 0U}},       {11U, 7U, {2U, 1U, 1U}},       {10U, 7U, {15U, 18U, 20U}},
    {10U, 7U, {16U, 19U, 20U}},    {10U, 7U, {100U, 100U, 100U}}, {7U, 7U, {9U, 7U, 7U}},
    {9U, 7U, {1U, 1U, 1U}},        {9U, 7U, {3U, 20U, 6U}},
};

static uint32_t queue_values[32];
static size_t queue_count;
static size_t queue_at;
static size_t rng_calls;
static bool zero_tail;
static unsigned checks;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)

void mf_rx_rng_init(MfRxRng* rng, uint32_t seed) {
    if(rng != NULL) rng->state = seed;
}

uint32_t mf_rx_rng_next(MfRxRng* rng) {
    return rng == NULL ? 0U : ++rng->state;
}

uint32_t mf_rx_rng_bounded(MfRxRng* rng, uint32_t bound) {
    (void)rng;
    uint32_t value;
    rng_calls++;
    if(queue_at < queue_count) {
        value = queue_values[queue_at++];
    } else {
        assert(zero_tail);
        value = 0U;
    }
    assert(bound != 0U && value < bound);
    return value;
}

static void set_queue(const uint32_t* values, size_t count, bool use_zero_tail) {
    assert(count <= sizeof(queue_values) / sizeof(queue_values[0]));
    if(count != 0U) memcpy(queue_values, values, count * sizeof(values[0]));
    queue_count = count;
    queue_at = 0U;
    rng_calls = 0U;
    zero_tail = use_zero_tail;
}

static uint32_t entity_roll(MfCallsignEntity wanted, uint8_t len) {
    uint8_t length_bit = (uint8_t)(1U << (len - 4U));
    uint32_t roll = 0U;
    for(uint8_t entity = 0U; entity < (uint8_t)wanted; entity++)
        if((entities[entity].lengths & length_bit) != 0U) roll += entities[entity].weight;
    return roll;
}

static unsigned owner_count(const MfCallsign* source, uint8_t len) {
    unsigned owners = 0U;
    for(uint8_t entity = 0U; entity < MfCallsignEntityCount; entity++) {
        MfCallsign candidate = *source;
        candidate.entity = entity;
        if(mf_callsign_valid(&candidate, len)) owners++;
    }
    return owners;
}

static void test_every_entity_length_and_rule_bucket(void) {
    bool reached[MfCallsignEntityCount] = {false};
    for(uint8_t entity = 0U; entity < MfCallsignEntityCount; entity++) {
        for(uint8_t len = 4U; len <= 6U; len++) {
            uint8_t total = entities[entity].rule_totals[len - 4U];
            CHECK((total != 0U) == ((entities[entity].lengths & (1U << (len - 4U))) != 0U));
            for(uint32_t rule_roll = 0U; rule_roll < total; rule_roll++) {
                uint32_t values[] = {
                    entity_roll((MfCallsignEntity)entity, len),
                    rule_roll,
                };
                MfCallsignGen gen = {0};
                MfCallsign call;
                MfRxRng rng = {1U};
                set_queue(values, sizeof(values) / sizeof(values[0]), true);
                CHECK(mf_callsign_generate(&gen, &rng, len, &call));
                CHECK(call.entity == entity);
                CHECK(mf_callsign_valid(&call, len));
                CHECK(call.prefix_len != 0U);
                CHECK(memcmp(call.text, call.prefix, call.prefix_len) == 0);
                CHECK(owner_count(&call, len) == 1U);
                reached[entity] = true;
            }
        }
    }
    for(uint8_t entity = 0U; entity < MfCallsignEntityCount; entity++)
        CHECK(reached[entity]);
}

static MfCallsign make_call(MfCallsignEntity entity, const char* prefix, const char* text) {
    MfCallsign call = {.entity = (uint8_t)entity};
    call.prefix_len = (uint8_t)strlen(prefix);
    call.text_len = (uint8_t)strlen(text);
    memcpy(call.prefix, prefix, call.prefix_len + 1U);
    memcpy(call.text, text, call.text_len + 1U);
    return call;
}

static void
    check_call(MfCallsignEntity entity, const char* prefix, const char* text, bool expected) {
    MfCallsign call = make_call(entity, prefix, text);
    CHECK(mf_callsign_valid(&call, call.text_len) == expected);
    if(expected) CHECK(owner_count(&call, call.text_len) == 1U);
}

static void test_eligibility_and_split_entities(void) {
    check_call(MfCallsignEntityJapan, "JA", "JA1A", false);
    check_call(MfCallsignEntityJapan, "JA", "JA1AA", false);
    check_call(MfCallsignEntityJapan, "JA", "JA1AAA", true);
    check_call(MfCallsignEntityFrance, "F", "F1AA", true);
    check_call(MfCallsignEntityFrance, "F", "F1AAA", true);
    check_call(MfCallsignEntityFrance, "F", "F1AAAA", false);

    check_call(MfCallsignEntitySpain, "EA", "EA1AA", true);
    check_call(MfCallsignEntitySpain, "EA", "EA6AA", false);
    check_call(MfCallsignEntitySpain, "EA", "EA8AA", false);
    check_call(MfCallsignEntitySpain, "EA", "EA9AA", false);
    check_call(MfCallsignEntityFinland, "OH", "OH1AA", true);
    check_call(MfCallsignEntityFinland, "OH", "OH0AA", false);

    check_call(MfCallsignEntityItaly, "IT", "IT8AA", true);
    check_call(MfCallsignEntityItaly, "IT", "IT9AA", false);
    check_call(MfCallsignEntityItaly, "IS", "IS0AA", false);
    check_call(MfCallsignEntityItaly, "IM", "IM9AA", false);
    check_call(MfCallsignEntityItaly, "IG", "IG9AA", false);
    check_call(MfCallsignEntityItaly, "IP", "IP9AA", false);

    check_call(MfCallsignEntityEuropeanRussia, "RA", "RA1AA", true);
    check_call(MfCallsignEntityEuropeanRussia, "RA", "RA8AA", false);
    check_call(MfCallsignEntityAsiaticRussia, "RA", "RA1AA", false);
    check_call(MfCallsignEntityAsiaticRussia, "RA", "RA8AA", true);

    check_call(MfCallsignEntityArgentina, "LU", "LU1AAA", true);
    check_call(MfCallsignEntityArgentina, "LU", "LU1ZAA", false);
    check_call(MfCallsignEntityArgentina, "LU", "LU2ZAA", true);
    check_call(MfCallsignEntitySlovenia, "S5", "S51AA", true);
}

static void test_collision_fallback_all_lengths(void) {
    static const char* collisions[] = {"K", "K", "KA"};
    for(uint8_t len = 4U; len <= 6U; len++) {
        MfCallsignGen gen = {0};
        MfCallsign call;
        MfRxRng rng = {1U};
        gen.last_prefix_len = (uint8_t)strlen(collisions[len - 4U]);
        memcpy(gen.last_prefix, collisions[len - 4U], gen.last_prefix_len + 1U);
        set_queue(NULL, 0U, true);
        CHECK(mf_callsign_generate(&gen, &rng, len, &call));
        CHECK(strcmp(call.prefix, collisions[len - 4U]) != 0);
        CHECK(mf_callsign_valid(&call, len));
        CHECK(rng_calls < 200U);
    }
}

static void test_invalid_input_does_not_publish(void) {
    MfCallsignGen gen = {.last_prefix = "DL", .last_prefix_len = 2U};
    MfCallsign out = make_call(MfCallsignEntityUs, "K", "K1AA");
    MfRxRng rng = {.state = 123U};
    MfCallsignGen before_gen = gen;
    MfCallsign before_out = out;
    MfRxRng before_rng = rng;

    CHECK(!mf_callsign_generate(&gen, &rng, 3U, &out));
    CHECK(memcmp(&gen, &before_gen, sizeof(gen)) == 0);
    CHECK(memcmp(&out, &before_out, sizeof(out)) == 0);
    CHECK(memcmp(&rng, &before_rng, sizeof(rng)) == 0);
    CHECK(!mf_callsign_generate(&gen, &rng, 7U, &out));
    CHECK(memcmp(&gen, &before_gen, sizeof(gen)) == 0);
    CHECK(memcmp(&out, &before_out, sizeof(out)) == 0);
    CHECK(memcmp(&rng, &before_rng, sizeof(rng)) == 0);
}

int main(void) {
    CHECK(MfCallsignEntityCount == 20U);
    test_every_entity_length_and_rule_bucket();
    test_eligibility_and_split_entities();
    test_collision_fallback_all_lengths();
    test_invalid_input_does_not_publish();
    printf("test_rx_callsign_controlled: %u checks passed\n", checks);
    return 0;
}
