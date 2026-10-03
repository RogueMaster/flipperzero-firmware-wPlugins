#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Mitsubishi Electric 144-bit protocol (IRremoteESP8266 MITSUBISHI_AC).
//
// This is Mitsubishi ELECTRIC - the MSZ/MSH/MLZ split systems - and is a
// different company and a different protocol from Mitsubishi Heavy
// Industries, which has its own app in this repo.
//
// Frame: 18 bytes, LSB first, sent twice with a 15.5 ms gap between copies.
// Bytes 0-4 are the fixed preamble 23 CB 26 01 00. Byte 17 is the sum of
// bytes 0..16.
//
// One quirk worth knowing: the low nibble of byte 8 is mode-dependent and is
// not documented as a named field anywhere. IRremoteESP8266's setMode writes
// the whole byte, so we replicate the same table - a frame without it is
// ignored by at least some units.
//
// This is the ONLY file (with its .c) that carries protocol knowledge. The
// app shell and all three views drive themselves off the enums and the name
// getters below, so a port never has to edit a view.
//
// Rules that keep the generic UI working:
//  - MitsubishiModeOff must stay the FIRST mode; the app sends the power-off
//    toggle when the user selects it.
//  - MitsubishiTogglePowerOff must stay the FIRST toggle.
//  - At most MITSUBISHI_MAX_MAIN_TOGGLES toggles fit on the main screen. Anything
//    beyond that belongs in MitsubishiExtra.
// --------------------------------------------------------------------------

// IR carrier
#define MITSUBISHI_IR_CARRIER_FREQ 38000
#define MITSUBISHI_IR_DUTY_CYCLE   0.33f

// Worst-case timing count for one transmission. Size generously; the app
// allocates this many uint32_t once.
#define MITSUBISHI_IR_MAX_TIMINGS 700

// Temperature range (Celsius)
#define MITSUBISHI_TEMP_MIN 16
#define MITSUBISHI_TEMP_MAX 31

// Main screen fits 6 toggle buttons before it collides with the footer
#define MITSUBISHI_MAX_MAIN_TOGGLES 6

// Temperature stamped into one-shot frames that still carry a setpoint
#define MITSUBISHI_DEFAULT_TOGGLE_TEMP 24

// Buffer for the short payload string shown on the Extra screen
#define MITSUBISHI_CODE_STR_LEN 24

/// Mitsubishi Electric ships three incompatible frame formats. Nothing in a
/// received signal tells the unit which one it wants, so the Setup screen
/// offers a picker and the choice is saved to the SD card.
///
/// The names match what AC Detector reports, so a user can read the bit count
/// off the detector and pick the same entry here.
typedef enum {
    MitsubishiModel144 = 0, ///< MSZ / MSH / MLZ, 18 bytes (MITSUBISHI_AC)
    MitsubishiModel112, ///< KPOA and some Sharp handsets, 14 bytes
    MitsubishiModel136, ///< 001CP T7WE10714, 17 bytes
    MitsubishiModelCount
} MitsubishiModel;

// Operating modes. Off must be first.
typedef enum {
    MitsubishiModeOff = 0,
    MitsubishiModeCool,
    MitsubishiModeAuto,
    MitsubishiModeDry,
    MitsubishiModeHeat,
    MitsubishiModeFan,
    MitsubishiModeCount
} MitsubishiMode;

// Fan speeds, slowest to fastest after Auto
typedef enum {
    MitsubishiFanAuto = 0,
    MitsubishiFanLow,
    MitsubishiFanMedium,
    MitsubishiFanHigh,
    MitsubishiFanCount
} MitsubishiFan;

// One-shot buttons on the main screen. PowerOff must be first and is not
// drawn as a button - it is sent when Mode is set to Off.
typedef enum {
    MitsubishiTogglePowerOff = 0,
    MitsubishiToggleSwingV, // vertical vane sweeps
    MitsubishiToggleSwingH, // horizontal vane sweeps
    MitsubishiToggleISee, // occupancy sensor aims the airflow
    MitsubishiToggleQuiet, // silent fan speed
    MitsubishiToggleEcono, // Ecocool
    MitsubishiToggleNatural, // natural airflow
    MitsubishiToggleCount
} MitsubishiToggle;

