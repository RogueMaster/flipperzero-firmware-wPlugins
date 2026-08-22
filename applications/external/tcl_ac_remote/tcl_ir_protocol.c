#include "tcl_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// TCL112 A/C, 14-byte (112-bit) message. Also Daewoo, Electrolux, Leberg and
// Teknopoint units that use the same board.
// Ported from IRremoteESP8266 src/ir_Tcl.{h,cpp}, Tcl112Protocol union.
//
//   byte 0-2  fixed 0x23 0xCB 0x26
//   byte 3    message type, 0b01 = normal
//   byte 4    fixed 0x00
//   byte 5    power:1(b2) offTimer:1(b3) onTimer:1(b4) quiet:1(b5)
//             light:1(b6) econo:1(b7)
//   byte 6    mode:4  health:1(b4)  turbo:1(b5)
//   byte 7    temp:4  -- stored as 31 - degrees
//   byte 8    fan:3  swingV:3(b3-5)  timerIndicator:1(b6)
//   byte 12   swingH:1(b3)  halfDegree:1(b5)  isTcl:1(b7)
//   byte 13   sum of bytes 0..12
//
// Sent once, least significant bit first within each byte.
// ==========================================================================

#define STATE_LEN 14

// Line coding (microseconds)
#define HDR_MARK   3000
#define HDR_SPACE  1650
#define BIT_MARK   500
#define ONE_SPACE  1050
#define ZERO_SPACE 325

// Mode field (byte 6, bits 0-3)
#define TCL_HEAT 1
#define TCL_DRY  2
#define TCL_COOL 3
#define TCL_FAN  7
#define TCL_AUTO 8

// Fan field (byte 8, bits 0-2)
#define TCL_FAN_AUTO 0b000
#define TCL_FAN_LOW  0b010
#define TCL_FAN_MED  0b011
#define TCL_FAN_HIGH 0b101

// Vertical vane (byte 8, bits 3-5)
#define TCL_SWINGV_OFF     0b000
#define TCL_SWINGV_HIGHEST 0b001
#define TCL_SWINGV_HIGH    0b010
#define TCL_SWINGV_MIDDLE  0b011
#define TCL_SWINGV_LOW     0b100
#define TCL_SWINGV_LOWEST  0b101
#define TCL_SWINGV_ON      0b111

/// Temperature is stored counting down from this value
#define TCL_TEMP_BASE 31

/// byte 3, normal message
#define TCL_MSGTYPE_NORMAL 0b01

static const uint8_t MODE_CODES[TclModeCount] = {
    TCL_COOL, // Off - unused, the power bit carries it
    TCL_COOL,
    TCL_AUTO,
    TCL_DRY,
    TCL_HEAT,
    TCL_FAN,
};

static const uint8_t FAN_CODES[TclFanCount] = {
    TCL_FAN_AUTO,
    TCL_FAN_LOW,
    TCL_FAN_MED,
    TCL_FAN_HIGH,
};

static const uint8_t VANE_CODES[TclExtraCount] = {
    TCL_SWINGV_HIGHEST,
    TCL_SWINGV_HIGH,
    TCL_SWINGV_MIDDLE,
    TCL_SWINGV_LOW,
    TCL_SWINGV_LOWEST,
};

