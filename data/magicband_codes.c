#include "magicband_codes.h"

#include <string.h>

// --- Pre-built presets (animations, etc.) ---
// Each entry is a name + hex payload starting from the function code
// (E905/E906/E908/E909). Add your presets here — they will appear in the
// Presets menu.
const MagicBandPreset magicband_presets[] = {
    {"White Lightning", "e90b0b0f0f5c5d48a5d1453205"},
    {"Taste the Rainbow", "E90C000F0F5D465BF005323748B0"},
    {"5 Color Cycle", "e90c000f0fb1b9b5b1a2307b7db0"},
    {"Circle White Vibe", "e91200010fbcbdbdbdbd30d037f4d2460000fcbb"},
    {"Blue Party", "e91300b60f404458f44882d06519d146060a307bff"},
    {"Purple Waves", "e9130002d037f0d23d0505000efa8983510ee7a0b0"},
    {"Opposite Flashes", "e91300650fbdb5bcb5bc7aec5c0a2915291548abb0"},
    {"Pink Heartbeat", "e914000cd037f0d23d050c0c0eec8983510eee0c3db0"},
    {"Purple Heartbeat", "e914002cd037f0d23d0212000eea8983510ee30c1eb0"},
    {"Love Sparkle", "e91400420f555b58f44882d0651bd1462a02307b5db0"},
    {"Multi-color Sparks", "e90e00010fbda0a0bda059070048aeb5"},
    {"Multi-color Flashes 1", "e90e00020fbca0bca0bc5917fb48aebb"},
    {"Multi-color Flashes 2", "e90e00110fbca7b9a7b959190248aeb0"},
    {"Multi-color Flashes 3", "e90e00150fbbbbbbbbbb59190248aeb0"},
    {"Circle Flashes 1", "e90e00830fb5b9b2adb659190b48aeb0"},
    {"Circle Flashes 2", "e90f00110f4f425807488dd2462a0717b8"},
    {"Blue / Purple / Yellow", "e911004f0f444f58f44882d1460607d06543b0"},
    {"Fade Green / Purple", "e911000f0f485958f44882d146020dd06505b0"},
    {"Fade Orange / Red", "e911004f0f4f5558f44882d146022ad06501b0"},
    {"Fade Red / Red", "e91100070f555d58f44882d1460508d06500b0"},
    {"Fade Blue / Yellow", "e91100440f514258f44882d146050fd06500b0"},
    {"Flash Purple / Green", "e91100010f5a475bf03134374894d13d0507b0"},
};
const uint32_t magicband_preset_count = sizeof(magicband_presets) / sizeof(magicband_presets[0]);

// --- Hex parsing helpers ---

static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

static uint8_t hex_byte(const char* s) {
    return (uint8_t)((hex_nibble(s[0]) << 4) | hex_nibble(s[1]));
}

uint8_t magicband_preset_to_beacon_data(const char* hex, uint8_t* output) {
    // Count hex payload length
    uint8_t hex_len = 0;
    for (const char* p = hex; *p && *(p + 1); p += 2) {
        hex_len++;
    }

    // AD header: length, type, Disney specifier, E1 00
    uint8_t pos = 0;
    output[pos++] = (uint8_t)(hex_len + 5);  // AD length: 0xFF + 8301 + E100 + payload
    output[pos++] = 0xFF;                    // AD type: manufacturer specific
    output[pos++] = 0x83;
    output[pos++] = 0x01;  // Disney specifier
    output[pos++] = 0xE1;
    output[pos++] = 0x00;  // Purpose unknown

    // Copy raw hex bytes
    for (const char* p = hex; *p && *(p + 1); p += 2) {
        output[pos++] = hex_byte(p);
    }

    return pos;
}

