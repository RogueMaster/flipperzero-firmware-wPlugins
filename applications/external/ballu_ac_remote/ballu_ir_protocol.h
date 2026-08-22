#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// TEMPLATE protocol module.
//
// This is the ONLY file (with its .c) that carries protocol knowledge. The
// app shell and all three views drive themselves off the enums and the name
// getters below, so a port never has to edit a view.
//
// Rules that keep the generic UI working:
//  - BalluModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - BalluTogglePowerOff must stay the FIRST toggle.
//  - At most BALLU_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in BalluExtra.
// --------------------------------------------------------------------------

// IR carrier
#define BALLU_IR_CARRIER_FREQ 38000
#define BALLU_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define BALLU_IR_MAX_TIMINGS 240

// Temperature range (Celsius)
#define BALLU_TEMP_MIN 16
#define BALLU_TEMP_MAX 32

// Main screen fits 6 toggle buttons before it collides with the footer
#define BALLU_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define BALLU_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define BALLU_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    BalluModeOff = 0,
    BalluModeCool,
    BalluModeAuto,
    BalluModeDry,
    BalluModeHeat,
    BalluModeFan,
    BalluModeCount
} BalluMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    BalluFanAuto = 0,
    BalluFanLow,
    BalluFanMedium,
    BalluFanHigh,
    BalluFanCount
} BalluFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    BalluTogglePowerOff = 0,
    BalluToggleSwingV,
    BalluToggleSwingH,
    BalluToggleCount
} BalluToggle;

// Less common commands, listed on the Extra screen
// The YKR-K/002E frame carries no further commands, so Extra just offers the
// two swing axes together, which the main screen can only set one at a time.
typedef enum {
    BalluExtraSwingBoth = 0,
    BalluExtraSwingNone,
    BalluExtraCount
} BalluExtra;

/**
 * Everything the encoder needs to build a frame.
 *
 * Protocols split into two families and this struct serves both:
 *  - "short code" remotes (Coolix, LG) send a dedicated word per button and
 *    ignore most of this;
 *  - "full state" remotes (Daikin, Gree, Panasonic, ...) resend the entire
 *    state on every press, so a button is a bit inside the frame and the
 *    encoder needs the rest of the settings to rebuild it.
 */
typedef struct {
    BalluMode mode;
    BalluFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = BalluToggle i currently believed on
    uint8_t option; // protocol variant, see ballu_ir_get_option_count()
} BalluRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for BalluModeOff (send the power-off toggle)
 */
bool ballu_ir_encode_state(const BalluRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool ballu_ir_encode_toggle(
    const BalluRequest* req,
    BalluToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool ballu_ir_encode_extra(
    const BalluRequest* req,
    BalluExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit BALLU_CODE_STR_LEN including the NUL.
 */
void ballu_ir_format_state(const BalluRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void ballu_ir_format_toggle(const BalluRequest* req, BalluToggle toggle, char* out, size_t len);

/** Same, for an Extra-screen entry. */
void ballu_ir_format_extra(const BalluRequest* req, BalluExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool ballu_ir_toggle_is_momentary(BalluToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool ballu_ir_mode_locks_fan(BalluMode mode);

/** Whether this mode carries no temperature setpoint. */
bool ballu_ir_mode_has_no_temp(BalluMode mode);

/** Display names. */
const char* ballu_ir_get_mode_name(BalluMode mode);
const char* ballu_ir_get_fan_name(BalluFan fan);
const char* ballu_ir_get_toggle_name(BalluToggle toggle);
const char* ballu_ir_get_extra_name(BalluExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t ballu_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* ballu_ir_get_option_label(void);

/** Display name of one variant. */
const char* ballu_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* ballu_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
