#pragma once

#include "midea_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDEA_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define MIDEA_STATE_FILE_VERSION 2

#define MIDEA_DEFAULT_MODE       MideaModeCool
#define MIDEA_DEFAULT_FAN        MideaFanAuto
#define MIDEA_DEFAULT_TEMP       24
#define MIDEA_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    MideaMode mode;
    MideaMode last_active_mode; // never Off
    MideaFan fan;
    uint8_t temp;

    // bit i set = MideaToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} MideaState;

MideaState* midea_state_alloc(void);
void midea_state_free(MideaState* state);
void midea_state_reset(MideaState* state);

bool midea_state_load(MideaState* state);
bool midea_state_save(MideaState* state);

void midea_state_set_mode(MideaState* state, MideaMode mode);
void midea_state_set_fan(MideaState* state, MideaFan fan);
void midea_state_temp_up(MideaState* state);
void midea_state_temp_down(MideaState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void midea_state_toggle(MideaState* state, MideaToggle toggle);
bool midea_state_toggle_active(const MideaState* state, MideaToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool midea_state_can_change_fan(const MideaState* state);
/** False in modes that carry no setpoint, and when off. */
bool midea_state_can_change_temp(const MideaState* state);

/** Snapshot the state into the struct the encoder takes. */
MideaRequest midea_state_request(const MideaState* state);

/** Short payload string for the Extra screen. */
void midea_state_format_current(const MideaState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
