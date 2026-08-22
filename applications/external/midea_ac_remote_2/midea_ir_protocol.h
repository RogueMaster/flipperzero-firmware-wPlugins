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
//  - MideaModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - MideaTogglePowerOff must stay the FIRST toggle.
//  - At most MIDEA_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in MideaExtra.
// --------------------------------------------------------------------------

// IR carrier
#define MIDEA_IR_CARRIER_FREQ 38000
#define MIDEA_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define MIDEA_IR_MAX_TIMINGS 220

// Kaysun units use a different word for vertical swing - and it happens to be
// the same one everyone else uses for Econo, so the two collide unless the
// right variant is picked.
typedef enum {
    MideaModelStandard = 0,
    MideaModelKaysun,
    MideaModelCount
} MideaModel;

// Temperature range (Celsius)
#define MIDEA_TEMP_MIN 17
#define MIDEA_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define MIDEA_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define MIDEA_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define MIDEA_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    MideaModeOff = 0,
    MideaModeCool,
    MideaModeAuto,
    MideaModeDry,
    MideaModeHeat,
    MideaModeFan,
    MideaModeCount
} MideaMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    MideaFanAuto = 0,
    MideaFanLow,
    MideaFanMedium,
    MideaFanHigh,
    MideaFanCount
} MideaFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    MideaTogglePowerOff = 0,
    MideaToggleSwing, // vertical vane
    MideaToggleTurbo,
    MideaToggleLight, // display backlight
    MideaToggleEcono,
    MideaToggleCount
} MideaToggle;

// Less common commands, listed on the Extra screen
typedef enum {
    MideaExtraQuietOn = 0,
    MideaExtraQuietOff,
    MideaExtraSelfClean,
    MideaExtra8CHeat, // only meaningful in Heat mode
    MideaExtraCount
} MideaExtra;

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
    MideaMode mode;
    MideaFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = MideaToggle i currently believed on
    uint8_t option; // protocol variant, see midea_ir_get_option_count()
} MideaRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for MideaModeOff (send the power-off toggle)
 */
bool midea_ir_encode_state(const MideaRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool midea_ir_encode_toggle(
    const MideaRequest* req,
    MideaToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool midea_ir_encode_extra(
    const MideaRequest* req,
    MideaExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit MIDEA_CODE_STR_LEN including the NUL.
 */
void midea_ir_format_state(const MideaRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void midea_ir_format_toggle(const MideaRequest* req, MideaToggle toggle, char* out, size_t len);

/** Same, for an Extra-screen entry. */
void midea_ir_format_extra(const MideaRequest* req, MideaExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool midea_ir_toggle_is_momentary(MideaToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool midea_ir_mode_locks_fan(MideaMode mode);

/** Whether this mode carries no temperature setpoint. */
bool midea_ir_mode_has_no_temp(MideaMode mode);

/** Display names. */
const char* midea_ir_get_mode_name(MideaMode mode);
const char* midea_ir_get_fan_name(MideaFan fan);
const char* midea_ir_get_toggle_name(MideaToggle toggle);
const char* midea_ir_get_extra_name(MideaExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t midea_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* midea_ir_get_option_label(void);

/** Display name of one variant. */
const char* midea_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* midea_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
