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
//  - DelonghiModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - DelonghiTogglePowerOff must stay the FIRST toggle.
//  - At most DELONGHI_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in DelonghiExtra.
// --------------------------------------------------------------------------

// IR carrier
#define DELONGHI_IR_CARRIER_FREQ 38000
#define DELONGHI_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define DELONGHI_IR_MAX_TIMINGS 150

// Temperature range (Celsius)
#define DELONGHI_TEMP_MIN 18
#define DELONGHI_TEMP_MAX 32

// Main screen fits 6 toggle buttons before it collides with the footer
#define DELONGHI_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define DELONGHI_DEFAULT_TOGGLE_TEMP 23

// Buffer for the short payload string shown on the Extra screen
#define DELONGHI_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    DelonghiModeOff = 0,
    DelonghiModeCool,
    DelonghiModeAuto,
    DelonghiModeDry,
    DelonghiModeFan,
    DelonghiModeCount
} DelonghiMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    DelonghiFanAuto = 0,
    DelonghiFanLow,
    DelonghiFanMedium,
    DelonghiFanHigh,
    DelonghiFanCount
} DelonghiFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    DelonghiTogglePowerOff = 0,
    DelonghiToggleBoost, // "Turbo" on the handset
    DelonghiToggleSleep,
    DelonghiToggleCount
} DelonghiToggle;

// Less common commands, listed on the Extra screen
typedef enum {
    DelonghiExtraOffTimerCancel = 0,
    DelonghiExtraOffTimer1h,
    DelonghiExtraOffTimer2h,
    DelonghiExtraOffTimer4h,
    DelonghiExtraOffTimer8h,
    DelonghiExtraCount
} DelonghiExtra;

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
    DelonghiMode mode;
    DelonghiFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = DelonghiToggle i currently believed on
    uint8_t option; // protocol variant, see delonghi_ir_get_option_count()
} DelonghiRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for DelonghiModeOff (send the power-off toggle)
 */
bool delonghi_ir_encode_state(const DelonghiRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool delonghi_ir_encode_toggle(
    const DelonghiRequest* req,
    DelonghiToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool delonghi_ir_encode_extra(
    const DelonghiRequest* req,
    DelonghiExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit DELONGHI_CODE_STR_LEN including the NUL.
 */
void delonghi_ir_format_state(const DelonghiRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void delonghi_ir_format_toggle(
    const DelonghiRequest* req,
    DelonghiToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void delonghi_ir_format_extra(
    const DelonghiRequest* req,
    DelonghiExtra extra,
    char* out,
    size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool delonghi_ir_toggle_is_momentary(DelonghiToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool delonghi_ir_mode_locks_fan(DelonghiMode mode);

/** Whether this mode carries no temperature setpoint. */
bool delonghi_ir_mode_has_no_temp(DelonghiMode mode);

/** Display names. */
const char* delonghi_ir_get_mode_name(DelonghiMode mode);
const char* delonghi_ir_get_fan_name(DelonghiFan fan);
const char* delonghi_ir_get_toggle_name(DelonghiToggle toggle);
const char* delonghi_ir_get_extra_name(DelonghiExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t delonghi_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* delonghi_ir_get_option_label(void);

/** Display name of one variant. */
const char* delonghi_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* delonghi_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
