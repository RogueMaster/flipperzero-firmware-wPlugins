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
//  - DaikinModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - DaikinTogglePowerOff must stay the FIRST toggle.
//  - At most DAIKIN_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in DaikinExtra.
// --------------------------------------------------------------------------

// IR carrier
#define DAIKIN_IR_CARRIER_FREQ 38000
#define DAIKIN_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define DAIKIN_IR_MAX_TIMINGS 700

// Temperature range (Celsius)
#define DAIKIN_TEMP_MIN 10
#define DAIKIN_TEMP_MAX 32

// Main screen fits 6 toggle buttons before it collides with the footer
#define DAIKIN_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define DAIKIN_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define DAIKIN_CODE_STR_LEN 24

/// Daikin ships eight incompatible frame formats. Nothing in a received
/// signal tells the unit which one it wants, so the Setup screen offers a
/// picker and the choice is saved to the SD card.
///
/// The names match what AC Detector reports, so a user can read the format
/// off the detector and pick the same entry here.
typedef enum {
    DaikinModelArc433 = 0, ///< ARC433 / ARC466, 35 bytes, three sections
    DaikinModelArc477, ///< ARC477A1 (FTXZ), 39 bytes
    DaikinModel216, ///< ARC433B69 / ARC484A4, 27 bytes
    DaikinModel160, ///< ARC423A5, 20 bytes
    DaikinModel176, ///< BRC4C151 / BRC4C153, 22 bytes
    DaikinModel152, ///< ARC480A5, 19 bytes
    DaikinModel128, ///< BRC52B63 / 17 series, 16 bytes
    DaikinModel64, ///< DGS01, a single 64-bit word
    DaikinModelCount
} DaikinModel;

// Operating modes. Off must be first.
typedef enum {
    DaikinModeOff = 0,
    DaikinModeCool,
    DaikinModeAuto,
    DaikinModeDry,
    DaikinModeHeat,
    DaikinModeFan,
    DaikinModeCount
} DaikinMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    DaikinFanAuto = 0,
    DaikinFanLow,
    DaikinFanMedium,
    DaikinFanHigh,
    DaikinFanCount
} DaikinFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    DaikinTogglePowerOff = 0,
    DaikinToggleSwing, // vertical vane
    DaikinTogglePowerful,
    DaikinToggleQuiet,
    DaikinToggleEcono,
    DaikinToggleMold, // dries the coil after shutdown
    DaikinToggleCount
} DaikinToggle;

// Less common commands, listed on the Extra screen
typedef enum {
    DaikinExtraSwingHOn = 0,
    DaikinExtraSwingHOff,
    DaikinExtraSensor,
    DaikinExtraCount
} DaikinExtra;

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
    DaikinMode mode;
    DaikinFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = DaikinToggle i currently believed on
    uint8_t option; // protocol variant, see daikin_ir_get_option_count()
} DaikinRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for DaikinModeOff (send the power-off toggle)
 */
bool daikin_ir_encode_state(const DaikinRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool daikin_ir_encode_toggle(
    const DaikinRequest* req,
    DaikinToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool daikin_ir_encode_extra(
    const DaikinRequest* req,
    DaikinExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit DAIKIN_CODE_STR_LEN including the NUL.
 */
void daikin_ir_format_state(const DaikinRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void daikin_ir_format_toggle(const DaikinRequest* req, DaikinToggle toggle, char* out, size_t len);

/** Same, for an Extra-screen entry. */
void daikin_ir_format_extra(const DaikinRequest* req, DaikinExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool daikin_ir_toggle_is_momentary(DaikinToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool daikin_ir_mode_locks_fan(DaikinMode mode);

/** Whether this mode carries no temperature setpoint. */
bool daikin_ir_mode_has_no_temp(DaikinMode mode);

/** Display names. */
const char* daikin_ir_get_mode_name(DaikinMode mode);
const char* daikin_ir_get_fan_name(DaikinFan fan);
const char* daikin_ir_get_toggle_name(DaikinToggle toggle);
const char* daikin_ir_get_extra_name(DaikinExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t daikin_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* daikin_ir_get_option_label(void);

/** Display name of one variant. */
const char* daikin_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* daikin_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
