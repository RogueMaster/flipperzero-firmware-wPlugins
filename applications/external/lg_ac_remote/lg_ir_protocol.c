#include "lg_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>

// ==========================================================================
// LG A/C, 28-bit protocol (models GE6711AR2853M / LG6711A20083V).
//
// Ported from IRremoteESP8266 src/ir_LG.{h,cpp} and its LGProtocol union.
// ESPHome's climate_ir_lg builds the same word.
//
// Word layout, least significant bit first:
//   bits  0-3   checksum
//   bits  4-7   fan
//   bits  8-11  temperature - 15
//   bits 12-14  mode
//   bits 15-17  unused
//   bits 18-19  power (0 = on, 3 = off)
//   bits 20-27  signature, always 0x88
// ==========================================================================

// Line coding (microseconds). Bit timings are shared; the header is not.
#define HDR_MARK      8500
#define HDR_SPACE     4250
#define LG2_HDR_MARK  3200
#define LG2_HDR_SPACE 9900
#define BIT_MARK      550
#define LG2_BIT_MARK  480
#define ONE_SPACE     1600
#define ZERO_SPACE    550

#define LG_BITS 28

// Field values
#define LG_SIGNATURE   0x88
#define LG_TEMP_ADJUST 15

#define LG_POWER_ON  0
#define LG_POWER_OFF 3

#define LG_MODE_COOL 0
#define LG_MODE_DRY  1
#define LG_MODE_FAN  2
#define LG_MODE_AUTO 3
#define LG_MODE_HEAT 4

#define LG_FAN_LOWEST 0
#define LG_FAN_LOW    1
#define LG_FAN_MEDIUM 2
#define LG_FAN_MAX    4
#define LG_FAN_AUTO   5

// Complete one-shot words
#define LG_OFF_COMMAND     0x88C0051
#define LG_LIGHT_TOGGLE    0x88C00A6
#define LG_SWINGV_TOGGLE   0x8810001
#define LG_SWINGV_LOWEST   0x8813048
#define LG_SWINGV_LOW      0x8813059
#define LG_SWINGV_MIDDLE   0x881306A
#define LG_SWINGV_UPPERMID 0x881307B
#define LG_SWINGV_HIGH     0x881308C
#define LG_SWINGV_HIGHEST  0x881309D
#define LG_SWINGH_AUTO     0x881316B
#define LG_SWINGH_OFF      0x881317C

static const uint8_t MODE_CODES[LgModeCount] = {
    LG_MODE_COOL, // Off - unused, power-off is its own word
    LG_MODE_COOL,
    LG_MODE_AUTO,
    LG_MODE_DRY,
    LG_MODE_HEAT,
    LG_MODE_FAN,
};

static const uint8_t FAN_CODES[LgFanCount] = {
    LG_FAN_AUTO,
    LG_FAN_LOW,
    LG_FAN_MEDIUM,
    LG_FAN_MAX,
};

static const uint32_t TOGGLE_CODES[LgToggleCount] = {
    LG_OFF_COMMAND,
    LG_SWINGV_TOGGLE,
    LG_LIGHT_TOGGLE,
};

static const uint32_t EXTRA_CODES[LgExtraCount] = {
    LG_SWINGV_LOWEST,
    LG_SWINGV_LOW,
    LG_SWINGV_MIDDLE,
    LG_SWINGV_UPPERMID,
    LG_SWINGV_HIGH,
    LG_SWINGV_HIGHEST,
    LG_SWINGH_AUTO,
    LG_SWINGH_OFF,
};

