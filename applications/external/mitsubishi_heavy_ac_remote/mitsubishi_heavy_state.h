#pragma once

#include "mitsubishi_heavy_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MITSUBISHI_HEAVY_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define MITSUBISHI_HEAVY_STATE_FILE_VERSION 2

#define MITSUBISHI_HEAVY_DEFAULT_MODE       MitsubishiHeavyModeCool
#define MITSUBISHI_HEAVY_DEFAULT_FAN        MitsubishiHeavyFanAuto
#define MITSUBISHI_HEAVY_DEFAULT_TEMP       24
#define MITSUBISHI_HEAVY_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    MitsubishiHeavyMode mode;
    MitsubishiHeavyMode last_active_mode; // never Off
    MitsubishiHeavyFan fan;
    uint8_t temp;

    // bit i set = MitsubishiHeavyToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} MitsubishiHeavyState;

MitsubishiHeavyState* mitsubishi_heavy_state_alloc(void);
void mitsubishi_heavy_state_free(MitsubishiHeavyState* state);
void mitsubishi_heavy_state_reset(MitsubishiHeavyState* state);

bool mitsubishi_heavy_state_load(MitsubishiHeavyState* state);
bool mitsubishi_heavy_state_save(MitsubishiHeavyState* state);

void mitsubishi_heavy_state_set_mode(MitsubishiHeavyState* state, MitsubishiHeavyMode mode);
void mitsubishi_heavy_state_set_fan(MitsubishiHeavyState* state, MitsubishiHeavyFan fan);
void mitsubishi_heavy_state_temp_up(MitsubishiHeavyState* state);
void mitsubishi_heavy_state_temp_down(MitsubishiHeavyState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void mitsubishi_heavy_state_toggle(MitsubishiHeavyState* state, MitsubishiHeavyToggle toggle);
bool mitsubishi_heavy_state_toggle_active(
    const MitsubishiHeavyState* state,
    MitsubishiHeavyToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool mitsubishi_heavy_state_can_change_fan(const MitsubishiHeavyState* state);
/** False in modes that carry no setpoint, and when off. */
bool mitsubishi_heavy_state_can_change_temp(const MitsubishiHeavyState* state);

/** Snapshot the state into the struct the encoder takes. */
MitsubishiHeavyRequest mitsubishi_heavy_state_request(const MitsubishiHeavyState* state);

/** Short payload string for the Extra screen. */
void mitsubishi_heavy_state_format_current(
    const MitsubishiHeavyState* state,
    char* out,
    size_t len);

#ifdef __cplusplus
}
#endif
