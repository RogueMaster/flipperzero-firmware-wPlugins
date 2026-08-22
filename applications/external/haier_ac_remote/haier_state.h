#pragma once

#include "haier_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAIER_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define HAIER_STATE_FILE_VERSION 2

#define HAIER_DEFAULT_MODE       HaierModeCool
#define HAIER_DEFAULT_FAN        HaierFanAuto
#define HAIER_DEFAULT_TEMP       24
#define HAIER_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    HaierMode mode;
    HaierMode last_active_mode; // never Off
    HaierFan fan;
    uint8_t temp;

    // bit i set = HaierToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} HaierState;

HaierState* haier_state_alloc(void);
void haier_state_free(HaierState* state);
void haier_state_reset(HaierState* state);

bool haier_state_load(HaierState* state);
bool haier_state_save(HaierState* state);

void haier_state_set_mode(HaierState* state, HaierMode mode);
void haier_state_set_fan(HaierState* state, HaierFan fan);
void haier_state_temp_up(HaierState* state);
void haier_state_temp_down(HaierState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void haier_state_toggle(HaierState* state, HaierToggle toggle);
bool haier_state_toggle_active(const HaierState* state, HaierToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool haier_state_can_change_fan(const HaierState* state);
/** False in modes that carry no setpoint, and when off. */
bool haier_state_can_change_temp(const HaierState* state);

/** Snapshot the state into the struct the encoder takes. */
HaierRequest haier_state_request(const HaierState* state);

/** Short payload string for the Extra screen. */
void haier_state_format_current(const HaierState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
