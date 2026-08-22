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
//  - ToshibaModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - ToshibaTogglePowerOff must stay the FIRST toggle.
//  - At most TOSHIBA_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in ToshibaExtra.
// --------------------------------------------------------------------------

// IR carrier
#define TOSHIBA_IR_CARRIER_FREQ 38000
#define TOSHIBA_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define TOSHIBA_IR_MAX_TIMINGS 320

// Handset variants. The model rides in byte 2's high nibble; remote B covers
// the WA-TH03A / WA-TH04A family.
typedef enum {
    ToshibaModelRemoteA = 0,
    ToshibaModelRemoteB,
    ToshibaModelCount
} ToshibaModel;

// Temperature range (Celsius)
#define TOSHIBA_TEMP_MIN 17
#define TOSHIBA_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define TOSHIBA_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define TOSHIBA_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define TOSHIBA_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    ToshibaModeOff = 0,
    ToshibaModeCool,
    ToshibaModeAuto,
    ToshibaModeDry,
    ToshibaModeHeat,
    ToshibaModeFan,
    ToshibaModeCount
} ToshibaMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    ToshibaFanAuto = 0,
    ToshibaFanLow,
    ToshibaFanMedium,
    ToshibaFanHigh,
    ToshibaFanCount
} ToshibaFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    ToshibaTogglePowerOff = 0,
    ToshibaToggleSwing, // vertical vane oscillation
    ToshibaToggleFilter, // "Pure" / ioniser filter
    ToshibaToggleCount
} ToshibaToggle;

// Less common commands, listed on the Extra screen
// Swing commands the main screen's on/off button cannot express.
typedef enum {
    ToshibaExtraSwingStep = 0,
    ToshibaExtraSwingToggle,
    ToshibaExtraCount
} ToshibaExtra;

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
    ToshibaMode mode;
    ToshibaFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = ToshibaToggle i currently believed on
    uint8_t option; // protocol variant, see toshiba_ir_get_option_count()
} ToshibaRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for ToshibaModeOff (send the power-off toggle)
 */
bool toshiba_ir_encode_state(const ToshibaRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool toshiba_ir_encode_toggle(
    const ToshibaRequest* req,
    ToshibaToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool toshiba_ir_encode_extra(
    const ToshibaRequest* req,
    ToshibaExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit TOSHIBA_CODE_STR_LEN including the NUL.
 */
void toshiba_ir_format_state(const ToshibaRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void toshiba_ir_format_toggle(
    const ToshibaRequest* req,
    ToshibaToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void toshiba_ir_format_extra(const ToshibaRequest* req, ToshibaExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool toshiba_ir_toggle_is_momentary(ToshibaToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool toshiba_ir_mode_locks_fan(ToshibaMode mode);

/** Whether this mode carries no temperature setpoint. */
bool toshiba_ir_mode_has_no_temp(ToshibaMode mode);

/** Display names. */
const char* toshiba_ir_get_mode_name(ToshibaMode mode);
const char* toshiba_ir_get_fan_name(ToshibaFan fan);
const char* toshiba_ir_get_toggle_name(ToshibaToggle toggle);
const char* toshiba_ir_get_extra_name(ToshibaExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t toshiba_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* toshiba_ir_get_option_label(void);

/** Display name of one variant. */
const char* toshiba_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* toshiba_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
