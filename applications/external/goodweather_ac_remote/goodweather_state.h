#pragma once

#include "goodweather_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GOODWEATHER_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
// v2 added the protocol-variant byte. Padding means a v1 file is the SAME 17
// bytes as a v2 one, so the size check cannot tell them apart - it is this
// version byte alone that makes the app reject a v1 file and fall back to
// defaults instead of reading a stale byte as the variant.
#define GOODWEATHER_STATE_FILE_VERSION 2

#define GOODWEATHER_DEFAULT_MODE       GoodweatherModeCool
#define GOODWEATHER_DEFAULT_FAN        GoodweatherFanAuto
#define GOODWEATHER_DEFAULT_TEMP       24
#define GOODWEATHER_DEFAULT_SAVE_STATE true

/**
 * Application state.
 *
 * Toggle states live in a bitmask rather than named booleans, so this struct
 * is the same for every protocol regardless of which buttons it has.
 */
typedef struct {
    GoodweatherMode mode;
    GoodweatherMode last_active_mode; // never Off
    GoodweatherFan fan;
    uint8_t temp;

    // bit i set = GoodweatherToggle i is believed to be on. The AC sends no
    // feedback, so this is a local guess for the UI only.
    uint32_t toggle_bits;

    // Selected protocol variant, when the protocol has more than one
    uint8_t option;

    bool save_state;
} GoodweatherState;

GoodweatherState* goodweather_state_alloc(void);
void goodweather_state_free(GoodweatherState* state);
void goodweather_state_reset(GoodweatherState* state);

bool goodweather_state_load(GoodweatherState* state);
bool goodweather_state_save(GoodweatherState* state);

void goodweather_state_set_mode(GoodweatherState* state, GoodweatherMode mode);
void goodweather_state_set_fan(GoodweatherState* state, GoodweatherFan fan);
void goodweather_state_temp_up(GoodweatherState* state);
void goodweather_state_temp_down(GoodweatherState* state);

/** Flip a latching toggle. Momentary buttons are left alone. */
void goodweather_state_toggle(GoodweatherState* state, GoodweatherToggle toggle);
bool goodweather_state_toggle_active(const GoodweatherState* state, GoodweatherToggle toggle);

/** False in modes where the unit forces the fan to Auto. */
bool goodweather_state_can_change_fan(const GoodweatherState* state);
/** False in modes that carry no setpoint, and when off. */
bool goodweather_state_can_change_temp(const GoodweatherState* state);

/** Snapshot the state into the struct the encoder takes. */
GoodweatherRequest goodweather_state_request(const GoodweatherState* state);

/** Short payload string for the Extra screen. */
void goodweather_state_format_current(const GoodweatherState* state, char* out, size_t len);

#ifdef __cplusplus
}
#endif
