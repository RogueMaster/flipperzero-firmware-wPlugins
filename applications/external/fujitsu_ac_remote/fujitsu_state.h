#pragma once

#include "fujitsu_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FUJITSU_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define FUJITSU_STATE_FILE_VERSION 2

#define FUJITSU_DEFAULT_MODE       FujitsuModeCool
#define FUJITSU_DEFAULT_FAN        FujitsuFanAuto
#define FUJITSU_DEFAULT_TEMP       24
#define FUJITSU_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    FujitsuMode mode;
    FujitsuMode last_active_mode; // never Off
    FujitsuFan fan;
    uint8_t temp;

    // bit i set = FujitsuToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} FujitsuState;

FujitsuState* fujitsu_state_alloc(void);
void fujitsu_state_free(FujitsuState* state);
void fujitsu_state_reset(FujitsuState* state);

bool fujitsu_state_load(FujitsuState* state);
bool fujitsu_state_save(FujitsuState* state);

void fujitsu_state_set_mode(FujitsuState* state, FujitsuMode mode);
void fujitsu_state_set_fan(FujitsuState* state, FujitsuFan fan);
void fujitsu_state_temp_up(FujitsuState* state);
void fujitsu_state_temp_down(FujitsuState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void fujitsu_state_toggle(FujitsuState* state, FujitsuToggle toggle);
bool fujitsu_state_toggle_active(const FujitsuState* state, FujitsuToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool fujitsu_state_can_change_fan(const FujitsuState* state);
/** False in modes that carry no setpoint, and when off. */
bool fujitsu_state_can_change_temp(const FujitsuState* state);

/** Snapshot the state into the struct the encoder takes. */
FujitsuRequest fujitsu_state_request(const FujitsuState* state);

/** Short payload string for the Extra screen. */
void fujitsu_state_format_current(const FujitsuState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
