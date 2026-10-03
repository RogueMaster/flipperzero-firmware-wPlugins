#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "keyer.h"

static unsigned checks;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)

static void check_no_event(MorseKeyer* keyer) {
    MorseKeyerEvent event;
    CHECK(!morse_keyer_pop_event(keyer, &event));
}

static void check_event(MorseKeyer* keyer, uint8_t type, uint8_t paddle) {
    MorseKeyerEvent event;
    CHECK(morse_keyer_pop_event(keyer, &event));
    CHECK(event.type == type);
    CHECK(event.paddle == paddle);
}

static void test_bug_repeats_dits_and_finishes_released_mark(void) {
    MorseKeyer keyer;

    morse_keyer_init(&keyer, MorseKeyerModeBug, 100U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, true, 0U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 0U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDit);
    CHECK(morse_keyer_output_active(&keyer, MorseKeyerPaddleDit));

    morse_keyer_tick(&keyer, 99U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 100U);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDit);
    CHECK(!morse_keyer_output_active(&keyer, MorseKeyerPaddleDit));

    morse_keyer_tick(&keyer, 199U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 200U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDit);

    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, false, 225U);
    check_no_event(&keyer);
    CHECK(morse_keyer_output_active(&keyer, MorseKeyerPaddleDit));
    morse_keyer_tick(&keyer, 299U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 300U);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDit);
    CHECK(!morse_keyer_output_active(&keyer, MorseKeyerPaddleDit));

    morse_keyer_tick(&keyer, 400U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 1000U);
    check_no_event(&keyer);
}

static void test_bug_dah_is_fully_manual(void) {
    MorseKeyer keyer;

    morse_keyer_init(&keyer, MorseKeyerModeBug, 80U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, true, 17U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDah);
    CHECK(morse_keyer_output_active(&keyer, MorseKeyerPaddleDah));

    morse_keyer_tick(&keyer, 10000U);
    check_no_event(&keyer);
    CHECK(morse_keyer_output_active(&keyer, MorseKeyerPaddleDah));

    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, false, 10001U);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDah);
    CHECK(!morse_keyer_output_active(&keyer, MorseKeyerPaddleDah));
    morse_keyer_tick(&keyer, 20000U);
    check_no_event(&keyer);
}

static void test_bug_simultaneous_contacts_and_dah_to_dits(void) {
    MorseKeyer keyer;

    morse_keyer_init(&keyer, MorseKeyerModeBug, 100U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, true, 0U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDah);

    /* The automatic mechanism continues its phase underneath the manual contact. */
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, true, 10U);
    morse_keyer_tick(&keyer, 10U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 110U);
    check_no_event(&keyer);
    CHECK(morse_keyer_output_active(&keyer, MorseKeyerPaddleDah));

    /* Releasing DAH during the hidden dit gap releases immediately. */
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, false, 150U);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDah);
    CHECK(!morse_keyer_output_active(&keyer, MorseKeyerPaddleDah));
    morse_keyer_tick(&keyer, 209U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 210U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDit);

    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, false, 225U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 310U);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDit);

    morse_keyer_init(&keyer, MorseKeyerModeBug, 100U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, true, 0U);
    morse_keyer_tick(&keyer, 0U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDit);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, true, 25U);
    check_no_event(&keyer);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, false, 50U);
    check_no_event(&keyer);
    CHECK(morse_keyer_output_active(&keyer, MorseKeyerPaddleDit));
    morse_keyer_tick(&keyer, 100U);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDit);
}

static void test_bug_reset_releases_every_contact(void) {
    MorseKeyer keyer;

    morse_keyer_init(&keyer, MorseKeyerModeBug, 60U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, true, 0U);
    morse_keyer_tick(&keyer, 0U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDit);
    morse_keyer_reset(&keyer);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDit);
    morse_keyer_tick(&keyer, 1000U);
    check_no_event(&keyer);

    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, true, 1100U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDah);
    morse_keyer_reset(&keyer);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDah);
    morse_keyer_tick(&keyer, 2000U);
    check_no_event(&keyer);

    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, true, 2100U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDah);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, true, 2110U);
    morse_keyer_tick(&keyer, 2110U);
    check_no_event(&keyer);
    morse_keyer_reset(&keyer);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDah);
    CHECK(!morse_keyer_output_active(&keyer, MorseKeyerPaddleDit));
    CHECK(!morse_keyer_output_active(&keyer, MorseKeyerPaddleDah));
    morse_keyer_tick(&keyer, 3000U);
    check_no_event(&keyer);
}

static void test_bug_timing_across_uint32_wrap(void) {
    MorseKeyer keyer;
    const uint32_t start = UINT32_MAX - 49U;

    morse_keyer_init(&keyer, MorseKeyerModeBug, 100U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, true, start);
    morse_keyer_tick(&keyer, start);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDit);

    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, false, UINT32_MAX - 25U);
    morse_keyer_tick(&keyer, UINT32_MAX - 24U);
    check_no_event(&keyer);
    CHECK(morse_keyer_output_active(&keyer, MorseKeyerPaddleDit));
    morse_keyer_tick(&keyer, 49U);
    check_no_event(&keyer);
    morse_keyer_tick(&keyer, 50U);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDit);
    morse_keyer_tick(&keyer, 150U);
    check_no_event(&keyer);
}

static void test_iambic_reset_regression(void) {
    MorseKeyer keyer;

    morse_keyer_init(&keyer, MorseKeyerModeIambicB, 60U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, true, 0U);
    morse_keyer_tick(&keyer, 0U);
    check_event(&keyer, MorseKeyerEventPress, MorseKeyerPaddleDit);

    /* Queue the opposite element, then model an RX Answer-to-Result cleanup. */
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, true, 1U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, false, 2U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, false, 2U);
    morse_keyer_reset(&keyer);

    CHECK(morse_keyer_get_mode(&keyer) == MorseKeyerModeIambicB);
    CHECK(morse_keyer_get_dit_duration(&keyer) == 60U);
    check_event(&keyer, MorseKeyerEventRelease, MorseKeyerPaddleDit);
    morse_keyer_tick(&keyer, 1000U);
    check_no_event(&keyer);
}

int main(void) {
    test_bug_repeats_dits_and_finishes_released_mark();
    test_bug_dah_is_fully_manual();
    test_bug_simultaneous_contacts_and_dah_to_dits();
    test_bug_reset_releases_every_contact();
    test_bug_timing_across_uint32_wrap();
    test_iambic_reset_regression();

    printf("test_keyer: %u checks passed\n", checks);
    return 0;
}
