#pragma once

#include "neoclima_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEOCLIMA_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define NEOCLIMA_STATE_FILE_VERSION 2

#define NEOCLIMA_DEFAULT_MODE       NeoclimaModeCool
#define NEOCLIMA_DEFAULT_FAN        NeoclimaFanAuto
#define NEOCLIMA_DEFAULT_TEMP       24
#define NEOCLIMA_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    NeoclimaMode mode;
    NeoclimaMode last_active_mode; // never Off
    NeoclimaFan fan;
    uint8_t temp;

    // bit i set = NeoclimaToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} NeoclimaState;

NeoclimaState* neoclima_state_alloc(void);
void neoclima_state_free(NeoclimaState* state);
void neoclima_state_reset(NeoclimaState* state);

bool neoclima_state_load(NeoclimaState* state);
bool neoclima_state_save(NeoclimaState* state);

void neoclima_state_set_mode(NeoclimaState* state, NeoclimaMode mode);
void neoclima_state_set_fan(NeoclimaState* state, NeoclimaFan fan);
void neoclima_state_temp_up(NeoclimaState* state);
void neoclima_state_temp_down(NeoclimaState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void neoclima_state_toggle(NeoclimaState* state, NeoclimaToggle toggle);
bool neoclima_state_toggle_active(const NeoclimaState* state, NeoclimaToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool neoclima_state_can_change_fan(const NeoclimaState* state);
/** False in modes that carry no setpoint, and when off. */
bool neoclima_state_can_change_temp(const NeoclimaState* state);

/** Snapshot the state into the struct the encoder takes. */
NeoclimaRequest neoclima_state_request(const NeoclimaState* state);

/** Short payload string for the Extra screen. */
void neoclima_state_format_current(const NeoclimaState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
