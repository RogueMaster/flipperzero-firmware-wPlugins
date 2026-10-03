#pragma once

#include "mitsubishi_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MITSUBISHI_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define MITSUBISHI_STATE_FILE_VERSION 2

#define MITSUBISHI_DEFAULT_MODE       MitsubishiModeCool
#define MITSUBISHI_DEFAULT_FAN        MitsubishiFanAuto
#define MITSUBISHI_DEFAULT_TEMP       24
#define MITSUBISHI_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    MitsubishiMode mode;
    MitsubishiMode last_active_mode; // never Off
    MitsubishiFan fan;
    uint8_t temp;

    // bit i set = MitsubishiToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} MitsubishiState;

MitsubishiState* mitsubishi_state_alloc(void);
void mitsubishi_state_free(MitsubishiState* state);
void mitsubishi_state_reset(MitsubishiState* state);

bool mitsubishi_state_load(MitsubishiState* state);
bool mitsubishi_state_save(MitsubishiState* state);

void mitsubishi_state_set_mode(MitsubishiState* state, MitsubishiMode mode);
void mitsubishi_state_set_fan(MitsubishiState* state, MitsubishiFan fan);
void mitsubishi_state_temp_up(MitsubishiState* state);
void mitsubishi_state_temp_down(MitsubishiState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void mitsubishi_state_toggle(MitsubishiState* state, MitsubishiToggle toggle);
bool mitsubishi_state_toggle_active(const MitsubishiState* state, MitsubishiToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool mitsubishi_state_can_change_fan(const MitsubishiState* state);
/** False in modes that carry no setpoint, and when off. */
bool mitsubishi_state_can_change_temp(const MitsubishiState* state);

/** Snapshot the state into the struct the encoder takes. */
MitsubishiRequest mitsubishi_state_request(const MitsubishiState* state);

/** Short payload string for the Extra screen. */
void mitsubishi_state_format_current(const MitsubishiState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
