#include "morse_flipper_training_timing.h"

#include <limits.h>

uint16_t morse_flipper_training_char_gap_ms(
    uint16_t dit_ms,
    uint8_t character_wpm,
    uint8_t farnsworth_wpm) {
    uint32_t total_ms;
    uint32_t spare_ms;
    uint32_t gap_ms;

    if(farnsworth_wpm == 0U || farnsworth_wpm >= character_wpm)
        return dit_ms > UINT16_MAX / 3U ? UINT16_MAX : (uint16_t)(dit_ms * 3U);
    total_ms = 60000U / farnsworth_wpm;
    if(total_ms <= 31U * (uint32_t)dit_ms)
        return dit_ms > UINT16_MAX / 3U ? UINT16_MAX : (uint16_t)(dit_ms * 3U);
    spare_ms = total_ms - 31U * (uint32_t)dit_ms;
    gap_ms = 3U * ((spare_ms + 9U) / 19U);
    return gap_ms > UINT16_MAX ? UINT16_MAX : (uint16_t)gap_ms;
}
