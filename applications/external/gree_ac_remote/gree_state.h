#pragma once

#include "gree_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GREE_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define GREE_STATE_FILE_VERSION 2

#define GREE_DEFAULT_MODE       GreeModeCool
#define GREE_DEFAULT_FAN        GreeFanAuto
#define GREE_DEFAULT_TEMP       24
#define GREE_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    GreeMode mode;
    GreeMode last_active_mode; // never Off
    GreeFan fan;
    uint8_t temp;

    // bit i set = GreeToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} GreeState;

GreeState* gree_state_alloc(void);
void gree_state_free(GreeState* state);
void gree_state_reset(GreeState* state);

bool gree_state_load(GreeState* state);
bool gree_state_save(GreeState* state);

void gree_state_set_mode(GreeState* state, GreeMode mode);
void gree_state_set_fan(GreeState* state, GreeFan fan);
void gree_state_temp_up(GreeState* state);
void gree_state_temp_down(GreeState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void gree_state_toggle(GreeState* state, GreeToggle toggle);
bool gree_state_toggle_active(const GreeState* state, GreeToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool gree_state_can_change_fan(const GreeState* state);
/** False in modes that carry no setpoint, and when off. */
bool gree_state_can_change_temp(const GreeState* state);

/** Snapshot the state into the struct the encoder takes. */
GreeRequest gree_state_request(const GreeState* state);

/** Short payload string for the Extra screen. */
void gree_state_format_current(const GreeState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
