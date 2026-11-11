#pragma once

#include <stdbool.h>
#include <infrared/signal/infrared_signal.h>

typedef enum {
    AcLgVariantLg,
    AcLgVariantLg2,
} AcLgVariant;

/* The two LG A/C formats have distinct header timings but share the 28-bit
 * state layout and checksum used by the public IRremoteESP8266 reference. */
bool ac_lg_matches_variant(const InfraredSignal* signal, AcLgVariant variant);
bool ac_lg_build_probe(AcLgVariant variant, bool power_on, InfraredSignal* signal);

#define AC_LG_MODE_COOL 0U
#define AC_LG_MODE_HEAT 4U

bool ac_lg_decode_signal(const InfraredSignal* signal, AcLgVariant* variant, uint32_t* frame);
bool ac_lg_read_controls(
    uint32_t frame,
    bool* power_on,
    uint8_t* temperature_c,
    uint8_t* mode_code,
    uint8_t* fan_index);
bool ac_lg_build_state(
    bool power_on,
    uint8_t temperature_c,
    uint8_t mode_code,
    uint8_t fan_index,
    uint32_t* frame);
bool ac_lg_transmit(
    AcLgVariant variant,
    bool power_on,
    uint8_t temperature_c,
    uint8_t mode_code,
    uint8_t fan_index);
