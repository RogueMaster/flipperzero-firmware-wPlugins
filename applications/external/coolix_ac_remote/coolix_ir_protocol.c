#include "coolix_ir_protocol.h"
#include <stdio.h>

_Static_assert(COOLIX_IR_MAX_TIMINGS >= 199, "COOLIX_IR_MAX_TIMINGS too small for protocol");

// Base word: 0xB2 identifies the family, the 0x0F nibble is fixed.
// Fan occupies 0xF000, temperature 0x00F0, mode 0x000C.
#define COOLIX_BASE 0xB20F00

// Mode bits (0x000C)
#define COOLIX_MODE_COOL    0x0
#define COOLIX_MODE_DRY_FAN 0x4
#define COOLIX_MODE_AUTO    0x8
#define COOLIX_MODE_HEAT    0xC

// Fan bits (0xF000)
#define COOLIX_FAN_MODE_AUTO_DRY 0x1000 // forced in Auto and Dry
#define COOLIX_FAN_AUTO          0xB000
#define COOLIX_FAN_MIN           0x9000
#define COOLIX_FAN_MED           0x5000
#define COOLIX_FAN_MAX           0x3000

// Temperature nibble stand-in used by Fan-only mode
#define COOLIX_FAN_TEMP_CODE 0xE0

// One-shot command words
#define COOLIX_OFF        0xB27BE0
#define COOLIX_SWING      0xB26BE0
#define COOLIX_SWING_V    0xB20FE0
#define COOLIX_SWING_H    0xB2F5A2
#define COOLIX_TURBO      0xB5F5A2
#define COOLIX_LED        0xB5F5A5
#define COOLIX_SLEEP      0xB2E003
#define COOLIX_CLEAN      0xB5F5AA
#define COOLIX_SILENCE_FP 0xB5F5B6
#define COOLIX_CMD_FAN    0xB2BFE4

// Temperature nibble per degree, 17..30 C. Not monotonic - it is a gray-ish
// code, so it has to stay a table.
#define COOLIX_TEMP_RANGE (COOLIX_TEMP_MAX - COOLIX_TEMP_MIN + 1)
static const uint8_t TEMP_CODES[COOLIX_TEMP_RANGE] = {
    0x00, // 17 C
    0x10, // 18 C
    0x30, // 19 C
    0x20, // 20 C
    0x60, // 21 C
    0x70, // 22 C
    0x50, // 23 C
    0x40, // 24 C
    0xC0, // 25 C
    0xD0, // 26 C
    0x90, // 27 C
    0x80, // 28 C
    0xA0, // 29 C
    0xB0, // 30 C
};

static const uint32_t FAN_CODES[CoolixFanCount] = {
    COOLIX_FAN_AUTO, // Auto
    COOLIX_FAN_MIN, // Low
    COOLIX_FAN_MED, // Med
    COOLIX_FAN_MAX, // High
};

static const uint32_t MODE_BITS[CoolixModeCount] = {
    0, // Off (handled separately)
    COOLIX_MODE_COOL,
    COOLIX_MODE_AUTO,
    COOLIX_MODE_DRY_FAN, // Dry: Dry/Fan bits + forced Auto fan
    COOLIX_MODE_HEAT,
    COOLIX_MODE_DRY_FAN, // Fan-only: Dry/Fan bits + fan temperature code
};

static const uint32_t TOGGLE_CODES[CoolixToggleCount] = {
    COOLIX_OFF,
    COOLIX_SWING,
    COOLIX_SWING_V,
    COOLIX_TURBO,
    COOLIX_LED,
    COOLIX_SLEEP,
};

static const uint32_t EXTRA_CODES[CoolixExtraCount] = {
    COOLIX_SILENCE_FP,
    COOLIX_SWING_H,
    COOLIX_CMD_FAN,
    COOLIX_CLEAN,
};

