#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Goodweather, 48-bit frame (IRremoteESP8266 GOODWEATHER).
//
// Also sold as Rapid, among others. Two things make this protocol unlike
// everything else in this repo:
//
//  - Logic 1 is the SHORT space and logic 0 the long one, the reverse of
//    every other protocol here.
//  - Each byte goes out followed by its own bitwise inverse, so a 48-bit
//    payload occupies 96 bits on the wire.
//
//   byte 1   bit 0 light, bit 3 turbo
//   byte 2   bits 0-3 the button the handset believes was pressed
//   byte 3   bit 0 sleep, bit 1 power, bits 2-3 swing, bit 4 air flow,
//            bits 5-6 fan
//   byte 4   bits 0-3 temperature - 16, bits 5-7 mode
//
// Bytes 0 and 5, and the top bit of byte 2, are not modelled by
// IRremoteESP8266. Their values here come from a real capture off a Rapid
// handset rather than being left at zero.
//
// This is the ONLY file (with its .c) that carries protocol knowledge. The
// app shell and all three views drive themselves off the enums and the name
// getters below, so a port never has to edit a view.
//
// Rules that keep the generic UI working:
//  - GoodweatherModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - GoodweatherTogglePowerOff must stay the FIRST toggle.
//  - At most GOODWEATHER_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in GoodweatherExtra.
// --------------------------------------------------------------------------

// IR carrier
#define GOODWEATHER_IR_CARRIER_FREQ 38000
#define GOODWEATHER_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define GOODWEATHER_IR_MAX_TIMINGS 220

// Temperature range (Celsius)
#define GOODWEATHER_TEMP_MIN 16
#define GOODWEATHER_TEMP_MAX 31

// Main screen fits 6 toggle buttons before it collides with the footer
#define GOODWEATHER_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define GOODWEATHER_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define GOODWEATHER_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    GoodweatherModeOff = 0,
    GoodweatherModeCool,
    GoodweatherModeAuto,
    GoodweatherModeDry,
    GoodweatherModeHeat,
    GoodweatherModeFan,
    GoodweatherModeCount
} GoodweatherMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    GoodweatherFanAuto = 0,
    GoodweatherFanLow,
    GoodweatherFanMedium,
    GoodweatherFanHigh,
    GoodweatherFanCount
} GoodweatherFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    GoodweatherTogglePowerOff = 0,
    GoodweatherToggleSwing,
    GoodweatherToggleTurbo,
    GoodweatherToggleLight,
    GoodweatherToggleSleep,
    GoodweatherToggleAirFlow,
    GoodweatherToggleCount
} GoodweatherToggle;

// Less common commands, listed on the Extra screen.
// The vane has two speeds as well as off, which the main screen's single
// Swing button cannot express.
typedef enum {
    GoodweatherExtraSwingFast = 0,
    GoodweatherExtraSwingSlow,
    GoodweatherExtraSwingOff,
    GoodweatherExtraHold,
    GoodweatherExtraTimer,
    GoodweatherExtraCount
} GoodweatherExtra;

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
    GoodweatherMode mode;
    GoodweatherFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = GoodweatherToggle i currently believed on
    uint8_t option; // protocol variant, see goodweather_ir_get_option_count()
} GoodweatherRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for GoodweatherModeOff (send the power-off toggle)
 */
bool goodweather_ir_encode_state(
    const GoodweatherRequest* req,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool goodweather_ir_encode_toggle(
    const GoodweatherRequest* req,
    GoodweatherToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool goodweather_ir_encode_extra(
    const GoodweatherRequest* req,
    GoodweatherExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit GOODWEATHER_CODE_STR_LEN including the NUL.
 */
void goodweather_ir_format_state(const GoodweatherRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void goodweather_ir_format_toggle(
    const GoodweatherRequest* req,
    GoodweatherToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void goodweather_ir_format_extra(
    const GoodweatherRequest* req,
    GoodweatherExtra extra,
    char* out,
    size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool goodweather_ir_toggle_is_momentary(GoodweatherToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool goodweather_ir_mode_locks_fan(GoodweatherMode mode);

/** Whether this mode carries no temperature setpoint. */
bool goodweather_ir_mode_has_no_temp(GoodweatherMode mode);

/** Display names. */
const char* goodweather_ir_get_mode_name(GoodweatherMode mode);
const char* goodweather_ir_get_fan_name(GoodweatherFan fan);
const char* goodweather_ir_get_toggle_name(GoodweatherToggle toggle);
const char* goodweather_ir_get_extra_name(GoodweatherExtra extra);

/**
 * Some protocols cover several incompatible handset variants that differ in
 * frame length, checksum or field encoding, and nothing in the signal says
 * which one a given unit expects. Expose them here and the Setup screen grows
 * a picker for them.
 *
 * @return number of variants, or 0 when the protocol has only one form
 */
uint8_t goodweather_ir_get_option_count(void);

/** Short label for the picker, e.g. "Model". Only used when count > 0. */
const char* goodweather_ir_get_option_label(void);

/** Display name of one variant. */
const char* goodweather_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* goodweather_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
