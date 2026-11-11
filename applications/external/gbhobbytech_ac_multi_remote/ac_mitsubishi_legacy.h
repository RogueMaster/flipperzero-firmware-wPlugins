#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <infrared/signal/infrared_signal.h>

#define AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES 17U
#define AC_MITSUBISHI_LEGACY_MAX_RAW_TIMINGS 300U

typedef enum {
    AcMitsubishiLegacyNone,
    AcMitsubishiLegacy112,
    AcMitsubishiLegacy136,
} AcMitsubishiLegacyVariant;

bool ac_mitsubishi_legacy_decode(
    const InfraredSignal* signal,
    AcMitsubishiLegacyVariant* variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES]);

bool ac_mitsubishi_legacy_build_probe(
    AcMitsubishiLegacyVariant variant,
    bool power_on,
    InfraredSignal* signal);

bool ac_mitsubishi_legacy_transmit(
    AcMitsubishiLegacyVariant variant,
    const uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES]);

bool ac_mitsubishi_legacy_get_controls(
    AcMitsubishiLegacyVariant variant,
    const uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES],
    bool* power_on,
    uint8_t* temperature_c,
    uint8_t* mode_code,
    uint8_t* fan_index);

void ac_mitsubishi_legacy_set_power(
    AcMitsubishiLegacyVariant variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES],
    bool power_on);

void ac_mitsubishi_legacy_set_temperature(
    AcMitsubishiLegacyVariant variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES],
    uint8_t temperature_c);

void ac_mitsubishi_legacy_toggle_mode(
    AcMitsubishiLegacyVariant variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES]);

void ac_mitsubishi_legacy_set_fan(
    AcMitsubishiLegacyVariant variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES],
    uint8_t fan_index);

uint8_t ac_mitsubishi_legacy_min_temperature(AcMitsubishiLegacyVariant variant);
uint8_t ac_mitsubishi_legacy_max_temperature(AcMitsubishiLegacyVariant variant);
uint8_t ac_mitsubishi_legacy_fan_count(AcMitsubishiLegacyVariant variant);
