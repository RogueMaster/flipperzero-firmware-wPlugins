#include "mf_radio_hal.h"

#include <assert.h>
#include <stdio.h>

static uint32_t valid_min_hz;
static uint32_t valid_max_hz;
static uint32_t allowed_min_hz;
static uint32_t allowed_max_hz;

static bool frequency_valid(uint32_t frequency_hz) {
    return frequency_hz >= valid_min_hz && frequency_hz <= valid_max_hz;
}

static bool frequency_allowed(uint32_t frequency_hz) {
    return frequency_hz >= allowed_min_hz && frequency_hz <= allowed_max_hz;
}

int main(void) {
    MfRadioCwfmTiming timing;
    MfRadioCwfmStaticConfig static_config;
    bool level;
    bool previous = false;
    uint32_t sum = 0U;
    unsigned short_count = 0U;
    unsigned long_count = 0U;
    unsigned i;

    valid_min_hz = allowed_min_hz = 387000000U;
    valid_max_hz = allowed_max_hz = 464000000U;
    assert(mf_radio_cwfm_static_config(
        434160000U, frequency_valid, frequency_allowed, &static_config));
    assert(static_config.frequency_hz == 434162380U && !static_config.data_level);
    assert(static_config.frequency_hz - MF_RADIO_CWFM_DEVIATION_HZ == 434160000U);

    assert(mf_radio_cwfm_static_config(
        464000000U, frequency_valid, frequency_allowed, &static_config));
    assert(static_config.frequency_hz == 463997620U && static_config.data_level);
    assert(static_config.frequency_hz + MF_RADIO_CWFM_DEVIATION_HZ == 464000000U);

    allowed_max_hz = 433920000U;
    assert(mf_radio_cwfm_static_config(
        433920000U, frequency_valid, frequency_allowed, &static_config));
    assert(static_config.frequency_hz == 433917620U && static_config.data_level);

    allowed_min_hz = allowed_max_hz = 434160000U;
    assert(!mf_radio_cwfm_static_config(
        434160000U, frequency_valid, frequency_allowed, &static_config));
    assert(!mf_radio_cwfm_static_config(434160000U, NULL, frequency_allowed, &static_config));
    assert(!mf_radio_cwfm_static_config(434160000U, frequency_valid, NULL, &static_config));
    assert(!mf_radio_cwfm_static_config(434160000U, frequency_valid, frequency_allowed, NULL));

    mf_radio_cwfm_timing_reset(&timing);
    for(i = 0U; i < 7U; i++) {
        uint16_t duration = mf_radio_cwfm_next_half_period(&timing, &level);
        assert(level != previous);
        previous = level;
        assert(duration == 714U || duration == 715U);
        short_count += duration == 714U;
        long_count += duration == 715U;
        sum += duration;
    }
    assert(short_count == 5U && long_count == 2U && sum == 5000U);

    mf_radio_cwfm_timing_reset(&timing);
    sum = 0U;
    for(i = 0U; i < 1400U; i++) {
        uint16_t duration = mf_radio_cwfm_next_half_period(&timing, &level);
        assert(duration == 714U || duration == 715U);
        sum += duration;
    }
    assert(sum == 1000000U);

    mf_radio_cwfm_timing_reset(&timing);
    assert(mf_radio_cwfm_next_half_period(&timing, &level) == 714U);
    assert(level);
    assert(mf_radio_cwfm_next_half_period(NULL, &level) == 0U);
    assert(mf_radio_cwfm_next_half_period(&timing, NULL) == 0U);

    puts("test_radio_cwfm_timing: passed");
    return 0;
}
