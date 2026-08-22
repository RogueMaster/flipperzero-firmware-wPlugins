#pragma once

#include "carrier_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CARRIER_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define CARRIER_STATE_FILE_VERSION 2

#define CARRIER_DEFAULT_MODE       CarrierModeCool
#define CARRIER_DEFAULT_FAN        CarrierFanAuto
#define CARRIER_DEFAULT_TEMP       24
#define CARRIER_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    CarrierMode mode;
    CarrierMode last_active_mode; // never Off
    CarrierFan fan;
    uint8_t temp;

    // bit i set = CarrierToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} CarrierState;

CarrierState* carrier_state_alloc(void);
void carrier_state_free(CarrierState* state);
void carrier_state_reset(CarrierState* state);

bool carrier_state_load(CarrierState* state);
bool carrier_state_save(CarrierState* state);

void carrier_state_set_mode(CarrierState* state, CarrierMode mode);
void carrier_state_set_fan(CarrierState* state, CarrierFan fan);
void carrier_state_temp_up(CarrierState* state);
void carrier_state_temp_down(CarrierState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void carrier_state_toggle(CarrierState* state, CarrierToggle toggle);
bool carrier_state_toggle_active(const CarrierState* state, CarrierToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool carrier_state_can_change_fan(const CarrierState* state);
/** False in modes that carry no setpoint, and when off. */
bool carrier_state_can_change_temp(const CarrierState* state);

/** Snapshot the state into the struct the encoder takes. */
CarrierRequest carrier_state_request(const CarrierState* state);

/** Short payload string for the Extra screen. */
void carrier_state_format_current(const CarrierState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
