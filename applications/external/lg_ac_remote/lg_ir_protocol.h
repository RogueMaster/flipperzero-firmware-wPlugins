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
//  - LgModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - LgTogglePowerOff must stay the FIRST toggle.
//  - At most LG_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in LgExtra.
// --------------------------------------------------------------------------

// IR carrier
#define LG_IR_CARRIER_FREQ 38000
#define LG_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define LG_IR_MAX_TIMINGS 80

// The word is identical between these, but LG2 handsets use a very different
// header: a short 3200us mark and a long 9900us space.
typedef enum {
    LgModelLg = 0, // GE6711AR2853M / LG6711A20083V
    LgModelLg2, // AKB75215403 / AKB74955603 / AKB73757604
    LgModelCount
} LgModel;

// Temperature range (Celsius)
#define LG_TEMP_MIN 16
#define LG_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define LG_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define LG_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define LG_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    LgModeOff = 0,
    LgModeCool,
    LgModeAuto,
    LgModeDry,
    LgModeHeat,
    LgModeFan,
    LgModeCount
} LgMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    LgFanAuto = 0,
    LgFanLow,
    LgFanMedium,
    LgFanHigh,
    LgFanCount
} LgFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    LgTogglePowerOff = 0,
    LgToggleSwing, // vertical vane oscillation on/off
    LgToggleLight, // display backlight
    LgToggleCount
} LgToggle;

// Less common commands, listed on the Extra screen
typedef enum {
    // Only commands the main screen cannot express. Turning vertical swing on
    // or off is the Swing button up there; these are the fixed vane positions
    // and the horizontal vane, which have no main-screen control.
    LgExtraVane1 = 0, // lowest
    LgExtraVane2,
    LgExtraVane3,
    LgExtraVane4,
    LgExtraVane5,
    LgExtraVane6, // highest
    LgExtraSwingHAuto,
    LgExtraSwingHOff,
    LgExtraCount
} LgExtra;

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
    LgMode mode;
    LgFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = LgToggle i currently believed on
    uint8_t option; // protocol variant, see lg_ir_get_option_count()
} LgRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for LgModeOff (send the power-off toggle)
 */
bool lg_ir_encode_state(const LgRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool lg_ir_encode_toggle(
    const LgRequest* req,
    LgToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool lg_ir_encode_extra(
    const LgRequest* req,
    LgExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit LG_CODE_STR_LEN including the NUL.
 */
void lg_ir_format_state(const LgRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void lg_ir_format_toggle(const LgRequest* req, LgToggle toggle, char* out, size_t len);

/** Same, for an Extra-screen entry. */
void lg_ir_format_extra(const LgRequest* req, LgExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool lg_ir_toggle_is_momentary(LgToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool lg_ir_mode_locks_fan(LgMode mode);

/** Whether this mode carries no temperature setpoint. */
bool lg_ir_mode_has_no_temp(LgMode mode);

/** Display names. */
const char* lg_ir_get_mode_name(LgMode mode);
const char* lg_ir_get_fan_name(LgFan fan);
const char* lg_ir_get_toggle_name(LgToggle toggle);
const char* lg_ir_get_extra_name(LgExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t lg_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* lg_ir_get_option_label(void);

/** Display name of one variant. */
const char* lg_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* lg_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