static const char* MODE_NAMES[TclModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[TclFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[TclToggleCount] =
    {"Power", "Swing", "Turbo", "Quiet", "Light", "Econo", "Health"};
static const char* EXTRA_NAMES[TclExtraCount] =
    {"Vane top", "Vane high", "Vane mid", "Vane low", "Vane bot"};

static uint8_t tcl_checksum(const uint8_t* st) {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < STATE_LEN - 1; i++) {
        sum += st[i];
    }
    return sum;
}

static void build_state(const TclRequest* req, bool power, uint8_t swing, uint8_t* st) {
    TclMode mode = req->mode;
    if(mode == TclModeOff || mode >= TclModeCount) mode = TclModeCool;

    TclFan fan = req->fan >= TclFanCount ? TclFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < TCL_TEMP_MIN) temp = TCL_TEMP_MIN;
    if(temp > TCL_TEMP_MAX) temp = TCL_TEMP_MAX;

    uint32_t tb = req->toggle_bits;

    memset(st, 0, STATE_LEN);
    st[0] = 0x23;
    st[1] = 0xCB;
    st[2] = 0x26;
    st[3] = TCL_MSGTYPE_NORMAL;
    st[4] = 0x00;

    st[5] = 0;
    if(power) st[5] |= 1 << 2;
    if((tb >> TclToggleQuiet) & 1) st[5] |= 1 << 5;
    if((tb >> TclToggleLight) & 1) st[5] |= 1 << 6;
    if((tb >> TclToggleEcono) & 1) st[5] |= 1 << 7;

    st[6] = (uint8_t)(MODE_CODES[mode] & 0x0F);
    if((tb >> TclToggleHealth) & 1) st[6] |= 1 << 4;
    if((tb >> TclToggleTurbo) & 1) st[6] |= 1 << 5;

    // Counts down from 31, so a higher setpoint is a smaller field value
    st[7] = (uint8_t)((TCL_TEMP_BASE - temp) & 0x0F);

    st[8] = (uint8_t)((FAN_CODES[fan] & 0x07) | ((swing & 0x07) << 3));

    st[12] = 0x00;
    st[13] = tcl_checksum(st);
}

static uint8_t swing_for(const TclRequest* req) {
    return ((req->toggle_bits >> TclToggleSwing) & 1) ? TCL_SWINGV_ON : TCL_SWINGV_OFF;
}

static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, TCL_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, STATE_LEN, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool tcl_ir_encode_state(const TclRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == TclModeOff || req->mode >= TclModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, swing_for(req), st);
    return encode_state_bytes(st, timings, timings_count);
}

bool tcl_ir_encode_toggle(
    const TclRequest* req,
    TclToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= TclToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, toggle != TclTogglePowerOff, swing_for(req), st);
    return encode_state_bytes(st, timings, timings_count);
}

bool tcl_ir_encode_extra(
    const TclRequest* req,
    TclExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= TclExtraCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, VANE_CODES[extra], st);
    return encode_state_bytes(st, timings, timings_count);
}

void tcl_ir_format_state(const TclRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == TclModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, swing_for(req), st);
    snprintf(out, len, "%02X %02X %02X", st[6], st[7], st[8]);
}

void tcl_ir_format_toggle(const TclRequest* req, TclToggle toggle, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= TclToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, toggle != TclTogglePowerOff, swing_for(req), st);
    snprintf(out, len, "%02X %02X %02X", st[5], st[6], st[8]);
}

void tcl_ir_format_extra(const TclRequest* req, TclExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= TclExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "vane %u", (unsigned)VANE_CODES[extra]);
}

bool tcl_ir_toggle_is_momentary(TclToggle toggle) {
    (void)toggle;
    return false;
}

bool tcl_ir_mode_locks_fan(TclMode mode) {
    (void)mode;
    return false;
}

bool tcl_ir_mode_has_no_temp(TclMode mode) {
    return mode == TclModeFan;
}

const char* tcl_ir_get_mode_name(TclMode mode) {
    return mode < TclModeCount ? MODE_NAMES[mode] : "?";
}

const char* tcl_ir_get_fan_name(TclFan fan) {
    return fan < TclFanCount ? FAN_NAMES[fan] : "?";
}

const char* tcl_ir_get_toggle_name(TclToggle toggle) {
    return toggle < TclToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* tcl_ir_get_extra_name(TclExtra extra) {
    return extra < TclExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t tcl_ir_get_option_count(void) {
    return 0; // one variant only
}

const char* tcl_ir_get_option_label(void) {
    return "Model";
}

const char* tcl_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* tcl_ir_get_protocol_name(void) {
    return "TCL";
}
