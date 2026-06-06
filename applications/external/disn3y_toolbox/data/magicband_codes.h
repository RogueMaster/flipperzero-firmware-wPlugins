#pragma once

#include <stdbool.h>
#include <stdint.h>

// Color palette (unique entries only, ordered ROYGBIV)
typedef enum {
    MagicBandColorRed,
    MagicBandColorRedOrange,
    MagicBandColorOrange,
    MagicBandColorYellowOrange,
    MagicBandColorOffYellow,
    MagicBandColorLime,
    MagicBandColorLimeGreen,
    MagicBandColorGreen,
    MagicBandColorCyan,
    MagicBandColorBlue,
    MagicBandColorMidnightBlue,
    MagicBandColorPurple,
    MagicBandColorBrightPurple,
    MagicBandColorLavender,
    MagicBandColorPink,
    MagicBandColorWhite,
    MagicBandColorUnique,
    MagicBandColorRandom,
    MagicBandColorOff,
    MagicBandColorCount,
} MagicBandColor;

// Light mask palette (unique entries only)
typedef enum {
    MagicBandMaskAllLEDs,
    MagicBandMaskTopRight,
    MagicBandMaskBottomRight,
    MagicBandMaskBottomLeft,
    MagicBandMaskTopLeft,
    MagicBandMaskCount,
} MagicBandMask;

// Vibration palette (unique entries only)
typedef enum {
    MagicBandVibNone,
    MagicBandVibShort1,
    MagicBandVibShort2,
    MagicBandVibShort3,
    MagicBandVibMedMix1,
    MagicBandVibLongMix,
    MagicBandVibLongHeavy,
    MagicBandVibHeavy,
    MagicBandVibPulse,
    MagicBandVibMedium,
    MagicBandVibLong,
    MagicBandVibCount,
} MagicBandVibration;

extern const char* const magicband_color_names[];
extern const uint8_t magicband_color_values[];

extern const char* const magicband_mask_names[];
extern const uint8_t magicband_mask_values[];

extern const char* const magicband_vibration_names[];
extern const uint8_t magicband_vibration_values[];

// Configurable parameters for code generation
typedef struct {
    MagicBandColor color; // E905: single color
    MagicBandColor color_inner; // E906: inner color
    MagicBandColor color_outer; // E906: outer color
    MagicBandColor color_center; // E909: center LED color
    MagicBandColor color_top_right; // E909: top right LED color
    MagicBandColor color_bottom_right; // E909: bottom right LED color
    MagicBandColor color_bottom_left; // E909: bottom left LED color
    MagicBandColor color_top_left; // E909: top left LED color
    uint8_t rgb_red; // E908: 6-bit red value (0-63)
    uint8_t rgb_green; // E908: 6-bit green value (0-63)
    uint8_t rgb_blue; // E908: 6-bit blue value (0-63)
    bool rgb_red_flash; // E908: red channel flashing
    bool rgb_green_flash; // E908: green channel flashing
    bool rgb_blue_flash; // E908: blue channel flashing
    MagicBandMask mask;
    MagicBandVibration vibration;
    bool always_on;
    uint8_t timing_scaler; // 0 or 1 (bit 6)
    uint8_t fade_out; // 0-3 (2 bits)
    uint8_t time_value; // 0-15 (4 bits)
} MagicBandCodeParams;

typedef enum {
    MagicBandCodeTypeE905,
    MagicBandCodeTypeE906,
    MagicBandCodeTypeE908,
    MagicBandCodeTypeE909,
    // Future code types go here
    MagicBandCodeTypeCount,
} MagicBandCodeType;

typedef struct {
    const char* name;
    const char* default_hex; // Default payload hex (without 8301 and E1/E200 prefix)
    MagicBandCodeType type;
} MagicBandCodeInfo;

extern const MagicBandCodeInfo magicband_code_info[];

// Initialize params to sensible defaults
void magicband_code_params_init(MagicBandCodeParams* params);

// Parse a hex string payload (e.g. "e90500090eedb0") and set params
// accordingly. The hex string should start with the function code
// (E905/E906/E908) and not include the 8301 Disn3y specifier or E100/E200
// prefix. Returns the detected code type, or MagicBandCodeTypeCount on error.
MagicBandCodeType magicband_code_params_from_hex(const char* hex, MagicBandCodeParams* params);

// Generate BLE advertisement payload for the given code type and params.
// Writes payload bytes into `output` and returns the number of bytes written.
// `output` must have room for at least 31 bytes (EXTRA_BEACON_MAX_DATA_SIZE).
uint8_t magicband_code_generate(
    MagicBandCodeType type,
    const MagicBandCodeParams* params,
    uint8_t* output);

// Pre-built preset entry (name + full hex payload starting from function code)
typedef struct {
    const char* name;
    const char* hex; // Payload hex starting from function code (e.g. "E905...")
} MagicBandPreset;

extern const MagicBandPreset magicband_presets[];
extern const uint32_t magicband_preset_count;

// Convert a preset hex string to a full beacon advertisement payload.
// Wraps the hex bytes with the AD header (length, 0xFF, 0x8301, E100, ...).
// Returns the number of bytes written to output.
uint8_t magicband_preset_to_beacon_data(const char* hex, uint8_t* output);