// --- Color palette (unique entries, maps enum -> 5-bit wire value) ---
const char* const magicband_color_names[] = {
    [MagicBandColorCyan] = "Cyan",
    [MagicBandColorPurple] = "Purple",
    [MagicBandColorBlue] = "Blue",
    [MagicBandColorMidnightBlue] = "Navy",
    [MagicBandColorBrightPurple] = "Violet",
    [MagicBandColorLavender] = "Lilac",
    [MagicBandColorPink] = "Pink",
    [MagicBandColorYellowOrange] = "Amber",
    [MagicBandColorOffYellow] = "Gold",
    [MagicBandColorLime] = "Lime",
    [MagicBandColorOrange] = "Orange",
    [MagicBandColorRedOrange] = "Coral",
    [MagicBandColorRed] = "Red",
    [MagicBandColorGreen] = "Green",
    [MagicBandColorLimeGreen] = "Jade",
    [MagicBandColorWhite] = "White",
    [MagicBandColorOff] = "Off",
    [MagicBandColorUnique] = "Unique",
    [MagicBandColorRandom] = "Random",
};

const uint8_t magicband_color_values[] = {
    [MagicBandColorCyan] = 0x00,         [MagicBandColorPurple] = 0x01,
    [MagicBandColorBlue] = 0x04,         [MagicBandColorMidnightBlue] = 0x03,
    [MagicBandColorBrightPurple] = 0x05, [MagicBandColorLavender] = 0x06,
    [MagicBandColorPink] = 0x08,         [MagicBandColorYellowOrange] = 0x0F,
    [MagicBandColorOffYellow] = 0x10,    [MagicBandColorLime] = 0x12,
    [MagicBandColorOrange] = 0x13,       [MagicBandColorRedOrange] = 0x14,
    [MagicBandColorRed] = 0x15,          [MagicBandColorGreen] = 0x19,
    [MagicBandColorLimeGreen] = 0x1A,    [MagicBandColorWhite] = 0x1B,
    [MagicBandColorOff] = 0x1D,          [MagicBandColorUnique] = 0x1E,
    [MagicBandColorRandom] = 0x1F,
};

// --- Light mask palette (unique entries, maps enum -> 3-bit wire value) ---
const char* const magicband_mask_names[] = {
    [MagicBandMaskAllLEDs] = "All",        // All LEDs
    [MagicBandMaskTopRight] = "Top R",     // Top Right
    [MagicBandMaskBottomRight] = "Bot R",  // Bottom Right
    [MagicBandMaskBottomLeft] = "Bot L",   // Bottom Left
    [MagicBandMaskTopLeft] = "Top L",      // Top Left
};

const uint8_t magicband_mask_values[] = {
    [MagicBandMaskAllLEDs] = 0x00,     [MagicBandMaskTopRight] = 0x01,
    [MagicBandMaskBottomRight] = 0x02, [MagicBandMaskBottomLeft] = 0x03,
    [MagicBandMaskTopLeft] = 0x04,
};

// --- Vibration palette (unique entries, maps enum -> 4-bit wire value) ---
const char* const magicband_vibration_names[] = {
    [MagicBandVibNone] = "None",        // no vibration
    [MagicBandVibShort1] = "1x Tap",    // - (6s break) -
    [MagicBandVibShort2] = "2x Tap",    // -- (6s break) --
    [MagicBandVibShort3] = "3x Tap",    // --- (6s break) ---
    [MagicBandVibMedMix1] = "Mix 1",    // --* (4s break) --*
    [MagicBandVibLongMix] = "Mix 2",    // ----*- (3s break) ----*-
    [MagicBandVibLongHeavy] = "Mix 3",  // ---***--- (3s break) ---***---
    [MagicBandVibHeavy] = "Heavy",      // # (4s break) #
    [MagicBandVibPulse] = "Pulse",      // '''''' (6s break) ''''''
    [MagicBandVibMedium] = "Medium",    // * (6s break) *
    [MagicBandVibLong] = "Long",        // % (5s break) %
};

const uint8_t magicband_vibration_values[] = {
    [MagicBandVibNone] = 0x00,      [MagicBandVibShort1] = 0x01,  [MagicBandVibShort2] = 0x02,
    [MagicBandVibShort3] = 0x03,    [MagicBandVibMedMix1] = 0x04, [MagicBandVibLongMix] = 0x05,
    [MagicBandVibLongHeavy] = 0x06, [MagicBandVibHeavy] = 0x07,   [MagicBandVibPulse] = 0x08,
    [MagicBandVibMedium] = 0x0A,    [MagicBandVibLong] = 0x0B,
};