static const char* MODE_NAMES[CoolixModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[CoolixFanCount] = {"Auto", "Low", "Med", "High"};
static const char* EXTRA_NAMES[CoolixExtraCount] = {"Silence", "Swing H", "Fan cmd", "Clean"};

// Fan is fixed to Auto by the unit in these modes
static inline bool mode_forces_auto_fan(CoolixMode mode) {
    return mode == CoolixModeAuto || mode == CoolixModeDry;
}

// One bit: mark plus a space whose length carries the value
static void add_bit(uint32_t* timings, size_t* idx, bool bit) {
    timings[(*idx)++] = COOLIX_IR_BIT_MARK;
    timings[(*idx)++] = bit ? COOLIX_IR_BIT1_SPACE : COOLIX_IR_BIT0_SPACE;
}

// One byte, MSB first, sent normally and then inverted
static void add_byte(uint32_t* timings, size_t* idx, uint8_t byte) {
    for(uint8_t mask = 0x80; mask; mask >>= 1) {
        add_bit(timings, idx, (byte & mask) != 0);
    }
    for(uint8_t mask = 0x80; mask; mask >>= 1) {
        add_bit(timings, idx, (byte & mask) == 0);
    }
}

// Leader, three payload bytes (most significant first), stop mark
static void encode_frame(uint32_t code, uint32_t* timings, size_t* idx) {
    timings[(*idx)++] = COOLIX_IR_LEAD_MARK;
    timings[(*idx)++] = COOLIX_IR_LEAD_SPACE;

    add_byte(timings, idx, (code >> 16) & 0xFF);
    add_byte(timings, idx, (code >> 8) & 0xFF);
    add_byte(timings, idx, code & 0xFF);

    timings[(*idx)++] = COOLIX_IR_STOP_MARK;
}

uint32_t coolix_ir_build_state(CoolixMode mode, CoolixFan fan, uint8_t temp) {
    if(mode == CoolixModeOff || mode >= CoolixModeCount) return COOLIX_OFF;

    uint32_t code = COOLIX_BASE | MODE_BITS[mode];

    if(mode == CoolixModeFan) {
        // Fan-only carries no setpoint, the temperature nibble is a marker
        code |= COOLIX_FAN_TEMP_CODE;
    } else {
        if(temp < COOLIX_TEMP_MIN) temp = COOLIX_TEMP_MIN;
        if(temp > COOLIX_TEMP_MAX) temp = COOLIX_TEMP_MAX;
        code |= TEMP_CODES[temp - COOLIX_TEMP_MIN];
    }

    if(mode_forces_auto_fan(mode)) {
        code |= COOLIX_FAN_MODE_AUTO_DRY;
    } else {
        code |= FAN_CODES[fan >= CoolixFanCount ? CoolixFanAuto : fan];
    }

    return code;
}

uint32_t coolix_ir_get_toggle_code(CoolixToggle toggle) {
    if(toggle >= CoolixToggleCount) return COOLIX_OFF;
    return TOGGLE_CODES[toggle];
}

uint32_t coolix_ir_get_extra_code(CoolixExtra extra) {
    if(extra >= CoolixExtraCount) return COOLIX_OFF;
    return EXTRA_CODES[extra];
}

bool coolix_ir_encode_code(uint32_t code, uint32_t* timings, size_t* timings_count) {
    if(!timings || !timings_count) return false;

    size_t idx = 0;
    encode_frame(code, timings, &idx);
    timings[idx++] = COOLIX_IR_GAP;
    encode_frame(code, timings, &idx);
    // No trailing gap - the signal ends on a mark

    *timings_count = idx;
    return true;
}

bool coolix_ir_encode_state(
    CoolixMode mode,
    CoolixFan fan,
    uint8_t temp,
    uint32_t* timings,
    size_t* timings_count) {
    if(mode == CoolixModeOff || mode >= CoolixModeCount) return false;
    return coolix_ir_encode_code(coolix_ir_build_state(mode, fan, temp), timings, timings_count);
}

bool coolix_ir_encode_toggle(CoolixToggle toggle, uint32_t* timings, size_t* timings_count) {
    if(toggle >= CoolixToggleCount) return false;
    return coolix_ir_encode_code(TOGGLE_CODES[toggle], timings, timings_count);
}

bool coolix_ir_encode_extra(CoolixExtra extra, uint32_t* timings, size_t* timings_count) {
    if(extra >= CoolixExtraCount) return false;
    return coolix_ir_encode_code(EXTRA_CODES[extra], timings, timings_count);
}

const char* coolix_ir_get_mode_name(CoolixMode mode) {
    if(mode >= CoolixModeCount) return "?";
    return MODE_NAMES[mode];
}

const char* coolix_ir_get_fan_name(CoolixFan fan) {
    if(fan >= CoolixFanCount) return "?";
    return FAN_NAMES[fan];
}

/// Short payload string for a main-screen button, shown on the Extra screen.
void coolix_ir_format_toggle(CoolixToggle toggle, char* out, size_t len) {
    if(!out || !len) return;
    if(toggle >= CoolixToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "0x%06lX", (unsigned long)TOGGLE_CODES[toggle]);
}

const char* coolix_ir_get_extra_name(CoolixExtra extra) {
    if(extra >= CoolixExtraCount) return "?";
    return EXTRA_NAMES[extra];
}
