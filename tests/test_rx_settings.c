#include <assert.h>
#include <stdio.h>

#include "morse_flipper_rx_settings.h"

int main(void) {
    MorseFlipperRxSettings settings;
    uint8_t min;
    uint8_t max;

    morse_flipper_rx_settings_reset(&settings);
    assert(settings.length == 5U && settings.wpm == 25U && settings.farnsworth_wpm == 12U);
    settings = (MorseFlipperRxSettings){.length = 99U, .wpm = 1U, .farnsworth_wpm = 30U};
    morse_flipper_rx_settings_normalize(&settings);
    assert(settings.length == 5U && settings.wpm == 10U && settings.farnsworth_wpm == 10U);

    for(uint8_t selection = 0U; selection < 6U; selection++) {
        static const uint8_t expected_min[] = {4U, 5U, 6U, 4U, 5U, 4U};
        static const uint8_t expected_max[] = {4U, 5U, 6U, 5U, 6U, 6U};
        morse_flipper_rx_settings_length_bounds(selection, &min, &max);
        assert(min == expected_min[selection] && max == expected_max[selection]);
    }
    morse_flipper_rx_settings_length_bounds(99U, &min, &max);
    assert(min == 4U && max == 6U);
    puts("test_rx_settings: passed");
    return 0;
}