// --- Code type info ---
const MagicBandCodeInfo magicband_code_info[] = {
    [MagicBandCodeTypeE905] =
        {
            .name = "Single Color (0xE905)",
            .default_hex = "e90500090eedb0",
            .type = MagicBandCodeTypeE905,
        },
    [MagicBandCodeTypeE906] =
        {
            .name = "Dual Color (0xE906)",
            .default_hex = "e90600220f4441b0",
            .type = MagicBandCodeTypeE906,
        },
    [MagicBandCodeTypeE908] =
        {
            .name = "RGB Color (0xE908)",
            .default_hex = "E908000ED2557C7C00B0",
            .type = MagicBandCodeTypeE908,
        },
    [MagicBandCodeTypeE909] =
        {
            .name = "5-Zone Color (0xE909)",
            .default_hex = "E909000E0FBCB5B9A4A7B0",
            .type = MagicBandCodeTypeE909,
        },
};

static void parse_timing_byte(uint8_t timing, MagicBandCodeParams* params) {
    params->always_on = (timing >> 7) & 1;
    params->timing_scaler = (timing >> 6) & 1;
    params->fade_out = (timing >> 4) & 0x03;
    params->time_value = timing & 0x0F;
}

static MagicBandVibration vibration_from_wire(uint8_t wire) {
    uint8_t val = wire & 0x0F;
    for (int i = 0; i < MagicBandVibCount; i++) {
        if (magicband_vibration_values[i] == val) return (MagicBandVibration)i;
    }
    return MagicBandVibNone;
}

static MagicBandColor color_from_wire(uint8_t wire) {
    for (int i = 0; i < MagicBandColorCount; i++) {
        if (magicband_color_values[i] == wire) return (MagicBandColor)i;
    }
    return MagicBandColorCyan;
}

static MagicBandMask mask_from_wire(uint8_t wire) {
    for (int i = 0; i < MagicBandMaskCount; i++) {
        if (magicband_mask_values[i] == wire) return (MagicBandMask)i;
    }
    return MagicBandMaskAllLEDs;
}

MagicBandCodeType magicband_code_params_from_hex(const char* hex, MagicBandCodeParams* params) {
    // Parse function code from first 4 hex chars
    uint8_t func_hi = hex_byte(hex);
    uint8_t func_lo = hex_byte(hex + 2);

    if (func_hi != 0xE9) return MagicBandCodeTypeCount;

    // Skip function code (4 chars) + spacer 00 (2 chars) = offset 6
    const char* p = hex + 6;

    // Timing byte
    uint8_t timing = hex_byte(p);
    parse_timing_byte(timing, params);
    p += 2;

    if (func_lo == 0x05) {
        // E905: skip unknown (0E), then mask_color, then vib
        p += 2;  // skip 0x0E
        uint8_t mask_color = hex_byte(p);
        p += 2;
        uint8_t vib = hex_byte(p);

        params->mask = mask_from_wire((mask_color >> 5) & 0x07);
        params->color = color_from_wire(mask_color & 0x1F);
        params->vibration = vibration_from_wire(vib);
        return MagicBandCodeTypeE905;

    } else if (func_lo == 0x06) {
        // E906: skip unknown (0F), then inner, outer, vib
        p += 2;  // skip 0x0F
        uint8_t inner = hex_byte(p);
        p += 2;
        uint8_t outer = hex_byte(p);
        p += 2;
        uint8_t vib = hex_byte(p);

        params->color_inner = color_from_wire(inner & 0x1F);
        params->color_outer = color_from_wire(outer & 0x1F);
        params->vibration = vibration_from_wire(vib);
        return MagicBandCodeTypeE906;

    } else if (func_lo == 0x08) {
        // E908: skip D2 (2), skip 55 (2), then red, green, blue, vib
        p += 2;  // skip 0xD2
        p += 2;  // skip 0x55
        uint8_t red = hex_byte(p);
        p += 2;
        uint8_t green = hex_byte(p);
        p += 2;
        uint8_t blue = hex_byte(p);
        p += 2;
        uint8_t vib = hex_byte(p);

        params->rgb_red = (red >> 1) & 0x3F;
        params->rgb_red_flash = (red >> 7) & 1;
        params->rgb_green = (green >> 1) & 0x3F;
        params->rgb_green_flash = (green >> 7) & 1;
        params->rgb_blue = (blue >> 1) & 0x3F;
        params->rgb_blue_flash = (blue >> 7) & 1;
        params->vibration = vibration_from_wire(vib);
        return MagicBandCodeTypeE908;

    } else if (func_lo == 0x09) {
        // E909: skip 0F (2), then center, top_right, bottom_right, bottom_left,
        // top_left, vib
        p += 2;  // skip 0x0F
        params->color_center = color_from_wire(hex_byte(p) & 0x1F);
        p += 2;
        params->color_top_right = color_from_wire(hex_byte(p) & 0x1F);
        p += 2;
        params->color_bottom_right = color_from_wire(hex_byte(p) & 0x1F);
        p += 2;
        params->color_bottom_left = color_from_wire(hex_byte(p) & 0x1F);
        p += 2;
        params->color_top_left = color_from_wire(hex_byte(p) & 0x1F);
        p += 2;
        uint8_t vib = hex_byte(p);
        params->vibration = vibration_from_wire(vib);
        return MagicBandCodeTypeE909;
    }

    return MagicBandCodeTypeCount;
}

