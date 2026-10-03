#pragma once

#include "kelvinator_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KELVINATOR_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define KELVINATOR_STATE_FILE_VERSION 2

#define KELVINATOR_DEFAULT_MODE       KelvinatorModeCool
#define KELVINATOR_DEFAULT_FAN        KelvinatorFanAuto
#define KELVINATOR_DEFAULT_TEMP       24
#define KELVINATOR_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    KelvinatorMode mode;
    KelvinatorMode last_active_mode; // never Off
    KelvinatorFan fan;
    uint8_t temp;

    // bit i set = KelvinatorToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} KelvinatorState;

KelvinatorState* kelvinator_state_alloc(void);
void kelvinator_state_free(KelvinatorState* state);
void kelvinator_state_reset(KelvinatorState* state);

bool kelvinator_state_load(KelvinatorState* state);
bool kelvinator_state_save(KelvinatorState* state);

void kelvinator_state_set_mode(KelvinatorState* state, KelvinatorMode mode);
void kelvinator_state_set_fan(KelvinatorState* state, KelvinatorFan fan);
void kelvinator_state_temp_up(KelvinatorState* state);
void kelvinator_state_temp_down(KelvinatorState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void kelvinator_state_toggle(KelvinatorState* state, KelvinatorToggle toggle);
bool kelvinator_state_toggle_active(const KelvinatorState* state, KelvinatorToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool kelvinator_state_can_change_fan(const KelvinatorState* state);
/** False in modes that carry no setpoint, and when off. */
bool kelvinator_state_can_change_temp(const KelvinatorState* state);

/** Snapshot the state into the struct the encoder takes. */
KelvinatorRequest kelvinator_state_request(const KelvinatorState* state);

/** Short payload string for the Extra screen. */
void kelvinator_state_format_current(const KelvinatorState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
