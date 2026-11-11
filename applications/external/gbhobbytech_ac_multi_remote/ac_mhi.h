#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <lib/infrared/signal/infrared_signal.h>

#define AC_MHI_88_STATE_BYTES  11U
#define AC_MHI_152_STATE_BYTES 19U
#define AC_MHI_MAX_STATE_BYTES AC_MHI_152_STATE_BYTES
#define AC_MHI_MAX_RAW_TIMINGS (2U + AC_MHI_152_STATE_BYTES * 8U * 2U + 1U)

typedef enum {
    AcMhiVariantUnknown,
    AcMhiVariant88,
    AcMhiVariant152,
} AcMhiVariant;

typedef enum {
    AcMhiModeAuto = 0U,
    AcMhiModeCool = 1U,
    AcMhiModeDry = 2U,
    AcMhiModeFan = 3U,
    AcMhiModeHeat = 4U,
} AcMhiMode;

/* Decode a raw Mitsubishi Heavy Industries signal and return its frame.
 * State bytes are returned in on-air order, including inverted byte pairs. */
bool ac_mhi_decode_signal(
    const InfraredSignal* signal,
    AcMhiVariant* variant,
    uint8_t state[AC_MHI_MAX_STATE_BYTES]);

/* Build a complete MHI state frame. temperature_c must be 17..31.
 * fan_code is the native protocol code; zero selects Auto on both variants. */
bool ac_mhi_build_state(
    AcMhiVariant variant,
    bool power_on,
    uint8_t temperature_c,
    AcMhiMode mode,
    uint8_t fan_code,
    uint8_t state[AC_MHI_MAX_STATE_BYTES]);

/* Convert an on-air state frame into mark/space timings for the Flipper IR API.
 * The caller transmits the timings at 38 kHz. */
bool ac_mhi_build_raw_timings(
    AcMhiVariant variant,
    const uint8_t state[AC_MHI_MAX_STATE_BYTES],
    uint32_t* timings,
    size_t timings_capacity,
    size_t* timings_count);
