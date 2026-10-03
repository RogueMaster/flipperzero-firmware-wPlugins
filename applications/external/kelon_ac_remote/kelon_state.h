#pragma once

#include "kelon_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KELON_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define KELON_STATE_FILE_VERSION 2

#define KELON_DEFAULT_MODE       KelonModeCool
#define KELON_DEFAULT_FAN        KelonFanAuto
#define KELON_DEFAULT_TEMP       24
#define KELON_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    KelonMode mode;
    KelonMode last_active_mode; // never Off
    KelonFan fan;
    uint8_t temp;

    // bit i set = KelonToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} KelonState;

KelonState* kelon_state_alloc(void);
void kelon_state_free(KelonState* state);
void kelon_state_reset(KelonState* state);

bool kelon_state_load(KelonState* state);
bool kelon_state_save(KelonState* state);

void kelon_state_set_mode(KelonState* state, KelonMode mode);
void kelon_state_set_fan(KelonState* state, KelonFan fan);
void kelon_state_temp_up(KelonState* state);
void kelon_state_temp_down(KelonState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void kelon_state_toggle(KelonState* state, KelonToggle toggle);
bool kelon_state_toggle_active(const KelonState* state, KelonToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool kelon_state_can_change_fan(const KelonState* state);
/** False in modes that carry no setpoint, and when off. */
bool kelon_state_can_change_temp(const KelonState* state);

/** Snapshot the state into the struct the encoder takes. */
KelonRequest kelon_state_request(const KelonState* state);

/** Short payload string for the Extra screen. */
void kelon_state_format_current(const KelonState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
