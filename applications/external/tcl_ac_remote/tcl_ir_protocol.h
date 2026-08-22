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
//  - TclModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - TclTogglePowerOff must stay the FIRST toggle.
//  - At most TCL_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in TclExtra.
// --------------------------------------------------------------------------

// IR carrier
#define TCL_IR_CARRIER_FREQ 38000
#define TCL_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define TCL_IR_MAX_TIMINGS 250

// Temperature range (Celsius)
#define TCL_TEMP_MIN 16
#define TCL_TEMP_MAX 31

// Main screen fits 6 toggle buttons before it collides with the footer
#define TCL_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define TCL_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define TCL_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    TclModeOff = 0,
    TclModeCool,
    TclModeAuto,
    TclModeDry,
    TclModeHeat,
    TclModeFan,
    TclModeCount
} TclMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    TclFanAuto = 0,
    TclFanLow,
    TclFanMedium,
    TclFanHigh,
    TclFanCount
} TclFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    TclTogglePowerOff = 0,
    TclToggleSwing, // vertical vane oscillation
    TclToggleTurbo,
    TclToggleQuiet,
    TclToggleLight, // display backlight
    TclToggleEcono,
    TclToggleHealth, // ioniser
    TclToggleCount
} TclToggle;

// Less common commands, listed on the Extra screen
// Fixed vane positions, which the main screen's Swing button cannot express.
typedef enum {
    TclExtraVaneHighest = 0,
    TclExtraVaneHigh,
    TclExtraVaneMiddle,
    TclExtraVaneLow,
    TclExtraVaneLowest,
    TclExtraCount
} TclExtra;

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
    TclMode mode;
    TclFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = TclToggle i currently believed on
    uint8_t option; // protocol variant, see tcl_ir_get_option_count()
} TclRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for TclModeOff (send the power-off toggle)
 */
bool tcl_ir_encode_state(const TclRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool tcl_ir_encode_toggle(
    const TclRequest* req,
    TclToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool tcl_ir_encode_extra(
    const TclRequest* req,
    TclExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit TCL_CODE_STR_LEN including the NUL.
 */
void tcl_ir_format_state(const TclRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void tcl_ir_format_toggle(const TclRequest* req, TclToggle toggle, char* out, size_t len);

/** Same, for an Extra-screen entry. */
void tcl_ir_format_extra(const TclRequest* req, TclExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool tcl_ir_toggle_is_momentary(TclToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool tcl_ir_mode_locks_fan(TclMode mode);

/** Whether this mode carries no temperature setpoint. */
bool tcl_ir_mode_has_no_temp(TclMode mode);

/** Display names. */
const char* tcl_ir_get_mode_name(TclMode mode);
const char* tcl_ir_get_fan_name(TclFan fan);
const char* tcl_ir_get_toggle_name(TclToggle toggle);
const char* tcl_ir_get_extra_name(TclExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t tcl_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* tcl_ir_get_option_label(void);

/** Display name of one variant. */
const char* tcl_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* tcl_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
