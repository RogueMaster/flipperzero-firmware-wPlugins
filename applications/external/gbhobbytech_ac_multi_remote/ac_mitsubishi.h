#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <infrared/signal/infrared_signal.h>

#define AC_MITSUBISHI_FRAME_BYTES 18U
#define AC_MITSUBISHI_PREFIX_MAX  120U
#define AC_MITSUBISHI_RAW_MAX     420U

bool ac_mitsubishi_decode_profile(
    const InfraredSignal* signal,
    uint8_t frame[AC_MITSUBISHI_FRAME_BYTES],
    uint32_t* prefix,
    size_t* prefix_count);

void ac_mitsubishi_update_checksum(uint8_t frame[AC_MITSUBISHI_FRAME_BYTES]);

bool ac_mitsubishi_build_signal(
    const uint8_t frame[AC_MITSUBISHI_FRAME_BYTES],
    const uint32_t* prefix,
    size_t prefix_count,
    uint32_t* timings,
    size_t timings_capacity,
    size_t* timings_count);
