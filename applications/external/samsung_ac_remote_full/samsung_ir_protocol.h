#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Samsung, 14-byte frame (IRremoteESP8266 SAMSUNG_AC).
//
// Unusual on the wire: a short 690/17844 lead-in, then the state in 7-byte
// sections, each with its own 3086/8864 header and closed by a 2.9 ms gap.
//
//   byte 1     high nibble: low half of the section 1 checksum
//   byte 2     low nibble: high half of the section 1 checksum
//   byte 5     bit 4 sleep, bit 5 quiet
//   byte 6     bits 4-5 power (first copy)
//   byte 8     high nibble: low half of the section 2 checksum
//   byte 9     low nibble: high half of the section 2 checksum,
//              bits 4-6 swing
//   byte 10    bits 1-3 special fan (powerful/breeze/econo), bit 4 display
//   byte 11    bit 0 ion, bit 1 clean toggle, bits 4-7 temperature - 16
//   byte 12    bits 1-3 fan, bits 4-6 mode
//   byte 13    bit 2 beep toggle, bits 4-5 power (second copy)
//
// Each section carries a checksum built from population counts rather than
// sums, and it deliberately skips the two nibbles it is stored in.
//
// This is the ONLY file (with its .c) that carries protocol knowledge. The
// app shell and all three views drive themselves off the enums and the name
// getters below, so a port never has to edit a view.
//
// Rules that keep the generic UI working:
//  - SamsungModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - SamsungTogglePowerOff must stay the FIRST toggle.
//  - At most SAMSUNG_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in SamsungExtra.
// --------------------------------------------------------------------------

// IR carrier
#define SAMSUNG_IR_CARRIER_FREQ 38000
#define SAMSUNG_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define SAMSUNG_IR_MAX_TIMINGS 700

// Temperature range (Celsius)
#define SAMSUNG_TEMP_MIN 16
#define SAMSUNG_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define SAMSUNG_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define SAMSUNG_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define SAMSUNG_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    SamsungModeOff = 0,
    SamsungModeCool,
    SamsungModeAuto,
    SamsungModeDry,
    SamsungModeHeat,
    SamsungModeFan,
    SamsungModeCount
} SamsungMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    SamsungFanAuto = 0,
    SamsungFanLow,
    SamsungFanMedium,
    SamsungFanHigh,
    SamsungFanTurbo,
    SamsungFanCount
} SamsungFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    SamsungTogglePowerOff = 0,
    SamsungToggleSwingV, // vertical vane sweeps
    SamsungToggleSwingH, // horizontal vane sweeps
    SamsungToggleQuiet,
    SamsungToggleIon, // Virus Doctor / plasma ioniser
    SamsungToggleDisplay, // panel backlight
    SamsungToggleSleep,
    SamsungToggleCount
} SamsungToggle;

// Less common commands, listed on the Extra screen.
// Powerful, WindFree and Econo share one three-bit field, so they are
// mutually exclusive and belong here rather than as main-screen toggles.
typedef enum {
    SamsungExtraPowerful = 0,
    SamsungExtraBreeze, // WindFree
    SamsungExtraEcono,
    SamsungExtraSpecialOff,
    SamsungExtraSwingBoth,
    SamsungExtraSwingNone,
    SamsungExtraClean, // self-clean toggle
    SamsungExtraBeep, // beep toggle
    SamsungExtraCount
} SamsungExtra;

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
    SamsungMode mode;
    SamsungFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = SamsungToggle i currently believed on
    uint8_t option; // protocol variant, see samsung_ir_get_option_count()
} SamsungRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for SamsungModeOff (send the power-off toggle)
 */
bool samsung_ir_encode_state(const SamsungRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool samsung_ir_encode_toggle(
    const SamsungRequest* req,
    SamsungToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool samsung_ir_encode_extra(
    const SamsungRequest* req,
    SamsungExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit SAMSUNG_CODE_STR_LEN including the NUL.
 */
void samsung_ir_format_state(const SamsungRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void samsung_ir_format_toggle(
    const SamsungRequest* req,
    SamsungToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void samsung_ir_format_extra(const SamsungRequest* req, SamsungExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool samsung_ir_toggle_is_momentary(SamsungToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool samsung_ir_mode_locks_fan(SamsungMode mode);

/** Whether this mode carries no temperature setpoint. */
bool samsung_ir_mode_has_no_temp(SamsungMode mode);

/** Display names. */
const char* samsung_ir_get_mode_name(SamsungMode mode);
const char* samsung_ir_get_fan_name(SamsungFan fan);
const char* samsung_ir_get_toggle_name(SamsungToggle toggle);
const char* samsung_ir_get_extra_name(SamsungExtra extra);

/**
 * Some protocols cover several incompatible handset variants that differ in
 * frame length, checksum or field encoding, and nothing in the signal says
 * which one a given unit expects. Expose them here and the Setup screen grows
 * a picker for them.
 *
 * @return number of variants, or 0 when the protocol has only one form
 */
uint8_t samsung_ir_get_option_count(void);

/** Short label for the picker, e.g. "Model". Only used when count > 0. */
const char* samsung_ir_get_option_label(void);

/** Display name of one variant. */
const char* samsung_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* samsung_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
