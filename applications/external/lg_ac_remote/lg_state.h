#pragma once

#include "lg_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LG_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define LG_STATE_FILE_VERSION 2

#define LG_DEFAULT_MODE       LgModeCool
#define LG_DEFAULT_FAN        LgFanAuto
#define LG_DEFAULT_TEMP       24
#define LG_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    LgMode mode;
    LgMode last_active_mode; // never Off
    LgFan fan;
    uint8_t temp;

    // bit i set = LgToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} LgState;

LgState* lg_state_alloc(void);
void lg_state_free(LgState* state);
void lg_state_reset(LgState* state);

bool lg_state_load(LgState* state);
bool lg_state_save(LgState* state);

void lg_state_set_mode(LgState* state, LgMode mode);
void lg_state_set_fan(LgState* state, LgFan fan);
void lg_state_temp_up(LgState* state);
void lg_state_temp_down(LgState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void lg_state_toggle(LgState* state, LgToggle toggle);
bool lg_state_toggle_active(const LgState* state, LgToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool lg_state_can_change_fan(const LgState* state);
/** False in modes that carry no setpoint, and when off. */
bool lg_state_can_change_temp(const LgState* state);

/** Snapshot the state into the struct the encoder takes. */
LgRequest lg_state_request(const LgState* state);

/** Short payload string for the Extra screen. */
void lg_state_format_current(const LgState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
