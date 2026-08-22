#pragma once

#include "panasonic_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PANASONIC_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define PANASONIC_STATE_FILE_VERSION 2

#define PANASONIC_DEFAULT_MODE       PanasonicModeCool
#define PANASONIC_DEFAULT_FAN        PanasonicFanAuto
#define PANASONIC_DEFAULT_TEMP       24
#define PANASONIC_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    PanasonicMode mode;
    PanasonicMode last_active_mode; // never Off
    PanasonicFan fan;
    uint8_t temp;

    // bit i set = PanasonicToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} PanasonicState;

PanasonicState* panasonic_state_alloc(void);
void panasonic_state_free(PanasonicState* state);
void panasonic_state_reset(PanasonicState* state);

bool panasonic_state_load(PanasonicState* state);
bool panasonic_state_save(PanasonicState* state);

void panasonic_state_set_mode(PanasonicState* state, PanasonicMode mode);
void panasonic_state_set_fan(PanasonicState* state, PanasonicFan fan);
void panasonic_state_temp_up(PanasonicState* state);
void panasonic_state_temp_down(PanasonicState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void panasonic_state_toggle(PanasonicState* state, PanasonicToggle toggle);
bool panasonic_state_toggle_active(const PanasonicState* state, PanasonicToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool panasonic_state_can_change_fan(const PanasonicState* state);
/** False in modes that carry no setpoint, and when off. */
bool panasonic_state_can_change_temp(const PanasonicState* state);

/** Snapshot the state into the struct the encoder takes. */
PanasonicRequest panasonic_state_request(const PanasonicState* state);

/** Short payload string for the Extra screen. */
void panasonic_state_format_current(const PanasonicState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
