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
//  - GreeModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - GreeTogglePowerOff must stay the FIRST toggle.
//  - At most GREE_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in GreeExtra.
// --------------------------------------------------------------------------

// IR carrier
#define GREE_IR_CARRIER_FREQ 38000
#define GREE_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define GREE_IR_MAX_TIMINGS 160

// YAW1F sets an extra "model A" bit in byte 2; the others leave it clear.
typedef enum {
    GreeModelYAW1F = 0, // Ultimate, EKOKAI, RusClimate (default)
    GreeModelYBOFB, // Green, YBOFB2, YAPOF3
    GreeModelYX1FSF, // Soleus Air window unit
    GreeModelCount
} GreeModel;

// Temperature range (Celsius)
#define GREE_TEMP_MIN 16
#define GREE_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define GREE_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define GREE_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define GREE_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    GreeModeOff = 0,
    GreeModeCool,
    GreeModeAuto,
    GreeModeDry,
    GreeModeHeat,
    GreeModeFan,
    GreeModeCount
} GreeMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    GreeFanAuto = 0,
    GreeFanLow,
    GreeFanMedium,
    GreeFanHigh,
    GreeFanCount
} GreeFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    GreeTogglePowerOff = 0,
    GreeToggleSwing, // vertical vane auto-swing
    GreeToggleTurbo,
    GreeToggleLight, // display backlight
    GreeToggleXfan, // dries the coil after shutdown
    GreeToggleSleep,
    GreeToggleEcono,
    GreeToggleCount
} GreeToggle;

// Less common commands, listed on the Extra screen
// Fixed vane positions, which the main screen's Swing button cannot express.
typedef enum {
    GreeExtraVaneAuto = 0,
    GreeExtraVaneUp,
    GreeExtraVaneMidUp,
    GreeExtraVaneMiddle,
    GreeExtraVaneMidDown,
    GreeExtraVaneDown,
    GreeExtraCount
} GreeExtra;

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
    GreeMode mode;
    GreeFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = GreeToggle i currently believed on
    uint8_t option; // protocol variant, see gree_ir_get_option_count()
} GreeRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for GreeModeOff (send the power-off toggle)
 */
bool gree_ir_encode_state(const GreeRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool gree_ir_encode_toggle(
    const GreeRequest* req,
    GreeToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool gree_ir_encode_extra(
    const GreeRequest* req,
    GreeExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit GREE_CODE_STR_LEN including the NUL.
 */
void gree_ir_format_state(const GreeRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void gree_ir_format_toggle(const GreeRequest* req, GreeToggle toggle, char* out, size_t len);

/** Same, for an Extra-screen entry. */
void gree_ir_format_extra(const GreeRequest* req, GreeExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool gree_ir_toggle_is_momentary(GreeToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool gree_ir_mode_locks_fan(GreeMode mode);

/** Whether this mode carries no temperature setpoint. */
bool gree_ir_mode_has_no_temp(GreeMode mode);

/** Display names. */
const char* gree_ir_get_mode_name(GreeMode mode);
const char* gree_ir_get_fan_name(GreeFan fan);
const char* gree_ir_get_toggle_name(GreeToggle toggle);
const char* gree_ir_get_extra_name(GreeExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t gree_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* gree_ir_get_option_label(void);

/** Display name of one variant. */
const char* gree_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* gree_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
