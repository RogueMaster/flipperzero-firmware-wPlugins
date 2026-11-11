#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <infrared/signal/infrared_signal.h>

#define AC_FUJITSU_MAX_STATE_BYTES 16U
#define AC_FUJITSU_MAX_RAW_TIMINGS (2U + AC_FUJITSU_MAX_STATE_BYTES * 8U * 2U + 1U)

typedef enum {
    AcFujitsuVariantUnknown,
    /* Shared safe baseline for ARRAH2E, ARREB1E, and ARRY4. */
    AcFujitsuVariantLong30,
    /* ARDB1 and ARJW2 share the same frame when swing is off. */
    AcFujitsuVariantCompact,
    /* ARREW4E protocol version 0x31. */
    AcFujitsuVariantLong31,
} AcFujitsuVariant;

bool ac_fujitsu_decode_signal(
    const InfraredSignal* signal,
    AcFujitsuVariant* variant,
    uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES]);

bool ac_fujitsu_build_probe(AcFujitsuVariant variant, bool power_on, InfraredSignal* signal);

bool ac_fujitsu_transmit(AcFujitsuVariant variant, const uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES]);

bool ac_fujitsu_build_state(
    AcFujitsuVariant variant,
    bool power_on,
    uint8_t temperature_c,
    uint8_t mode,
    uint8_t fan_code,
    uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES]);

void ac_fujitsu_update_checksum(
    AcFujitsuVariant variant,
    uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES]);

uint8_t ac_fujitsu_min_temperature(void);
uint8_t ac_fujitsu_max_temperature(void);
