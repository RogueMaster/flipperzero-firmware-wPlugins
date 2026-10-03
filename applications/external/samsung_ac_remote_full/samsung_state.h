#pragma once

#include "samsung_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMSUNG_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define SAMSUNG_STATE_FILE_VERSION 2

#define SAMSUNG_DEFAULT_MODE       SamsungModeCool
#define SAMSUNG_DEFAULT_FAN        SamsungFanAuto
#define SAMSUNG_DEFAULT_TEMP       24
#define SAMSUNG_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    SamsungMode mode;
    SamsungMode last_active_mode; // never Off
    SamsungFan fan;
    uint8_t temp;

    // bit i set = SamsungToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} SamsungState;

SamsungState* samsung_state_alloc(void);
void samsung_state_free(SamsungState* state);
void samsung_state_reset(SamsungState* state);

bool samsung_state_load(SamsungState* state);
bool samsung_state_save(SamsungState* state);

void samsung_state_set_mode(SamsungState* state, SamsungMode mode);
void samsung_state_set_fan(SamsungState* state, SamsungFan fan);
void samsung_state_temp_up(SamsungState* state);
void samsung_state_temp_down(SamsungState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void samsung_state_toggle(SamsungState* state, SamsungToggle toggle);
bool samsung_state_toggle_active(const SamsungState* state, SamsungToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool samsung_state_can_change_fan(const SamsungState* state);
/** False in modes that carry no setpoint, and when off. */
bool samsung_state_can_change_temp(const SamsungState* state);

/** Snapshot the state into the struct the encoder takes. */
SamsungRequest samsung_state_request(const SamsungState* state);

/** Short payload string for the Extra screen. */
void samsung_state_format_current(const SamsungState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
