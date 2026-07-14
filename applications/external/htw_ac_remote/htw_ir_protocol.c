#include "htw_ir_protocol.h"
#include <string.h>

_Static_assert(HTW_IR_MAX_TIMINGS >= 199, "HTW_IR_MAX_TIMINGS too small for protocol");

// Temperature codes (non-linear mapping 17-30°C)
// Index 0 = 17°C, Index 13 = 30°C
static const uint8_t TEMP_CODES[14] = {
    0x0, // 17°C
    0x8, // 18°C
    0xC, // 19°C
    0x4, // 20°C
    0x6, // 21°C
    0xE, // 22°C
    0xA, // 23°C
    0x2, // 24°C
    0x3, // 25°C
    0xB, // 26°C
    0x9, // 27°C
    0x1, // 28°C
    0x5, // 29°C
    0xD // 30°C
};

// Fan speed codes
static const uint8_t FAN_CODES[] = {
    0xFD, // Auto (for Cool/Heat/Fan modes)
    0xF9, // Low
    0xFA, // Medium
    0xFC // High
};

// Fan code for Auto/Dry modes (forced, cannot change)
#define FAN_CODE_FORCED 0xF8

// Mode nibbles (upper nibble of MODE_TEMP byte)
static const uint8_t MODE_NIBBLES[] = {
    0x00, // Off (not used directly)
    0x00, // Cool
    0x10, // Auto
    0x20, // Dry
    0x30, // Heat
    0x00 // Fan-only (special: MODE_TEMP = 0x27)
};

// Fan-only mode has fixed MODE_TEMP value
#define MODE_TEMP_FAN_ONLY 0x27

// Family bytes
#define FAMILY_A_B0 0x4D
#define FAMILY_A_B1 0xB2
#define FAMILY_B_B0 0xAD
#define FAMILY_B_B1 0x52
#define FAMILY_B_B2 0xAF
#define FAMILY_B_B3 0x50

// Toggle codes for Family B (0xAD family)
static const uint8_t TOGGLE_CODES[] = {
    0x00, // Power Off (not in this family)
    0x00, // Swing (not in this family)
    0xA5, // LED
    0x45, // Turbo
    0xC5, // Fresh
    0x55 // Clean
};

// Fixed frames for Power Off and Swing (Family A)
static const uint8_t POWER_OFF_FRAME[6] = {0x4D, 0xB2, 0xDE, 0x21, 0x07, 0xF8};
static const uint8_t SWING_FRAME[6] = {0x4D, 0xB2, 0xD6, 0x29, 0x07, 0xF8};

// Note: Timer base ARG values are embedded in TIMER_STEPS table below

// Full timer table: 34 steps
// Format: {base_arg, timer_on_flag, timer_off_page_mask, hours * 10}
static const struct {
    uint8_t base_arg;
    uint8_t on_flag;
    uint8_t off_page_mask;
    uint16_t hours_x10; // hours * 10 for precision (5 = 0.5h, 10 = 1.0h, etc.)
} TIMER_STEPS[HTW_TIMER_STEPS_COUNT] = {
    // Page 0: 0.5 - 8.0h
    {0x80, 0x00, 0x00, 5}, // Step 1:  0.5h
    {0xC0, 0x00, 0x00, 10}, // Step 2:  1.0h
    {0xA0, 0x00, 0x00, 15}, // Step 3:  1.5h
    {0xE0, 0x00, 0x00, 20}, // Step 4:  2.0h
    {0x90, 0x00, 0x00, 25}, // Step 5:  2.5h
    {0xD0, 0x00, 0x00, 30}, // Step 6:  3.0h
    {0xB0, 0x00, 0x00, 35}, // Step 7:  3.5h
    {0xF0, 0x00, 0x00, 40}, // Step 8:  4.0h
    {0x88, 0x00, 0x00, 45}, // Step 9:  4.5h
    {0xC8, 0x00, 0x00, 50}, // Step 10: 5.0h
    {0xA8, 0x00, 0x00, 55}, // Step 11: 5.5h
    {0xE8, 0x00, 0x00, 60}, // Step 12: 6.0h
    {0x98, 0x00, 0x00, 65}, // Step 13: 6.5h
    {0xD8, 0x00, 0x00, 70}, // Step 14: 7.0h
    {0xB8, 0x00, 0x00, 75}, // Step 15: 7.5h
    {0xF8, 0x00, 0x00, 80}, // Step 16: 8.0h
    // Page 1: 8.5 - 16h
    {0x80, 0x04, 0x80, 85}, // Step 17: 8.5h
    {0xC0, 0x04, 0x80, 90}, // Step 18: 9.0h
    {0xA0, 0x04, 0x80, 95}, // Step 19: 9.5h
    {0xE0, 0x04, 0x80, 100}, // Step 20: 10.0h
    {0xD0, 0x04, 0x80, 110}, // Step 21: 11h
    {0xF0, 0x04, 0x80, 120}, // Step 22: 12h
    {0xC8, 0x04, 0x80, 130}, // Step 23: 13h
    {0xE8, 0x04, 0x80, 140}, // Step 24: 14h
    {0xD8, 0x04, 0x80, 150}, // Step 25: 15h
    {0xF8, 0x04, 0x80, 160}, // Step 26: 16h
    // Page 2: 17 - 24h
    {0xC0, 0x02, 0x40, 170}, // Step 27: 17h
    {0xE0, 0x02, 0x40, 180}, // Step 28: 18h
    {0xD0, 0x02, 0x40, 190}, // Step 29: 19h
    {0xF0, 0x02, 0x40, 200}, // Step 30: 20h
    {0xC8, 0x02, 0x40, 210}, // Step 31: 21h
    {0xE8, 0x02, 0x40, 220}, // Step 32: 22h
    {0xD8, 0x02, 0x40, 230}, // Step 33: 23h
    {0xF8, 0x02, 0x40, 240}, // Step 34: 24h
};

