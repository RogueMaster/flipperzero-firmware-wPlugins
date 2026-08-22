#pragma once

#include "delonghi_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DELONGHI_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define DELONGHI_STATE_FILE_VERSION 2

#define DELONGHI_DEFAULT_MODE       DelonghiModeCool
#define DELONGHI_DEFAULT_FAN        DelonghiFanAuto
#define DELONGHI_DEFAULT_TEMP       24
#define DELONGHI_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    DelonghiMode mode;
    DelonghiMode last_active_mode; // never Off
    DelonghiFan fan;
    uint8_t temp;

    // bit i set = DelonghiToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} DelonghiState;

DelonghiState* delonghi_state_alloc(void);
void delonghi_state_free(DelonghiState* state);
void delonghi_state_reset(DelonghiState* state);

bool delonghi_state_load(DelonghiState* state);
bool delonghi_state_save(DelonghiState* state);

void delonghi_state_set_mode(DelonghiState* state, DelonghiMode mode);
void delonghi_state_set_fan(DelonghiState* state, DelonghiFan fan);
void delonghi_state_temp_up(DelonghiState* state);
void delonghi_state_temp_down(DelonghiState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void delonghi_state_toggle(DelonghiState* state, DelonghiToggle toggle);
bool delonghi_state_toggle_active(const DelonghiState* state, DelonghiToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool delonghi_state_can_change_fan(const DelonghiState* state);
/** False in modes that carry no setpoint, and when off. */
bool delonghi_state_can_change_temp(const DelonghiState* state);

/** Snapshot the state into the struct the encoder takes. */
DelonghiRequest delonghi_state_request(const DelonghiState* state);

/** Short payload string for the Extra screen. */
void delonghi_state_format_current(const DelonghiState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
