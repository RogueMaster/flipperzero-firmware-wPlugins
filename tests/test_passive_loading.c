#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "morse_flipper_passive_loading.h"

static unsigned checks;

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

static void test_exact_frames_and_one_shot_load(void) {
    MorseFlipperPassiveLoading loading;

    morse_flipper_passive_loading_start(&loading, 100U);
    CHECK(loading.active && loading.frame == 0U);
    CHECK(strcmp(morse_flipper_passive_loading_suffix(loading.frame), "...") == 0);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 299U));
    CHECK(loading.frame == 0U);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 300U));
    CHECK(loading.frame == 1U && strcmp(morse_flipper_passive_loading_suffix(loading.frame), "...") == 0);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 500U));
    CHECK(loading.frame == 2U && strcmp(morse_flipper_passive_loading_suffix(loading.frame), " ..") == 0);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 700U));
    CHECK(loading.frame == 3U && strcmp(morse_flipper_passive_loading_suffix(loading.frame), ". .") == 0);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 900U));
    CHECK(loading.frame == 4U && strcmp(morse_flipper_passive_loading_suffix(loading.frame), ".. ") == 0);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 1099U));
    CHECK(morse_flipper_passive_loading_tick(&loading, 1100U));
    CHECK(!loading.active);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 1200U));
}

static void test_late_ticks_and_wrap(void) {
    MorseFlipperPassiveLoading loading;

    morse_flipper_passive_loading_start(&loading, 0U);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 850U));
    CHECK(loading.frame == 4U);
    CHECK(morse_flipper_passive_loading_tick(&loading, 1000U));

    morse_flipper_passive_loading_start(&loading, UINT32_MAX - 300U);
    CHECK(!morse_flipper_passive_loading_tick(&loading, UINT32_MAX - 100U));
    CHECK(loading.frame == 1U);
    CHECK(!morse_flipper_passive_loading_tick(&loading, 499U));
    CHECK(loading.frame == 4U);
    CHECK(morse_flipper_passive_loading_tick(&loading, 699U));
}

static void test_triple_back_cancellation(void) {
    MorseFlipperPassiveLoading loading;

    morse_flipper_passive_loading_start(&loading, 0U);
    CHECK(!morse_flipper_passive_loading_input(&loading, true, false, false, 1U));
    CHECK(!morse_flipper_passive_loading_input(&loading, true, false, false, 700U));
    CHECK(morse_flipper_passive_loading_input(&loading, true, false, false, 701U));

    morse_flipper_passive_loading_start(&loading, 0U);
    CHECK(!morse_flipper_passive_loading_input(&loading, true, false, false, 1U));
    CHECK(!morse_flipper_passive_loading_input(&loading, true, false, false, 702U));
    CHECK(!morse_flipper_passive_loading_input(&loading, true, false, false, 703U));
    CHECK(morse_flipper_passive_loading_input(&loading, true, false, false, 704U));
    CHECK(!morse_flipper_passive_loading_input(&loading, false, true, false, 705U));
    CHECK(!morse_flipper_passive_loading_input(&loading, true, false, false, 706U));
    CHECK(!morse_flipper_passive_loading_input(&loading, false, false, true, 707U));
    CHECK(!morse_flipper_passive_loading_input(&loading, true, false, false, 708U));
}

int main(void) {
    test_exact_frames_and_one_shot_load();
    test_late_ticks_and_wrap();
    test_triple_back_cancellation();
    printf("test_passive_loading: %u checks passed\n", checks);
    return 0;
}
