#pragma once
/*
 * fsd_ota.h — Tesla OTA-install detection (0x318 GTW_carState), shared by both
 * platforms.
 *
 * GTW_updateInProgress is byte6 bits[1:0]; raw 2 = installing, and all TX
 * pauses while it is set. On current cars byte6 is a rolling counter instead
 * (steps by 2, always odd), so bits[1:0] alternate 1/3, and dropped RX frames
 * can alias any counter to a constant raw value. Matching one raw value is not
 * enough: the ESP32's raw==1 debounce latched on a few dropped frames and the
 * 1/3 alternation never released it (#183).
 *
 * A real flag holds still; a counter changes every frame. So a sample only
 * asserts when raw == 2 AND byte6 repeats the previous 0x318 byte6 exactly.
 * FSD_OTA_ASSERT_FRAMES asserting samples in a row latch tesla_ota_in_progress;
 * FSD_OTA_CLEAR_FRAMES samples of anything else release it.
 *
 * Header-only `static inline`: the ESP32 build compiles no fsd_logic sources,
 * so fsd_logic/fsd_handler.c and esp32/.firmware/fsd_handler.cpp both include
 * this one copy (host-tested by test/test_fsd_core.c and test_esp32_core.cpp).
 */

#include "fsd_state.h"
#include <stdbool.h>
#include <stdint.h>

#define FSD_OTA_RAW_MASK       0x03u // GTW_updateInProgress = byte6 bits[1:0]
#define FSD_OTA_RAW_INSTALLING 2u // only raw value that pauses TX (raw 1 false-positived, #19)
#define FSD_OTA_ASSERT_FRAMES  3u // consecutive asserting samples to latch
#define FSD_OTA_CLEAR_FRAMES   6u // consecutive other samples to release

// Feed one 0x318 byte6 (caller checks DLC). Updates ota_raw_state, the
// debounce counters and tesla_ota_in_progress.
static inline void fsd_ota_update(FSDState* s, uint8_t byte6) {
    uint8_t raw = byte6 & FSD_OTA_RAW_MASK;
    s->ota_raw_state = raw;
    bool repeat = s->ota_last_valid && byte6 == s->ota_last_byte6;
    if(raw == FSD_OTA_RAW_INSTALLING && repeat) {
        if(s->ota_assert_count < 255u) s->ota_assert_count++;
        s->ota_clear_count = 0;
        if(s->ota_assert_count >= FSD_OTA_ASSERT_FRAMES) s->tesla_ota_in_progress = true;
    } else {
        if(s->ota_clear_count < 255u) s->ota_clear_count++;
        s->ota_assert_count = 0;
        if(s->ota_clear_count >= FSD_OTA_CLEAR_FRAMES) s->tesla_ota_in_progress = false;
    }
    s->ota_last_byte6 = byte6;
    s->ota_last_valid = true;
}
