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
//  - HaierModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - HaierTogglePowerOff must stay the FIRST toggle.
//  - At most HAIER_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in HaierExtra.
// --------------------------------------------------------------------------

// IR carrier
#define HAIER_IR_CARRIER_FREQ 38000
#define HAIER_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define HAIER_IR_MAX_TIMINGS 250

// Temperature range (Celsius)
#define HAIER_TEMP_MIN 16
#define HAIER_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define HAIER_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define HAIER_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define HAIER_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    HaierModeOff = 0,
    HaierModeCool,
    HaierModeAuto,
    HaierModeDry,
    HaierModeHeat,
    HaierModeFan,
    HaierModeCount
} HaierMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    HaierFanAuto = 0,
    HaierFanLow,
    HaierFanMedium,
    HaierFanHigh,
    HaierFanCount
} HaierFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    HaierTogglePowerOff = 0,
    HaierToggleSwing, // vertical vane auto
    HaierToggleTurbo,
    HaierToggleQuiet,
    HaierToggleHealth, // ioniser
    HaierToggleSleep,
    HaierToggleCount
} HaierToggle;

// Less common commands, listed on the Extra screen
// Fixed vane positions and the horizontal vane.
typedef enum {
    HaierExtraVaneTop = 0,
    HaierExtraVaneMiddle,
    HaierExtraVaneBottom,
    HaierExtraVaneDown,
    HaierExtraSwingHAuto,
    HaierExtraSwingHMiddle,
    HaierExtraCount
} HaierExtra;

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
    HaierMode mode;
    HaierFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = HaierToggle i currently believed on
    uint8_t option; // protocol variant, see haier_ir_get_option_count()
} HaierRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for HaierModeOff (send the power-off toggle)
 */
bool haier_ir_encode_state(const HaierRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool haier_ir_encode_toggle(
    const HaierRequest* req,
    HaierToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool haier_ir_encode_extra(
    const HaierRequest* req,
    HaierExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit HAIER_CODE_STR_LEN including the NUL.
 */
void haier_ir_format_state(const HaierRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void haier_ir_format_toggle(const HaierRequest* req, HaierToggle toggle, char* out, size_t len);

/** Same, for an Extra-screen entry. */
void haier_ir_format_extra(const HaierRequest* req, HaierExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool haier_ir_toggle_is_momentary(HaierToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool haier_ir_mode_locks_fan(HaierMode mode);

/** Whether this mode carries no temperature setpoint. */
bool haier_ir_mode_has_no_temp(HaierMode mode);

/** Display names. */
const char* haier_ir_get_mode_name(HaierMode mode);
const char* haier_ir_get_fan_name(HaierFan fan);
const char* haier_ir_get_toggle_name(HaierToggle toggle);
const char* haier_ir_get_extra_name(HaierExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t haier_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* haier_ir_get_option_label(void);

/** Display name of one variant. */
const char* haier_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* haier_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
