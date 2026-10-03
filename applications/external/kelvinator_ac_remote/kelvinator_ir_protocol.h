#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Kelvinator, 16-byte frame (IRremoteESP8266 KELVINATOR).
//
// Worth knowing before you reach for another app: Gree's YAP0F8 and YAPOF3
// handsets speak this protocol, not Gree's own, and so do Sharp's A5VEY and
// YB1FA. If the detector says Kelvinator, this is the app even when the badge
// on the unit says something else.
//
//   byte 0     bits 0-2 mode, bit 3 power, bits 4-5 basic fan, bit 6 swing auto
//   byte 1     bits 0-3 temperature - 16
//   byte 2     bit 4 turbo, bit 5 light, bit 6 ion filter, bit 7 X-Fan
//   byte 3     0x50 constant
//   byte 4     bits 0-3 vertical vane, bit 4 horizontal swing
//   byte 7     bits 4-7 checksum of bytes 0-6
//   byte 8-10  copies of bytes 0-2
//   byte 11    0x70 constant
//   byte 12    bit 7 quiet
//   byte 14    bits 4-6 fan (the full-resolution value)
//   byte 15    bits 4-7 checksum of bytes 8-14
//
// On the wire it is four 32-bit chunks, least significant bit first: two of
// them carry a three-bit 0b010 marker before the gap that follows.
//
// This is the ONLY file (with its .c) that carries protocol knowledge. The
// app shell and all three views drive themselves off the enums and the name
// getters below, so a port never has to edit a view.
//
// Rules that keep the generic UI working:
//  - KelvinatorModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - KelvinatorTogglePowerOff must stay the FIRST toggle.
//  - At most KELVINATOR_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in KelvinatorExtra.
// --------------------------------------------------------------------------

// IR carrier
#define KELVINATOR_IR_CARRIER_FREQ 38000
#define KELVINATOR_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define KELVINATOR_IR_MAX_TIMINGS 700

// Temperature range (Celsius)
#define KELVINATOR_TEMP_MIN 16
#define KELVINATOR_TEMP_MAX 30

// Main screen fits 6 toggle buttons before it collides with the footer
#define KELVINATOR_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define KELVINATOR_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define KELVINATOR_CODE_STR_LEN 24

// Operating modes. Off must be first.
typedef enum {
    KelvinatorModeOff = 0,
    KelvinatorModeCool,
    KelvinatorModeAuto,
    KelvinatorModeDry,
    KelvinatorModeHeat,
    KelvinatorModeFan,
    KelvinatorModeCount
} KelvinatorMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    KelvinatorFanAuto = 0,
    KelvinatorFanLow,
    KelvinatorFanMedium,
    KelvinatorFanHigh,
    KelvinatorFanCount
} KelvinatorFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    KelvinatorTogglePowerOff = 0,
    KelvinatorToggleSwingV, // vertical vane sweeps
    KelvinatorToggleSwingH, // horizontal vane sweeps
    KelvinatorToggleTurbo,
    KelvinatorToggleXfan, // dries the coil after shutdown
    KelvinatorToggleLight, // display backlight
    KelvinatorToggleQuiet,
    KelvinatorToggleCount
} KelvinatorToggle;

// Less common commands, listed on the Extra screen.
// The five fixed vane positions the main screen's Swing button cannot
// express, plus the ion filter.
typedef enum {
    KelvinatorExtraVaneHighest = 0,
    KelvinatorExtraVaneUpperMid,
    KelvinatorExtraVaneMiddle,
    KelvinatorExtraVaneLowerMid,
    KelvinatorExtraVaneLowest,
    KelvinatorExtraVaneOff,
    KelvinatorExtraIonOn,
    KelvinatorExtraIonOff,
    KelvinatorExtraCount
} KelvinatorExtra;

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
    KelvinatorMode mode;
    KelvinatorFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = KelvinatorToggle i currently believed on
    uint8_t option; // protocol variant, see kelvinator_ir_get_option_count()
} KelvinatorRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for KelvinatorModeOff (send the power-off toggle)
 */
bool kelvinator_ir_encode_state(
    const KelvinatorRequest* req,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool kelvinator_ir_encode_toggle(
    const KelvinatorRequest* req,
    KelvinatorToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool kelvinator_ir_encode_extra(
    const KelvinatorRequest* req,
    KelvinatorExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit KELVINATOR_CODE_STR_LEN including the NUL.
 */
void kelvinator_ir_format_state(const KelvinatorRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void kelvinator_ir_format_toggle(
    const KelvinatorRequest* req,
    KelvinatorToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void kelvinator_ir_format_extra(
    const KelvinatorRequest* req,
    KelvinatorExtra extra,
    char* out,
    size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool kelvinator_ir_toggle_is_momentary(KelvinatorToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool kelvinator_ir_mode_locks_fan(KelvinatorMode mode);

/** Whether this mode carries no temperature setpoint. */
bool kelvinator_ir_mode_has_no_temp(KelvinatorMode mode);

/** Display names. */
const char* kelvinator_ir_get_mode_name(KelvinatorMode mode);
const char* kelvinator_ir_get_fan_name(KelvinatorFan fan);
const char* kelvinator_ir_get_toggle_name(KelvinatorToggle toggle);
const char* kelvinator_ir_get_extra_name(KelvinatorExtra extra);

/**
 * Some protocols cover several incompatible handset variants that differ in
 * frame length, checksum or field encoding, and nothing in the signal says
 * which one a given unit expects. Expose them here and the Setup screen grows
 * a picker for them.
 *
 * @return number of variants, or 0 when the protocol has only one form
 */
uint8_t kelvinator_ir_get_option_count(void);

/** Short label for the picker, e.g. "Model". Only used when count > 0. */
const char* kelvinator_ir_get_option_label(void);

/** Display name of one variant. */
const char* kelvinator_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* kelvinator_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
