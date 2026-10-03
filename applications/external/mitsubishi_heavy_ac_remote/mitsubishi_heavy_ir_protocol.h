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
//  - MitsubishiHeavyModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - MitsubishiHeavyTogglePowerOff must stay the FIRST toggle.
//  - At most MITSUBISHI_HEAVY_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in MitsubishiHeavyExtra.
// --------------------------------------------------------------------------

// IR carrier
#define MITSUBISHI_HEAVY_IR_CARRIER_FREQ 38000
#define MITSUBISHI_HEAVY_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define MITSUBISHI_HEAVY_IR_MAX_TIMINGS 330

// Temperature range (Celsius)
#define MITSUBISHI_HEAVY_TEMP_MIN 17
#define MITSUBISHI_HEAVY_TEMP_MAX 31

// Main screen fits 6 toggle buttons before it collides with the footer
#define MITSUBISHI_HEAVY_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define MITSUBISHI_HEAVY_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define MITSUBISHI_HEAVY_CODE_STR_LEN 24

/// Mitsubishi Heavy ships two frame formats on the same line coding: the
/// 19-byte ZM-S and the 11-byte ZJ-S. Nothing in a received signal says which
/// a unit wants, so the Setup screen offers a picker.
typedef enum {
    MitsubishiHeavyModelZms = 0, ///< ZM-S, 19 bytes
    MitsubishiHeavyModelZjs, ///< ZJ-S, 11 bytes
    MitsubishiHeavyModelCount
} MitsubishiHeavyModel;

// Operating modes. Off must be first.
typedef enum {
    MitsubishiHeavyModeOff = 0,
    MitsubishiHeavyModeCool,
    MitsubishiHeavyModeAuto,
    MitsubishiHeavyModeDry,
    MitsubishiHeavyModeHeat,
    MitsubishiHeavyModeFan,
    MitsubishiHeavyModeCount
} MitsubishiHeavyMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    MitsubishiHeavyFanAuto = 0,
    MitsubishiHeavyFanLow,
    MitsubishiHeavyFanMedium,
    MitsubishiHeavyFanHigh,
    MitsubishiHeavyFanCount
} MitsubishiHeavyFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    MitsubishiHeavyTogglePowerOff = 0,
    MitsubishiHeavyToggleSwing, // vertical vane auto-swing
    MitsubishiHeavyToggleClean,
    MitsubishiHeavyToggleFilter,
    MitsubishiHeavyToggleNight,
    MitsubishiHeavyToggleSilent,
    MitsubishiHeavyToggleCount
} MitsubishiHeavyToggle;

// Less common commands, listed on the Extra screen
// Fixed vane positions and the horizontal vane.
typedef enum {
    MitsubishiHeavyExtraVaneHighest = 0,
    MitsubishiHeavyExtraVaneHigh,
    MitsubishiHeavyExtraVaneMiddle,
    MitsubishiHeavyExtraVaneLow,
    MitsubishiHeavyExtraVaneLowest,
    MitsubishiHeavyExtraSwingHAuto,
    MitsubishiHeavyExtraSwingHOff,
    MitsubishiHeavyExtraCount
} MitsubishiHeavyExtra;

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
    MitsubishiHeavyMode mode;
    MitsubishiHeavyFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = MitsubishiHeavyToggle i currently believed on
    uint8_t option; // protocol variant, see mitsubishi_heavy_ir_get_option_count()
} MitsubishiHeavyRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for MitsubishiHeavyModeOff (send the power-off toggle)
 */
bool mitsubishi_heavy_ir_encode_state(
    const MitsubishiHeavyRequest* req,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool mitsubishi_heavy_ir_encode_toggle(
    const MitsubishiHeavyRequest* req,
    MitsubishiHeavyToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool mitsubishi_heavy_ir_encode_extra(
    const MitsubishiHeavyRequest* req,
    MitsubishiHeavyExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit MITSUBISHI_HEAVY_CODE_STR_LEN including the NUL.
 */
void mitsubishi_heavy_ir_format_state(const MitsubishiHeavyRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void mitsubishi_heavy_ir_format_toggle(
    const MitsubishiHeavyRequest* req,
    MitsubishiHeavyToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void mitsubishi_heavy_ir_format_extra(
    const MitsubishiHeavyRequest* req,
    MitsubishiHeavyExtra extra,
    char* out,
    size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool mitsubishi_heavy_ir_toggle_is_momentary(MitsubishiHeavyToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool mitsubishi_heavy_ir_mode_locks_fan(MitsubishiHeavyMode mode);

/** Whether this mode carries no temperature setpoint. */
bool mitsubishi_heavy_ir_mode_has_no_temp(MitsubishiHeavyMode mode);

/** Display names. */
const char* mitsubishi_heavy_ir_get_mode_name(MitsubishiHeavyMode mode);
const char* mitsubishi_heavy_ir_get_fan_name(MitsubishiHeavyFan fan);
const char* mitsubishi_heavy_ir_get_toggle_name(MitsubishiHeavyToggle toggle);
const char* mitsubishi_heavy_ir_get_extra_name(MitsubishiHeavyExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t mitsubishi_heavy_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* mitsubishi_heavy_ir_get_option_label(void);

/** Display name of one variant. */
const char* mitsubishi_heavy_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* mitsubishi_heavy_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
