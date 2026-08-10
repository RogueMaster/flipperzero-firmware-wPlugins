#include "morse_flipper_radio_config.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(morse_flipper_radio_config_candidate(0U) == MF_RADIO_DEFAULT_FREQUENCY_HZ);
    assert(morse_flipper_radio_config_candidate(433920000U) == 433920000U);
    assert(morse_flipper_radio_config_candidate(123U) == 123U);
    puts("test_radio_config_boundary: passed");
    return 0;
}
