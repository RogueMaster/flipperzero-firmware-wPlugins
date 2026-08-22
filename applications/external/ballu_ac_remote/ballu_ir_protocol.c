#include "ballu_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Ballu A/C with the YKR-K/002E handset, 13-byte frame.
// Ported from ESPHome esphome/components/ballu/ballu.cpp. IRremoteESP8266 has
// no Ballu protocol, so ESPHome is the only reference for this one.
//
//   byte 0   0xC3
//   byte 1   (temp - 8) << 3, plus the vertical swing field
//   byte 2   horizontal swing field
//   byte 4   fan
//   byte 6   mode
//   byte 9   power
//   byte 11  0x1E
//   byte 12  sum of bytes 0..11
//
// The swing fields are inverted: the bits are SET when swing is OFF.
// Sent once, least significant bit first within each byte.
// ==========================================================================

#define STATE_LEN 13

// Line coding (microseconds)
#define HDR_MARK   9000
#define HDR_SPACE  4500
#define BIT_MARK   575
#define ONE_SPACE  1675
#define ZERO_SPACE 550

// Mode field (byte 6)
#define B_AUTO 0x00
#define B_COOL 0x20
#define B_DRY  0x40
#define B_HEAT 0x80
#define B_FAN  0xC0

// Fan field (byte 4)
#define B_FAN_AUTO 0xA0
#define B_FAN_HIGH 0x20
#define B_FAN_MED  0x40
#define B_FAN_LOW  0x60

// Swing fields. Counter-intuitively, these bits mean "not swinging".
#define B_SWING_VER_OFF 0x07
#define B_SWING_HOR_OFF 0xE0

#define B_POWER 0x20

static const uint8_t MODE_CODES[BalluModeCount] = {
    B_AUTO, // Off - the mode field still carries auto, power byte carries off
    B_COOL,
    B_AUTO,
    B_DRY,
    B_HEAT,
    B_FAN,
};

static const uint8_t FAN_CODES[BalluFanCount] = {
    B_FAN_AUTO,
    B_FAN_LOW,
    B_FAN_MED,
    B_FAN_HIGH,
};

static const char* MODE_NAMES[BalluModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[BalluFanCount] = {"Auto", "Low", "Med", "High"};
// Long labels wrap onto a second line, so the space here is a break hint.
static const char* TOGGLE_NAMES[BalluToggleCount] = {"Power", "Swing V", "Swing H"};
static const char* EXTRA_NAMES[BalluExtraCount] = {"Swing both", "Swing off"};

static void
    build_state(const BalluRequest* req, bool power, bool swing_ver, bool swing_hor, uint8_t* st) {
    BalluMode mode = req->mode;
    if(mode >= BalluModeCount) mode = BalluModeCool;

    BalluFan fan = req->fan >= BalluFanCount ? BalluFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < BALLU_TEMP_MIN) temp = BALLU_TEMP_MIN;
    if(temp > BALLU_TEMP_MAX) temp = BALLU_TEMP_MAX;

    memset(st, 0, STATE_LEN);
    st[0] = 0xC3;
    st[1] = (uint8_t)(((temp - 8) << 3) | (swing_ver ? 0 : B_SWING_VER_OFF));
    st[2] = swing_hor ? 0 : B_SWING_HOR_OFF;
    st[4] = FAN_CODES[fan];
    st[6] = MODE_CODES[mode];
    st[9] = power ? B_POWER : 0;
    st[11] = 0x1E;

    uint8_t sum = 0;
    for(uint8_t i = 0; i < STATE_LEN - 1; i++) {
        sum += st[i];
    }
    st[12] = sum;
}

static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, BALLU_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, STATE_LEN, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

static bool sv(const BalluRequest* req) {
    return (req->toggle_bits >> BalluToggleSwingV) & 1;
}

static bool sh(const BalluRequest* req) {
    return (req->toggle_bits >> BalluToggleSwingH) & 1;
}

bool ballu_ir_encode_state(const BalluRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == BalluModeOff || req->mode >= BalluModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, sv(req), sh(req), st);
    return encode_state_bytes(st, timings, timings_count);
}

bool ballu_ir_encode_toggle(
    const BalluRequest* req,
    BalluToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= BalluToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, toggle != BalluTogglePowerOff, sv(req), sh(req), st);
    return encode_state_bytes(st, timings, timings_count);
}

bool ballu_ir_encode_extra(
    const BalluRequest* req,
    BalluExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= BalluExtraCount) return false;

    bool both = extra == BalluExtraSwingBoth;
    uint8_t st[STATE_LEN];
    build_state(req, true, both, both, st);
    return encode_state_bytes(st, timings, timings_count);
}

void ballu_ir_format_state(const BalluRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == BalluModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, sv(req), sh(req), st);
    snprintf(out, len, "%02X %02X %02X", st[1], st[4], st[6]);
}

void ballu_ir_format_toggle(const BalluRequest* req, BalluToggle toggle, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= BalluToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, toggle != BalluTogglePowerOff, sv(req), sh(req), st);
    snprintf(out, len, "%02X %02X %02X", st[1], st[2], st[9]);
}

void ballu_ir_format_extra(const BalluRequest* req, BalluExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    snprintf(out, len, extra == BalluExtraSwingBoth ? "V+H on" : "V+H off");
}

bool ballu_ir_toggle_is_momentary(BalluToggle toggle) {
    (void)toggle;
    return false;
}

bool ballu_ir_mode_locks_fan(BalluMode mode) {
    (void)mode;
    return false;
}

bool ballu_ir_mode_has_no_temp(BalluMode mode) {
    return mode == BalluModeFan;
}

const char* ballu_ir_get_mode_name(BalluMode mode) {
    return mode < BalluModeCount ? MODE_NAMES[mode] : "?";
}

const char* ballu_ir_get_fan_name(BalluFan fan) {
    return fan < BalluFanCount ? FAN_NAMES[fan] : "?";
}

const char* ballu_ir_get_toggle_name(BalluToggle toggle) {
    return toggle < BalluToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* ballu_ir_get_extra_name(BalluExtra extra) {
    return extra < BalluExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t ballu_ir_get_option_count(void) {
    return 0; // one variant only
}

const char* ballu_ir_get_option_label(void) {
    return "Model";
}

const char* ballu_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* ballu_ir_get_protocol_name(void) {
    return "Ballu";
}