// Less common commands, listed on the Extra screen.
// Fixed vane positions on both axes, which the main screen's two swing
// buttons can only set to auto or centred.
typedef enum {
    MitsubishiExtraVaneHighest = 0,
    MitsubishiExtraVaneHigh,
    MitsubishiExtraVaneMiddle,
    MitsubishiExtraVaneLow,
    MitsubishiExtraVaneLowest,
    MitsubishiExtraWideLeftMax,
    MitsubishiExtraWideLeft,
    MitsubishiExtraWideMiddle,
    MitsubishiExtraWideRight,
    MitsubishiExtraWideRightMax,
    MitsubishiExtraWideWide,
    MitsubishiExtraCount
} MitsubishiExtra;

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
    MitsubishiMode mode;
    MitsubishiFan fan;
    uint8_t temp;
    uint32_t toggle_bits; // bit i = MitsubishiToggle i currently believed on
    uint8_t option; // protocol variant, see mitsubishi_ir_get_option_count()
} MitsubishiRequest;

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for MitsubishiModeOff (send the power-off toggle)
 */
bool mitsubishi_ir_encode_state(
    const MitsubishiRequest* req,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Encode a main-screen button press.
 * `req->toggle_bits` already reflects the press.
 */
bool mitsubishi_ir_encode_toggle(
    const MitsubishiRequest* req,
    MitsubishiToggle toggle,
    uint32_t* timings,
    size_t* timings_count);

/** Encode an Extra-screen command. */
bool mitsubishi_ir_encode_extra(
    const MitsubishiRequest* req,
    MitsubishiExtra extra,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Short human-readable payload for the current state, for the Extra screen.
 * Width-agnostic: short-code protocols print a hex word, byte-array
 * protocols print a digest. Must fit MITSUBISHI_CODE_STR_LEN including the NUL.
 */
void mitsubishi_ir_format_state(const MitsubishiRequest* req, char* out, size_t len);

/** Same, for a main-screen button press. */
void mitsubishi_ir_format_toggle(
    const MitsubishiRequest* req,
    MitsubishiToggle toggle,
    char* out,
    size_t len);

/** Same, for an Extra-screen entry. */
void mitsubishi_ir_format_extra(
    const MitsubishiRequest* req,
    MitsubishiExtra extra,
    char* out,
    size_t len);

/**
 * True for buttons that step through positions rather than latching on/off
 * (a vane-step button, say). The UI never shows an indicator dot for these.
 */
bool mitsubishi_ir_toggle_is_momentary(MitsubishiToggle toggle);

/** Whether the unit locks the fan to Auto in this mode. */
bool mitsubishi_ir_mode_locks_fan(MitsubishiMode mode);

/** Whether this mode carries no temperature setpoint. */
bool mitsubishi_ir_mode_has_no_temp(MitsubishiMode mode);

/** Display names. */
const char* mitsubishi_ir_get_mode_name(MitsubishiMode mode);
const char* mitsubishi_ir_get_fan_name(MitsubishiFan fan);
const char* mitsubishi_ir_get_toggle_name(MitsubishiToggle toggle);
const char* mitsubishi_ir_get_extra_name(MitsubishiExtra extra);

/**
 * Some protocols cover several incompatible handset variants that differ in
 * frame length, checksum or field encoding, and nothing in the signal says
 * which one a given unit expects. Expose them here and the Setup screen grows
 * a picker for them.
 *
 * @return number of variants, or 0 when the protocol has only one form
 */
uint8_t mitsubishi_ir_get_option_count(void);

/** Short label for the picker, e.g. "Model". Only used when count > 0. */
const char* mitsubishi_ir_get_option_label(void);

/** Display name of one variant. */
const char* mitsubishi_ir_get_option_name(uint8_t option);

/** Protocol name shown in the main screen footer. */
const char* mitsubishi_ir_get_protocol_name(void);

#ifdef __cplusplus
}
#endif