// Mode names
static const char* MODE_NAMES[] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};

// Fan names
static const char* FAN_NAMES[] = {"Auto", "Low", "Med", "High"};

// Helper: Invert byte (XOR with 0xFF)
static inline uint8_t invert_byte(uint8_t b) {
    return b ^ 0xFF;
}

// Helper: Add a single bit timing (LSB first within byte)
static void add_bit(uint32_t* timings, size_t* idx, bool bit) {
    timings[(*idx)++] = HTW_IR_BIT_MARK;
    timings[(*idx)++] = bit ? HTW_IR_BIT1_SPACE : HTW_IR_BIT0_SPACE;
}

// Helper: Add a byte (LSB first)
static void add_byte(uint32_t* timings, size_t* idx, uint8_t byte) {
    for(int i = 0; i < 8; i++) {
        add_bit(timings, idx, (byte >> i) & 1);
    }
}

// Helper: Add leader (mark + space)
static void add_leader(uint32_t* timings, size_t* idx) {
    timings[(*idx)++] = HTW_IR_LEAD_MARK;
    timings[(*idx)++] = HTW_IR_LEAD_SPACE;
}

// Helper: Add stop mark
static void add_stop(uint32_t* timings, size_t* idx) {
    timings[(*idx)++] = HTW_IR_STOP_MARK;
}

// Helper: Add gap
static void add_gap(uint32_t* timings, size_t* idx) {
    timings[(*idx)++] = HTW_IR_GAP;
}

// Helper: Encode a single frame (6 bytes)
static void encode_frame(const uint8_t* data, uint32_t* timings, size_t* idx) {
    add_leader(timings, idx);
    for(int i = 0; i < 6; i++) {
        add_byte(timings, idx, data[i]);
    }
    add_stop(timings, idx);
}

// Helper: Encode double frame (for reliability)
static void encode_double_frame(const uint8_t* data, uint32_t* timings, size_t* count) {
    size_t idx = 0;

    // Frame 1
    encode_frame(data, timings, &idx);
    add_gap(timings, &idx);

    // Frame 2 (same data)
    encode_frame(data, timings, &idx);
    // No gap after second frame

    *count = idx;
}

// Helper: Get MODE_TEMP byte for state command
static uint8_t get_mode_temp(HtwMode mode, uint8_t temp) {
    if(mode == HtwModeFan) {
        return MODE_TEMP_FAN_ONLY;
    }

    if(temp < HTW_TEMP_MIN || temp > HTW_TEMP_MAX) {
        temp = 24; // Default
    }

    uint8_t temp_code = TEMP_CODES[temp - HTW_TEMP_MIN];
    uint8_t mode_nibble = MODE_NIBBLES[mode];

    return mode_nibble | temp_code;
}

// Helper: Get FAN byte for state command
static uint8_t get_fan_byte(HtwMode mode, HtwFan fan) {
    // Auto and Dry modes force fan to 0xF8
    if(mode == HtwModeAuto || mode == HtwModeDry) {
        return FAN_CODE_FORCED;
    }

    if(fan >= HtwFanCount) {
        fan = HtwFanAuto;
    }

    return FAN_CODES[fan];
}

