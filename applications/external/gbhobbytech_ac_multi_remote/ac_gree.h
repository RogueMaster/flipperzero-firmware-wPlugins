#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <infrared/signal/infrared_signal.h>

#define AC_GREE_STATE_BYTES     8U
#define AC_GREE_RAW_MAX_TIMINGS 160U

typedef enum {
    AcGreeModelYaw1f,
    AcGreeModelYbofb,
    AcGreeModelYx1fsf,
    AcGreeModelUnknown,
} AcGreeModel;

bool ac_gree_decode_signal(
    const InfraredSignal* signal,
    AcGreeModel* model,
    uint8_t frame[AC_GREE_STATE_BYTES]);

bool ac_gree_build_probe(AcGreeModel model, bool power_on, InfraredSignal* signal);

bool ac_gree_transmit(AcGreeModel model, const uint8_t frame[AC_GREE_STATE_BYTES]);

bool ac_gree_get_controls(
    AcGreeModel* model,
    const uint8_t frame[AC_GREE_STATE_BYTES],
    bool* power_on,
    uint8_t* temperature_c,
    uint8_t* mode_code,
    uint8_t* fan_index);

void ac_gree_set_power(AcGreeModel model, uint8_t frame[AC_GREE_STATE_BYTES], bool power_on);

void ac_gree_set_temperature(uint8_t frame[AC_GREE_STATE_BYTES], uint8_t temperature_c);

void ac_gree_toggle_mode(uint8_t frame[AC_GREE_STATE_BYTES]);

void ac_gree_set_fan(uint8_t frame[AC_GREE_STATE_BYTES], uint8_t fan_index);
