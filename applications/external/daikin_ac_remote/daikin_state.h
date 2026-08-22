#pragma once

#include "daikin_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DAIKIN_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define DAIKIN_STATE_FILE_VERSION 2

#define DAIKIN_DEFAULT_MODE       DaikinModeCool
#define DAIKIN_DEFAULT_FAN        DaikinFanAuto
#define DAIKIN_DEFAULT_TEMP       24
#define DAIKIN_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    DaikinMode mode;
    DaikinMode last_active_mode; // never Off
    DaikinFan fan;
    uint8_t temp;

    // bit i set = DaikinToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} DaikinState;

DaikinState* daikin_state_alloc(void);
void daikin_state_free(DaikinState* state);
void daikin_state_reset(DaikinState* state);

bool daikin_state_load(DaikinState* state);
bool daikin_state_save(DaikinState* state);

void daikin_state_set_mode(DaikinState* state, DaikinMode mode);
void daikin_state_set_fan(DaikinState* state, DaikinFan fan);
void daikin_state_temp_up(DaikinState* state);
void daikin_state_temp_down(DaikinState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void daikin_state_toggle(DaikinState* state, DaikinToggle toggle);
bool daikin_state_toggle_active(const DaikinState* state, DaikinToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool daikin_state_can_change_fan(const DaikinState* state);
/** False in modes that carry no setpoint, and when off. */
bool daikin_state_can_change_temp(const DaikinState* state);

/** Snapshot the state into the struct the encoder takes. */
DaikinRequest daikin_state_request(const DaikinState* state);

/** Short payload string for the Extra screen. */
void daikin_state_format_current(const DaikinState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