bool htw_ir_encode_state(
    HtwMode mode,
    HtwFan fan,
    uint8_t temp,
    uint32_t* timings,
    size_t* timings_count) {
    if(!timings || !timings_count) return false;
    if(mode == HtwModeOff || mode >= HtwModeCount) return false;

    uint8_t fan_byte = get_fan_byte(mode, fan);
    uint8_t mode_temp = get_mode_temp(mode, temp);

    uint8_t frame[6] = {
        FAMILY_A_B0, // b0 = 0x4D
        FAMILY_A_B1, // b1 = 0xB2 (inv of 0x4D)
        fan_byte, // b2 = FAN
        invert_byte(fan_byte), // b3 = INV(FAN)
        mode_temp, // b4 = MODE_TEMP
        invert_byte(mode_temp) // b5 = INV(MODE_TEMP)
    };

    encode_double_frame(frame, timings, timings_count);
    return true;
}

bool htw_ir_encode_toggle(HtwToggle toggle, uint32_t* timings, size_t* timings_count) {
    if(!timings || !timings_count) return false;
    if(toggle >= HtwToggleCount) return false;

    uint8_t frame[6];

    if(toggle == HtwTogglePowerOff) {
        memcpy(frame, POWER_OFF_FRAME, 6);
    } else if(toggle == HtwToggleSwing) {
        memcpy(frame, SWING_FRAME, 6);
    } else {
        // Family B toggles: LED, Turbo, Fresh, Clean
        uint8_t code = TOGGLE_CODES[toggle];
        frame[0] = FAMILY_B_B0; // 0xAD
        frame[1] = FAMILY_B_B1; // 0x52
        frame[2] = FAMILY_B_B2; // 0xAF
        frame[3] = FAMILY_B_B3; // 0x50
        frame[4] = code;
        frame[5] = invert_byte(code);
    }

    encode_double_frame(frame, timings, timings_count);
    return true;
}

bool htw_ir_encode_timer_on(
    HtwMode mode,
    HtwFan fan,
    uint8_t temp,
    uint8_t timer_step,
    uint32_t* timings,
    size_t* timings_count) {
    if(!timings || !timings_count) return false;
    if(timer_step < 1 || timer_step > HTW_TIMER_STEPS_COUNT) return false;
    if(mode == HtwModeOff || mode >= HtwModeCount) return false;

    uint8_t fan_byte = get_fan_byte(mode, fan);
    uint8_t mode_temp = get_mode_temp(mode, temp);

    // CMD = MODE_TEMP ^ 0xC0
    uint8_t cmd = mode_temp ^ 0xC0;

    // ARG = base_arg | on_flag
    const uint8_t step_idx = timer_step - 1;
    uint8_t arg = TIMER_STEPS[step_idx].base_arg | TIMER_STEPS[step_idx].on_flag;

    uint8_t frame[6] = {
        FAMILY_A_B0, // b0 = 0x4D
        FAMILY_A_B1, // b1 = 0xB2
        fan_byte, // b2 = FAN
        invert_byte(fan_byte), // b3 = INV(FAN)
        cmd, // b4 = CMD
        arg // b5 = ARG (NOT inverted - INV2 format)
    };

    encode_double_frame(frame, timings, timings_count);
    return true;
}

bool htw_ir_encode_timer_off(
    HtwMode mode,
    uint8_t temp,
    uint8_t timer_step,
    uint32_t* timings,
    size_t* timings_count) {
    if(!timings || !timings_count) return false;
    if(timer_step < 1 || timer_step > HTW_TIMER_STEPS_COUNT) return false;

    uint8_t mode_temp = get_mode_temp(mode, temp);

    const uint8_t step_idx = timer_step - 1;
    uint8_t base_arg = TIMER_STEPS[step_idx].base_arg;
    uint8_t page_mask = TIMER_STEPS[step_idx].off_page_mask;

    uint8_t frame[6] = {
        FAMILY_A_B0, // b0 = 0x4D
        FAMILY_A_B1, // b1 = 0xB2
        base_arg, // b2 = ARG_BASE
        invert_byte(base_arg), // b3 = INV(ARG_BASE)
        mode_temp | page_mask, // b4 = MODE_TEMP | PAGE_MASK
        0xFE // b5 = 0xFE (fixed)
    };

    encode_double_frame(frame, timings, timings_count);
    return true;
}

float htw_ir_get_timer_hours(uint8_t step) {
    if(step < 1 || step > HTW_TIMER_STEPS_COUNT) {
        return 0.0f;
    }
    return TIMER_STEPS[step - 1].hours_x10 / 10.0f;
}

const char* htw_ir_get_mode_name(HtwMode mode) {
    if(mode >= HtwModeCount) return "?";
    return MODE_NAMES[mode];
}

const char* htw_ir_get_fan_name(HtwFan fan) {
    if(fan >= HtwFanCount) return "?";
    return FAN_NAMES[fan];
}