static const char* MODE_NAMES[LgModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[LgFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[LgToggleCount] = {"Power", "Swing", "Light"};
static const char* EXTRA_NAMES[LgExtraCount] =
    {"Vane 1", "Vane 2", "Vane 3", "Vane 4", "Vane 5", "Vane 6", "SwingH on", "SwingH off"};
static const char* MODEL_NAMES[LgModelCount] = {"LG", "LG2"};

/// Sum of the four nibbles above the checksum nibble, low 4 bits kept.
static uint8_t lg_checksum(uint32_t raw) {
    uint32_t body = raw >> 4;
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 4; i++) {
        sum += (body >> (i * 4)) & 0xF;
    }
    return sum & 0xF;
}

static uint32_t build_state(LgMode mode, LgFan fan, uint8_t temp) {
    if(mode == LgModeOff || mode >= LgModeCount) return LG_OFF_COMMAND;
    if(fan >= LgFanCount) fan = LgFanAuto;

    if(temp < LG_TEMP_MIN) temp = LG_TEMP_MIN;
    if(temp > LG_TEMP_MAX) temp = LG_TEMP_MAX;

    uint32_t raw = 0;
    raw |= (uint32_t)LG_SIGNATURE << 20;
    raw |= (uint32_t)LG_POWER_ON << 18;
    raw |= (uint32_t)(MODE_CODES[mode] & 0x7) << 12;
    raw |= (uint32_t)((temp - LG_TEMP_ADJUST) & 0xF) << 8;
    raw |= (uint32_t)(FAN_CODES[fan] & 0xF) << 4;
    raw |= lg_checksum(raw);
    return raw;
}

static bool encode_word(uint32_t code, uint8_t model, uint32_t* timings, size_t* count) {
    bool lg2 = model == LgModelLg2;
    uint32_t hdr_mark = lg2 ? LG2_HDR_MARK : HDR_MARK;
    uint32_t hdr_space = lg2 ? LG2_HDR_SPACE : HDR_SPACE;
    uint32_t bit_mark = lg2 ? LG2_BIT_MARK : BIT_MARK;

    IrBuild b = ir_build_init(timings, LG_IR_MAX_TIMINGS);
    ir_item(&b, hdr_mark, hdr_space);
    ir_int_msb(&b, code, LG_BITS, bit_mark, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, bit_mark, count);
}

// LG is a short-code protocol: each button has its own complete word, so the
// request's toggle bits never enter the frame.
bool lg_ir_encode_state(const LgRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == LgModeOff || req->mode >= LgModeCount) return false;
    return encode_word(
        build_state(req->mode, req->fan, req->temp), req->option, timings, timings_count);
}

bool lg_ir_encode_toggle(
    const LgRequest* req,
    LgToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= LgToggleCount) return false;
    return encode_word(TOGGLE_CODES[toggle], req->option, timings, timings_count);
}

bool lg_ir_encode_extra(
    const LgRequest* req,
    LgExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= LgExtraCount) return false;
    return encode_word(EXTRA_CODES[extra], req->option, timings, timings_count);
}

void lg_ir_format_state(const LgRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    snprintf(out, len, "%07lX", (unsigned long)build_state(req->mode, req->fan, req->temp));
}

void lg_ir_format_toggle(const LgRequest* req, LgToggle toggle, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(toggle >= LgToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "%07lX", (unsigned long)TOGGLE_CODES[toggle]);
}

void lg_ir_format_extra(const LgRequest* req, LgExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= LgExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "%07lX", (unsigned long)EXTRA_CODES[extra]);
}

bool lg_ir_toggle_is_momentary(LgToggle toggle) {
    (void)toggle;
    return false;
}

bool lg_ir_mode_locks_fan(LgMode mode) {
    // LG accepts any fan speed in any mode
    (void)mode;
    return false;
}

bool lg_ir_mode_has_no_temp(LgMode mode) {
    return mode == LgModeFan;
}

const char* lg_ir_get_mode_name(LgMode mode) {
    return mode < LgModeCount ? MODE_NAMES[mode] : "?";
}

const char* lg_ir_get_fan_name(LgFan fan) {
    return fan < LgFanCount ? FAN_NAMES[fan] : "?";
}

const char* lg_ir_get_toggle_name(LgToggle toggle) {
    return toggle < LgToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* lg_ir_get_extra_name(LgExtra extra) {
    return extra < LgExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t lg_ir_get_option_count(void) {
    return LgModelCount;
}

const char* lg_ir_get_option_label(void) {
    return "Protocol";
}

const char* lg_ir_get_option_name(uint8_t option) {
    return option < LgModelCount ? MODEL_NAMES[option] : "?";
}

const char* lg_ir_get_protocol_name(void) {
    return "LG";
}
