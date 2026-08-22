#pragma once

#include "ballu_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BALLU_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define BALLU_STATE_FILE_VERSION 2

#define BALLU_DEFAULT_MODE       BalluModeCool
#define BALLU_DEFAULT_FAN        BalluFanAuto
#define BALLU_DEFAULT_TEMP       24
#define BALLU_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    BalluMode mode;
    BalluMode last_active_mode; // never Off
    BalluFan fan;
    uint8_t temp;

    // bit i set = BalluToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} BalluState;

BalluState* ballu_state_alloc(void);
void ballu_state_free(BalluState* state);
void ballu_state_reset(BalluState* state);

bool ballu_state_load(BalluState* state);
bool ballu_state_save(BalluState* state);

void ballu_state_set_mode(BalluState* state, BalluMode mode);
void ballu_state_set_fan(BalluState* state, BalluFan fan);
void ballu_state_temp_up(BalluState* state);
void ballu_state_temp_down(BalluState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void ballu_state_toggle(BalluState* state, BalluToggle toggle);
bool ballu_state_toggle_active(const BalluState* state, BalluToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool ballu_state_can_change_fan(const BalluState* state);
/** False in modes that carry no setpoint, and when off. */
bool ballu_state_can_change_temp(const BalluState* state);

/** Snapshot the state into the struct the encoder takes. */
BalluRequest ballu_state_request(const BalluState* state);

/** Short payload string for the Extra screen. */
void ballu_state_format_current(const BalluState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
