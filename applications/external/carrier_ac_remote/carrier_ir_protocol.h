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
//  - CarrierModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - CarrierTogglePowerOff must stay the FIRST toggle.
//  - At most CARRIER_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in CarrierExtra.
// --------------------------------------------------------------------------

// IR carrier
#define CARRIER_IR_CARRIER_FREQ 38000
#define CARRIER_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define CARRIER_IR_MAX_TIMINGS 150

// Temperature range (Celsius)
#define CARRIER_TEMP_MIN 16
#define CARRIER_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define CARRIER_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define CARRIER_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define CARRIER_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    CarrierModeOff = 0,
    CarrierModeCool,
    CarrierModeHeat,
    CarrierModeFan,
    CarrierModeCount
} CarrierMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    CarrierFanAuto = 0,
    CarrierFanLow,
    CarrierFanMedium,
    CarrierFanHigh,
    CarrierFanCount
} CarrierFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    CarrierTogglePowerOff = 0,
    CarrierToggleSwing, // vertical vane
    CarrierToggleSleep,
    CarrierToggleCount
} CarrierToggle;

// Less common commands, listed on the Extra screen
typedef enum {
    CarrierExtraTimersOff = 0,
    CarrierExtraOff1h,
    CarrierExtraOff2h,
    CarrierExtraOff4h,
    CarrierExtraOff8h,
    CarrierExtraOn1h,
    CarrierExtraOn2h,
    CarrierExtraOn4h,
    CarrierExtraOn8h,
    CarrierExtraCount
} CarrierExtra;

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
    CarrierMode mode;
    CarrierFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = CarrierToggle i currently believed on
    uint8_t option; // protocol variant, see carrier_ir_get_option_count()
} CarrierRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for CarrierModeOff (send the power-off toggle)
 */
bool carrier_ir_encode_state(const CarrierRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool carrier_ir_encode_toggle(
    const CarrierRequest* req,
    CarrierToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool carrier_ir_encode_extra(
    const CarrierRequest* req,
    CarrierExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit CARRIER_CODE_STR_LEN including the NUL.
 */
void carrier_ir_format_state(const CarrierRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void carrier_ir_format_toggle(
    const CarrierRequest* req,
    CarrierToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void carrier_ir_format_extra(const CarrierRequest* req, CarrierExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool carrier_ir_toggle_is_momentary(CarrierToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool carrier_ir_mode_locks_fan(CarrierMode mode);

/** Whether this mode carries no temperature setpoint. */
bool carrier_ir_mode_has_no_temp(CarrierMode mode);

/** Display names. */
const char* carrier_ir_get_mode_name(CarrierMode mode);
const char* carrier_ir_get_fan_name(CarrierFan fan);
const char* carrier_ir_get_toggle_name(CarrierToggle toggle);
const char* carrier_ir_get_extra_name(CarrierExtra extra);

/**
 * Incompatible handset variants, if this protocol has any. Nothing in the
 * signal says which one a unit expects, so the Setup screen offers a picker
 * when this returns non-zero.
 */
uint8_t carrier_ir_get_option_count(void);

/** Short label for the picker. Only used when the count is non-zero. */
const char* carrier_ir_get_option_label(void);

/** Display name of one variant. */
const char* carrier_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* carrier_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