void magicband_code_params_init(MagicBandCodeParams* params) {
    // Set baseline defaults, then override per code type from default_hex
    memset(params, 0, sizeof(*params));
    for (int i = 0; i < MagicBandCodeTypeCount; i++) {
        magicband_code_params_from_hex(magicband_code_info[i].default_hex, params);
    }
}

static uint8_t magicband_code_generate_e905(const MagicBandCodeParams* params, uint8_t* output) {
    // Build timing byte: [always_on:1][scaler:1][fade_out:2][time_value:4]
    uint8_t timing =
        (uint8_t)(((params->always_on ? 1 : 0) << 7) | ((params->timing_scaler & 0x01) << 6) |
                  ((params->fade_out & 0x03) << 4) | (params->time_value & 0x0F));

    // Build mask+color byte: [mask:3][color:5]
    uint8_t mask_color = (uint8_t)(((magicband_mask_values[params->mask] & 0x07) << 5) |
                                   (magicband_color_values[params->color] & 0x1F));

    // Build enable+vibration byte: 0xB enable nibble + vibration nibble
    uint8_t vib_byte = (uint8_t)(0xB0 | (magicband_vibration_values[params->vibration] & 0x0F));

    const uint8_t data[] = {
        0x0C,  // AD length (12 bytes follow)
        0xFF,  // AD type: manufacturer specific
        0x83,  // Disney specifier
        0x01,
        0xE1,  // Purpose unknown
        0x00,
        0xE9,  // Single color function
        0x05,
        0x00,  // Spacer
        timing,
        0x0E,  // Purpose unknown
        mask_color, vib_byte,
    };
    const uint8_t len = sizeof(data);
    memcpy(output, data, len);
    return len;
}

static uint8_t magicband_code_generate_e906(const MagicBandCodeParams* params, uint8_t* output) {
    // Build timing byte: [always_on:1][scaler:1][fade_out:2][time_value:4]
    uint8_t timing =
        (uint8_t)(((params->always_on ? 1 : 0) << 7) | ((params->timing_scaler & 0x01) << 6) |
                  ((params->fade_out & 0x03) << 4) | (params->time_value & 0x0F));

    // Build inner color byte: [010:3][color:5]
    uint8_t inner = (uint8_t)(0x40 | (magicband_color_values[params->color_inner] & 0x1F));

    // Build outer color byte: [010:3][color:5]
    uint8_t outer = (uint8_t)(0x40 | (magicband_color_values[params->color_outer] & 0x1F));

    // Build enable+vibration byte: 0xB enable nibble + vibration nibble
    uint8_t vib_byte = (uint8_t)(0xB0 | (magicband_vibration_values[params->vibration] & 0x0F));

    const uint8_t data[] = {
        0x0E,  // AD length (14 bytes follow)
        0xFF,  // AD type: manufacturer specific
        0x83,  // Disney specifier
        0x01,
        0xE2,  // Purpose unknown
        0x00,
        0xE9,  // Dual color function
        0x06,
        0x00,  // Spacer
        timing,
        0x0F,  // Purpose unknown
        inner,  outer, vib_byte,
    };
    const uint8_t len = sizeof(data);
    memcpy(output, data, len);
    return len;
}

