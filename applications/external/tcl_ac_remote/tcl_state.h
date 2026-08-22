#pragma once

#include "tcl_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCL_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define TCL_STATE_FILE_VERSION 2

#define TCL_DEFAULT_MODE       TclModeCool
#define TCL_DEFAULT_FAN        TclFanAuto
#define TCL_DEFAULT_TEMP       24
#define TCL_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    TclMode mode;
    TclMode last_active_mode; // never Off
    TclFan fan;
    uint8_t temp;

    // bit i set = TclToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} TclState;

TclState* tcl_state_alloc(void);
void tcl_state_free(TclState* state);
void tcl_state_reset(TclState* state);

bool tcl_state_load(TclState* state);
bool tcl_state_save(TclState* state);

void tcl_state_set_mode(TclState* state, TclMode mode);
void tcl_state_set_fan(TclState* state, TclFan fan);
void tcl_state_temp_up(TclState* state);
void tcl_state_temp_down(TclState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void tcl_state_toggle(TclState* state, TclToggle toggle);
bool tcl_state_toggle_active(const TclState* state, TclToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool tcl_state_can_change_fan(const TclState* state);
/** False in modes that carry no setpoint, and when off. */
bool tcl_state_can_change_temp(const TclState* state);

/** Snapshot the state into the struct the encoder takes. */
TclRequest tcl_state_request(const TclState* state);

/** Short payload string for the Extra screen. */
void tcl_state_format_current(const TclState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
