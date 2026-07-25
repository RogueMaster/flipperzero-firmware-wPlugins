#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "plugins/tx_groups/mf_tx_groups_api.h"

static unsigned checks;

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

static void test_api_layout(void) {
    CHECK(offsetof(MfTxGroupsApi, mapped) == 0U);
    CHECK(sizeof(MfTxGroupsApi) >= sizeof(MorseFlipperMappedFalApi));
    CHECK(MF_TX_GROUPS_API_VERSION == 1U);
}

static void test_seeded_generation_and_text_input(void) {
    MorseFlipperTxGroup first;
    MorseFlipperTxGroup second;

    morse_flipper_tx_group_init(&first);
    morse_flipper_tx_group_init(&second);
    CHECK(strcmp(first.target, "ABCDE") == 0);
    CHECK(first.pass_min == 90U && first.pass_max == 110U);
    morse_flipper_tx_group_set_seed(&first, 0x12345678U);
    morse_flipper_tx_group_set_seed(&second, 0x12345678U);
    morse_flipper_tx_group_start(&first, false);
    morse_flipper_tx_group_start(&second, false);
    CHECK(strcmp(first.target, second.target) == 0);
    morse_flipper_tx_group_feed_text(&first, "a |b\x7f" "3");
    CHECK(strcmp(first.answer, "AB3") == 0);
    CHECK(morse_flipper_tx_group_answer_len(&first) == 3U);
    CHECK(!morse_flipper_tx_group_complete(&first));
}

static void test_raw_rescue_and_scoring(void) {
    MorseFlipperTxGroup group;

    morse_flipper_tx_group_init(&group);
    memcpy(group.target, "EEEEE", sizeof(group.target));
    for(uint8_t i = 0U; i < MORSE_FLIPPER_TX_GROUP_LEN; i++) {
        morse_flipper_tx_group_feed_mark(&group, 100U);
        if(i + 1U < MORSE_FLIPPER_TX_GROUP_LEN)
            morse_flipper_tx_group_feed_space(&group, 300U);
    }
    CHECK(morse_flipper_tx_group_expected_marks(&group) == MORSE_FLIPPER_TX_GROUP_LEN);
    CHECK(morse_flipper_tx_group_marks_complete(&group));
    CHECK(morse_flipper_tx_group_finalize_answer_from_raw(&group, 100U));
    CHECK(strcmp(group.answer, "EEEEE") == 0);
    morse_flipper_tx_group_score(&group, 100U, false);
    CHECK(group.result.correct == MORSE_FLIPPER_TX_GROUP_LEN);
    CHECK(group.result.speed_pct == 100U && group.result.letter_gap_pct == 100U);
    CHECK(group.result.passed);
}

int main(void) {
    test_api_layout();
    test_seeded_generation_and_text_input();
    test_raw_rescue_and_scoring();
    printf("test_tx_groups: %u checks passed\n", checks);
    return 0;
}
