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
//  - PanasonicModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - PanasonicTogglePowerOff must stay the FIRST toggle.
//  - At most PANASONIC_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in PanasonicExtra.
// --------------------------------------------------------------------------

// IR carrier
#define PANASONIC_IR_CARRIER_FREQ 38000
#define PANASONIC_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define PANASONIC_IR_MAX_TIMINGS 460

// Handset variants. They differ in a handful of fixed bytes, and CKP/RKR
// additionally SWAP the Quiet and Powerful bit positions.
typedef enum {
    PanasonicModelJke = 0, // plain baseline
    PanasonicModelLke,
    PanasonicModelNke,
    PanasonicModelDke,
    PanasonicModelCkp,
    PanasonicModelRkr,
    PanasonicModelCount
} PanasonicModel;

// Temperature range (Celsius)
#define PANASONIC_TEMP_MIN 16
#define PANASONIC_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define PANASONIC_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define PANASONIC_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define PANASONIC_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    PanasonicModeOff = 0,
    PanasonicModeCool,
    PanasonicModeAuto,
    PanasonicModeDry,
    PanasonicModeHeat,
    PanasonicModeFan,
    PanasonicModeCount
} PanasonicMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    PanasonicFanAuto = 0,
    PanasonicFanLow,
    PanasonicFanMedium,
    PanasonicFanHigh,
    PanasonicFanCount
} PanasonicFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    PanasonicTogglePowerOff = 0,
    PanasonicToggleSwing, // vertical vane auto
    PanasonicToggleQuiet,
    PanasonicTogglePowerful, // mutually exclusive with Quiet on the handset
    PanasonicToggleCount
} PanasonicToggle;

// Less common commands, listed on the Extra screen
// Fixed vane positions, which the Swing button cannot express.
typedef enum {
    PanasonicExtraVaneHighest = 0,
    PanasonicExtraVaneHigh,
    PanasonicExtraVaneMiddle,
    PanasonicExtraVaneLow,
    PanasonicExtraVaneLowest,
    PanasonicExtraCount
} PanasonicExtra;

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
    PanasonicMode mode;
    PanasonicFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = PanasonicToggle i currently believed on
    uint8_t option; // protocol variant, see panasonic_ir_get_option_count()
} PanasonicRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for PanasonicModeOff (send the power-off toggle)
 */
bool panasonic_ir_encode_state(
    const PanasonicRequest* req,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool panasonic_ir_encode_toggle(
    const PanasonicRequest* req,
    PanasonicToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool panasonic_ir_encode_extra(
    const PanasonicRequest* req,
    PanasonicExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit PANASONIC_CODE_STR_LEN including the NUL.
 */
void panasonic_ir_format_state(const PanasonicRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void panasonic_ir_format_toggle(
    const PanasonicRequest* req,
    PanasonicToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void panasonic_ir_format_extra(
    const PanasonicRequest* req,
    PanasonicExtra extra,
    char* out,
    size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool panasonic_ir_toggle_is_momentary(PanasonicToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool panasonic_ir_mode_locks_fan(PanasonicMode mode);

/** Whether this mode carries no temperature setpoint. */
bool panasonic_ir_mode_has_no_temp(PanasonicMode mode);

/** Display names. */
const char* panasonic_ir_get_mode_name(PanasonicMode mode);
const char* panasonic_ir_get_fan_name(PanasonicFan fan);
const char* panasonic_ir_get_toggle_name(PanasonicToggle toggle);
const char* panasonic_ir_get_extra_name(PanasonicExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t panasonic_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* panasonic_ir_get_option_label(void);

/** Display name of one variant. */
const char* panasonic_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* panasonic_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
