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
//  - FujitsuModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - FujitsuTogglePowerOff must stay the FIRST toggle.
//  - At most FUJITSU_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in FujitsuExtra.
// --------------------------------------------------------------------------

// IR carrier
#define FUJITSU_IR_CARRIER_FREQ 38000
#define FUJITSU_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define FUJITSU_IR_MAX_TIMINGS 280

// Handset variants. They differ in frame length, checksum and, for ARREW4E,
// how the temperature is packed - nothing in the signal identifies which one
// a unit expects, so it is a user setting.
typedef enum {
    FujitsuModelARRAH2E = 0,
    FujitsuModelARREB1E,
    FujitsuModelARRY4,
    FujitsuModelARREW4E,
    FujitsuModelARDB1,
    FujitsuModelARJW2,
    FujitsuModelCount
} FujitsuModel;

// Temperature range (Celsius)
#define FUJITSU_TEMP_MIN 16
#define FUJITSU_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define FUJITSU_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define FUJITSU_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define FUJITSU_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    FujitsuModeOff = 0,
    FujitsuModeCool,
    FujitsuModeAuto,
    FujitsuModeDry,
    FujitsuModeHeat,
    FujitsuModeFan,
    FujitsuModeCount
} FujitsuMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    FujitsuFanAuto = 0,
    FujitsuFanLow,
    FujitsuFanMedium,
    FujitsuFanHigh,
    FujitsuFanCount
} FujitsuFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    FujitsuTogglePowerOff = 0,
    FujitsuToggleSwing, // vertical vane, a bit in the long frame
    FujitsuTogglePowerful, // momentary, sent as a short command
    FujitsuToggleEcono, // momentary, sent as a short command
    FujitsuToggleFilter,
    FujitsuToggleClean,
    FujitsuToggleCount
} FujitsuToggle;

// Less common commands, listed on the Extra screen
// Short command frames the long frame cannot express.
typedef enum {
    FujitsuExtraStepVert = 0,
    FujitsuExtraSwingVert,
    FujitsuExtraStepHoriz,
    FujitsuExtraSwingHoriz,
    FujitsuExtraCount
} FujitsuExtra;

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
    FujitsuMode mode;
    FujitsuFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = FujitsuToggle i currently believed on
    uint8_t option; // protocol variant, see fujitsu_ir_get_option_count()
} FujitsuRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for FujitsuModeOff (send the power-off toggle)
 */
bool fujitsu_ir_encode_state(const FujitsuRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool fujitsu_ir_encode_toggle(
    const FujitsuRequest* req,
    FujitsuToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool fujitsu_ir_encode_extra(
    const FujitsuRequest* req,
    FujitsuExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit FUJITSU_CODE_STR_LEN including the NUL.
 */
void fujitsu_ir_format_state(const FujitsuRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void fujitsu_ir_format_toggle(
    const FujitsuRequest* req,
    FujitsuToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void fujitsu_ir_format_extra(const FujitsuRequest* req, FujitsuExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool fujitsu_ir_toggle_is_momentary(FujitsuToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool fujitsu_ir_mode_locks_fan(FujitsuMode mode);

/** Whether this mode carries no temperature setpoint. */
bool fujitsu_ir_mode_has_no_temp(FujitsuMode mode);

/** Display names. */
const char* fujitsu_ir_get_mode_name(FujitsuMode mode);
const char* fujitsu_ir_get_fan_name(FujitsuFan fan);
const char* fujitsu_ir_get_toggle_name(FujitsuToggle toggle);
const char* fujitsu_ir_get_extra_name(FujitsuExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t fujitsu_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* fujitsu_ir_get_option_label(void);

/** Display name of one variant. */
const char* fujitsu_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* fujitsu_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
