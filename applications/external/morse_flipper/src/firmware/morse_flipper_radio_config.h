#pragma once

#include <stdint.h>

#include "plugins/radio/mf_radio_types.h"

static inline uint32_t morse_flipper_radio_config_candidate(uint32_t stored_hz) {
    return stored_hz != 0U ? stored_hz : MF_RADIO_DEFAULT_FREQUENCY_HZ;
}
