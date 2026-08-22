#pragma once

#include "toshiba_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOSHIBA_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define TOSHIBA_STATE_FILE_VERSION 2

#define TOSHIBA_DEFAULT_MODE       ToshibaModeCool
#define TOSHIBA_DEFAULT_FAN        ToshibaFanAuto
#define TOSHIBA_DEFAULT_TEMP       24
#define TOSHIBA_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    ToshibaMode mode;
    ToshibaMode last_active_mode; // never Off
    ToshibaFan fan;
    uint8_t temp;

    // bit i set = ToshibaToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} ToshibaState;

ToshibaState* toshiba_state_alloc(void);
void toshiba_state_free(ToshibaState* state);
void toshiba_state_reset(ToshibaState* state);

bool toshiba_state_load(ToshibaState* state);
bool toshiba_state_save(ToshibaState* state);

void toshiba_state_set_mode(ToshibaState* state, ToshibaMode mode);
void toshiba_state_set_fan(ToshibaState* state, ToshibaFan fan);
void toshiba_state_temp_up(ToshibaState* state);
void toshiba_state_temp_down(ToshibaState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void toshiba_state_toggle(ToshibaState* state, ToshibaToggle toggle);
bool toshiba_state_toggle_active(const ToshibaState* state, ToshibaToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool toshiba_state_can_change_fan(const ToshibaState* state);
/** False in modes that carry no setpoint, and when off. */
bool toshiba_state_can_change_temp(const ToshibaState* state);

/** Snapshot the state into the struct the encoder takes. */
ToshibaRequest toshiba_state_request(const ToshibaState* state);

/** Short payload string for the Extra screen. */
void toshiba_state_format_current(const ToshibaState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
