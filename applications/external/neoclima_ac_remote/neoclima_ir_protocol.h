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
//  - NeoclimaModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - NeoclimaTogglePowerOff must stay the FIRST toggle.
//  - At most NEOCLIMA_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in NeoclimaExtra.
// --------------------------------------------------------------------------

// IR carrier
#define NEOCLIMA_IR_CARRIER_FREQ 38000
#define NEOCLIMA_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define NEOCLIMA_IR_MAX_TIMINGS 220

// Temperature range (Celsius)
#define NEOCLIMA_TEMP_MIN 16
#define NEOCLIMA_TEMP_MAX 32

// Main screen fits 6 toggle buttons before it collides with the footer
#define NEOCLIMA_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define NEOCLIMA_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define NEOCLIMA_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    NeoclimaModeOff = 0,
    NeoclimaModeCool,
    NeoclimaModeAuto,
    NeoclimaModeDry,
    NeoclimaModeHeat,
    NeoclimaModeFan,
    NeoclimaModeCount
} NeoclimaMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    NeoclimaFanAuto = 0,
    NeoclimaFanLow,
    NeoclimaFanMedium,
    NeoclimaFanHigh,
    NeoclimaFanCount
} NeoclimaFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    NeoclimaTogglePowerOff = 0,
    NeoclimaToggleSwing, // vertical vane
    NeoclimaToggleTurbo,
    NeoclimaToggleLight, // display backlight
    NeoclimaToggleEcono,
    NeoclimaToggleIon,
    NeoclimaToggleCount
} NeoclimaToggle;

// Less common commands, listed on the Extra screen
typedef enum {
    NeoclimaExtraSwingH = 0,
    NeoclimaExtraFresh,
    NeoclimaExtraEye,
    NeoclimaExtraHold,
    NeoclimaExtra8CHeat,
    NeoclimaExtraCount
} NeoclimaExtra;

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
    NeoclimaMode mode;
    NeoclimaFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = NeoclimaToggle i currently believed on
    uint8_t option; // protocol variant, see neoclima_ir_get_option_count()
} NeoclimaRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for NeoclimaModeOff (send the power-off toggle)
 */
bool neoclima_ir_encode_state(const NeoclimaRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool neoclima_ir_encode_toggle(
    const NeoclimaRequest* req,
    NeoclimaToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool neoclima_ir_encode_extra(
    const NeoclimaRequest* req,
    NeoclimaExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit NEOCLIMA_CODE_STR_LEN including the NUL.
 */
void neoclima_ir_format_state(const NeoclimaRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void neoclima_ir_format_toggle(
    const NeoclimaRequest* req,
    NeoclimaToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void neoclima_ir_format_extra(
    const NeoclimaRequest* req,
    NeoclimaExtra extra,
    char* out,
    size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool neoclima_ir_toggle_is_momentary(NeoclimaToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool neoclima_ir_mode_locks_fan(NeoclimaMode mode);

/** Whether this mode carries no temperature setpoint. */
bool neoclima_ir_mode_has_no_temp(NeoclimaMode mode);

/** Display names. */
const char* neoclima_ir_get_mode_name(NeoclimaMode mode);
const char* neoclima_ir_get_fan_name(NeoclimaFan fan);
const char* neoclima_ir_get_toggle_name(NeoclimaToggle toggle);
const char* neoclima_ir_get_extra_name(NeoclimaExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t neoclima_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* neoclima_ir_get_option_label(void);

/** Display name of one variant. */
const char* neoclima_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* neoclima_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
