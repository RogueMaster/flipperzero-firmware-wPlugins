#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Kelon / Hisense, 48-bit frame (IRremoteESP8266 KELON).
//
// Sold under Hisense, Kelon and some AUX badges. Two things about it are
// unlike the other protocols in this repo:
//
//  - Power and vertical swing are TOGGLE bits, not absolute state. The frame
//    says "flip it", so the app cannot know or show which way the unit ended
//    up. Both are reported as momentary.
//  - The fan field runs backwards. Auto is 0, but then the wire value falls
//    as the speed rises: low is 3 and high is 1.
//
//   bits 0-15   preamble 0x83 0x06
//   bits 16-17  fan
//   bit  18     power toggle
//   bit  19     sleep
//   bits 20-22  dehumidifier grade, -2..+2 biased
//   bit  23     vertical swing toggle
//   bits 24-26  mode
//   bit  27     timer enable
//   bits 28-31  temperature - 18
//   bits 32-38  timer
//   bit  39     smart mode
//   bit  44     super cool, first copy
//   bit  47     super cool, second copy
//
// Sent least significant bit first, no checksum.
//
// This is the ONLY file (with its .c) that carries protocol knowledge. The
// app shell and all three views drive themselves off the enums and the name
// getters below, so a port never has to edit a view.
//
// Rules that keep the generic UI working:
//  - KelonModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - KelonTogglePowerOff must stay the FIRST toggle.
//  - At most KELON_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in KelonExtra.
// --------------------------------------------------------------------------

// IR carrier
#define KELON_IR_CARRIER_FREQ 38000
#define KELON_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define KELON_IR_MAX_TIMINGS 700

// Temperature range (Celsius)
#define KELON_TEMP_MIN 18
#define KELON_TEMP_MAX 32

// Main screen fits 6 toggle buttons before it collides with the footer
#define KELON_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define KELON_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define KELON_CODE_STR_LEN 24

/// Kelon ships two frame formats. Nothing in a received signal says which a
/// unit wants, so the Setup screen offers a picker.
typedef enum {
    KelonModel48 = 0, ///< RCH-R0Y3 and the ON/OFF handsets, 48 bits
    KelonModel168, ///< DG11R2-01, 21 bytes in three sections
    KelonModelCount
} KelonModel;

// Operating modes. Off must be first.
typedef enum {
    KelonModeOff = 0,
    KelonModeCool,
    KelonModeSmart, // the unit picks; the remote shows 26 C
    KelonModeDry,
    KelonModeHeat,
    KelonModeFan,
    KelonModeCount
} KelonMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    KelonFanAuto = 0,
    KelonFanLow,
    KelonFanMedium,
    KelonFanHigh,
    KelonFanCount
} KelonFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    KelonTogglePowerOff = 0,
    KelonToggleSwingV, // a toggle on the wire, so momentary here
    KelonToggleSleep,
    KelonToggleSuperCool,
    KelonToggleCount
} KelonToggle;

// Less common commands, listed on the Extra screen.
// The dehumidifier grade, which only does anything in Dry mode.
typedef enum {
    KelonExtraDryMinus2 = 0,
    KelonExtraDryMinus1,
    KelonExtraDryZero,
    KelonExtraDryPlus1,
    KelonExtraDryPlus2,
    KelonExtraCount
} KelonExtra;

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
    KelonMode mode;
    KelonFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = KelonToggle i currently believed on
    uint8_t option; // protocol variant, see kelon_ir_get_option_count()
} KelonRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for KelonModeOff (send the power-off toggle)
 */
bool kelon_ir_encode_state(const KelonRequest* req, uint32_t* timings, size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool kelon_ir_encode_toggle(
    const KelonRequest* req,
    KelonToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool kelon_ir_encode_extra(
    const KelonRequest* req,
    KelonExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit KELON_CODE_STR_LEN including the NUL.
 */
void kelon_ir_format_state(const KelonRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void kelon_ir_format_toggle(const KelonRequest* req, KelonToggle toggle, char* out, size_t len);

/** Same, for an Extra-screen entry. */
void kelon_ir_format_extra(const KelonRequest* req, KelonExtra extra, char* out, size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool kelon_ir_toggle_is_momentary(KelonToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool kelon_ir_mode_locks_fan(KelonMode mode);

/** Whether this mode carries no temperature setpoint. */
bool kelon_ir_mode_has_no_temp(KelonMode mode);

/** Display names. */
const char* kelon_ir_get_mode_name(KelonMode mode);
const char* kelon_ir_get_fan_name(KelonFan fan);
const char* kelon_ir_get_toggle_name(KelonToggle toggle);
const char* kelon_ir_get_extra_name(KelonExtra extra);

/**
 * Some protocols cover several incompatible handset variants that differ in
 * frame length, checksum or field encoding, and nothing in the signal says
 * which one a given unit expects. Expose them here and the Setup screen grows
 * a picker for them.
 *
 * @return number of variants, or 0 when the protocol has only one form
 */
uint8_t kelon_ir_get_option_count(void);

/** Short label for the picker, e.g. "Model". Only used when count > 0. */
const char* kelon_ir_get_option_label(void);

/** Display name of one variant. */
const char* kelon_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* kelon_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