static uint8_t magicband_code_generate_e908(const MagicBandCodeParams* params, uint8_t* output) {
    // Build timing byte: [always_on:1][scaler:1][fade_out:2][time_value:4]
    uint8_t timing =
        (uint8_t)(((params->always_on ? 1 : 0) << 7) | ((params->timing_scaler & 0x01) << 6) |
                  ((params->fade_out & 0x03) << 4) | (params->time_value & 0x0F));

    // Build color bytes: [flash:1][value:6][0:1]
    uint8_t red =
        (uint8_t)(((params->rgb_red_flash ? 1 : 0) << 7) | ((params->rgb_red & 0x3F) << 1));
    uint8_t green =
        (uint8_t)(((params->rgb_green_flash ? 1 : 0) << 7) | ((params->rgb_green & 0x3F) << 1));
    uint8_t blue =
        (uint8_t)(((params->rgb_blue_flash ? 1 : 0) << 7) | ((params->rgb_blue & 0x3F) << 1));

    // Build enable+vibration byte: 0xB enable nibble + vibration nibble
    uint8_t vib_byte = (uint8_t)(0xB0 | (magicband_vibration_values[params->vibration] & 0x0F));

    const uint8_t data[] = {
        0x10,  // AD length (16 bytes follow)
        0xFF,  // AD type: manufacturer specific
        0x83,  // Disney specifier
        0x01,
        0xE2,  // Purpose unknown
        0x00,
        0xE9,  // RGB color function
        0x08,
        0x00,  // Spacer
        timing,
        0xD2,  // Purpose unknown
        0x55,  // Purpose unknown
        red,    green, blue, vib_byte,
    };
    const uint8_t len = sizeof(data);
    memcpy(output, data, len);
    return len;
}

static uint8_t magicband_code_generate_e909(const MagicBandCodeParams* params, uint8_t* output) {
    // Build timing byte: [always_on:1][scaler:1][fade_out:2][time_value:4]
    uint8_t timing =
        (uint8_t)(((params->always_on ? 1 : 0) << 7) | ((params->timing_scaler & 0x01) << 6) |
                  ((params->fade_out & 0x03) << 4) | (params->time_value & 0x0F));

    // Build zone color bytes: [101:3][color:5]
    uint8_t center = (uint8_t)(0xA0 | (magicband_color_values[params->color_center] & 0x1F));
    uint8_t top_right = (uint8_t)(0xA0 | (magicband_color_values[params->color_top_right] & 0x1F));
    uint8_t bottom_right =
        (uint8_t)(0xA0 | (magicband_color_values[params->color_bottom_right] & 0x1F));
    uint8_t bottom_left =
        (uint8_t)(0xA0 | (magicband_color_values[params->color_bottom_left] & 0x1F));
    uint8_t top_left = (uint8_t)(0xA0 | (magicband_color_values[params->color_top_left] & 0x1F));

    // Build enable+vibration byte: 0xB enable nibble + vibration nibble
    uint8_t vib_byte = (uint8_t)(0xB0 | (magicband_vibration_values[params->vibration] & 0x0F));

    const uint8_t data[] = {
        0x10,  // AD length (16 bytes follow)
        0xFF,  // AD type: manufacturer specific
        0x83,  // Disney specifier
        0x01,
        0xE1,  // Purpose unknown
        0x00,
        0xE9,  // 5-zone color function
        0x09,
        0x00,  // Spacer
        timing,
        0x0F,  // Purpose unknown
        center, top_right, bottom_right, bottom_left, top_left, vib_byte,
    };
    const uint8_t len = sizeof(data);
    memcpy(output, data, len);
    return len;
}

uint8_t magicband_code_generate(MagicBandCodeType type, const MagicBandCodeParams* params,
                                uint8_t* output) {
    switch (type) {
        case MagicBandCodeTypeE905:
            return magicband_code_generate_e905(params, output);
        case MagicBandCodeTypeE906:
            return magicband_code_generate_e906(params, output);
        case MagicBandCodeTypeE908:
            return magicband_code_generate_e908(params, output);
        case MagicBandCodeTypeE909:
            return magicband_code_generate_e909(params, output);
        default:
            return 0;
    }
}
