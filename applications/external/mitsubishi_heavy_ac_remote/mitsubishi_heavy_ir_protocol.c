#include "mitsubishi_heavy_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Mitsubishi Heavy Industries, 152-bit (19 byte) ZM-S series frame.
// Ported from IRremoteESP8266 src/ir_MitsubishiHeavy.{h,cpp}.
//
// Two things about this protocol are unlike the others here:
//   1. There is no checksum. Instead, from byte 4 onward every odd byte is
//      the bitwise complement of the byte before it.
//   2. The line coding is reversed: a one is the SHORT space (420us) and a
//      zero is the LONG one (1220us).
//
//   byte 0-4  signature AD 51 3C E5 1A  (E5/1A is already an inverted pair)
//   byte 5    mode:3 power:1(b3) clean:1(b5) filter:1(b6)
//   byte 7    temp:4  -- degrees - 17
//   byte 9    fan:4
//   byte 11   three:1(b1) d:1(b4) swingV:3(b5-7)
//   byte 13   swingH:4
//   byte 15   night:1(b6) silent:1(b7)
//   byte 17   0x80
//   even byte + 1 = its complement
// ==========================================================================

#define STATE_LEN 19
#define SIG_LEN   5

// Line coding (microseconds). One and zero spaces are deliberately this way
// round - see the note above.
#define HDR_MARK   3140
#define HDR_SPACE  1630
#define BIT_MARK   370
#define ONE_SPACE  420
#define ZERO_SPACE 1220

// Mode field (byte 5, bits 0-2)
#define MH_AUTO 0
#define MH_COOL 1
#define MH_DRY  2
#define MH_FAN  3
#define MH_HEAT 4

// Fan field (byte 9, bits 0-3)
#define MH_FAN_AUTO 0x0
#define MH_FAN_LOW  0x1
#define MH_FAN_MED  0x2
#define MH_FAN_HIGH 0x3

// Vertical vane (byte 11, bits 5-7)
#define MH_SWINGV_AUTO    0
#define MH_SWINGV_HIGHEST 1
#define MH_SWINGV_HIGH    2
#define MH_SWINGV_MIDDLE  3
#define MH_SWINGV_LOW     4
#define MH_SWINGV_LOWEST  5
#define MH_SWINGV_OFF     6

// Horizontal vane (byte 13, bits 0-3)
#define MH_SWINGH_AUTO 0
#define MH_SWINGH_OFF  8

static const uint8_t ZMS_SIG[SIG_LEN] = {0xAD, 0x51, 0x3C, 0xE5, 0x1A};

static const uint8_t MODE_CODES[MitsubishiHeavyModeCount] = {
    MH_COOL, // Off - unused, the power bit carries it
    MH_COOL,
    MH_AUTO,
    MH_DRY,
    MH_HEAT,
    MH_FAN,
};

static const uint8_t FAN_CODES[MitsubishiHeavyFanCount] = {
    MH_FAN_AUTO,
    MH_FAN_LOW,
    MH_FAN_MED,
    MH_FAN_HIGH,
};

static const char* MODE_NAMES[MitsubishiHeavyModeCount] =
    {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[MitsubishiHeavyFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[MitsubishiHeavyToggleCount] =
    {"Power", "Swing", "Clean", "Filter", "Night", "Silent"};
static const char* EXTRA_NAMES[MitsubishiHeavyExtraCount] =
    {"Vane top", "Vane high", "Vane mid", "Vane low", "Vane bot", "SwingH on", "SwingH off"};

/// From byte 4 on, every odd byte is the complement of the one before it.
static void invert_byte_pairs(uint8_t* st) {
    for(uint8_t i = SIG_LEN - 1; i < STATE_LEN; i += 2) {
        st[i] = (uint8_t)~st[i - 1];
    }
}

static void build_state(
    const MitsubishiHeavyRequest* req,
    bool power,
    uint8_t swing_v,
    uint8_t swing_h,
    uint8_t* st) {
    MitsubishiHeavyMode mode = req->mode;
    if(mode == MitsubishiHeavyModeOff || mode >= MitsubishiHeavyModeCount)
        mode = MitsubishiHeavyModeCool;

    MitsubishiHeavyFan fan = req->fan >= MitsubishiHeavyFanCount ? MitsubishiHeavyFanAuto :
                                                                   req->fan;

    uint8_t temp = req->temp;
    if(temp < MITSUBISHI_HEAVY_TEMP_MIN) temp = MITSUBISHI_HEAVY_TEMP_MIN;
    if(temp > MITSUBISHI_HEAVY_TEMP_MAX) temp = MITSUBISHI_HEAVY_TEMP_MAX;

    uint32_t tb = req->toggle_bits;

    memset(st, 0, STATE_LEN);
    memcpy(st, ZMS_SIG, SIG_LEN);

    st[5] = (uint8_t)(MODE_CODES[mode] & 0x07);
    if(power) st[5] |= 1 << 3;
    if((tb >> MitsubishiHeavyToggleClean) & 1) st[5] |= 1 << 5;
    if((tb >> MitsubishiHeavyToggleFilter) & 1) st[5] |= 1 << 6;

    st[7] = (uint8_t)((temp - MITSUBISHI_HEAVY_TEMP_MIN) & 0x0F);
    st[9] = (uint8_t)(FAN_CODES[fan] & 0x0F);
    st[11] = (uint8_t)((swing_v & 0x07) << 5);
    st[13] = (uint8_t)(swing_h & 0x0F);

    st[15] = 0;
    if((tb >> MitsubishiHeavyToggleNight) & 1) st[15] |= 1 << 6;
    if((tb >> MitsubishiHeavyToggleSilent) & 1) st[15] |= 1 << 7;

    st[17] = 0x80;

    invert_byte_pairs(st);
}

static uint8_t swing_v_for(const MitsubishiHeavyRequest* req) {
    return ((req->toggle_bits >> MitsubishiHeavyToggleSwing) & 1) ? MH_SWINGV_AUTO : MH_SWINGV_OFF;
}

static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, MITSUBISHI_HEAVY_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, STATE_LEN, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool mitsubishi_heavy_ir_encode_state(
    const MitsubishiHeavyRequest* req,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == MitsubishiHeavyModeOff || req->mode >= MitsubishiHeavyModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, swing_v_for(req), MH_SWINGH_AUTO, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool mitsubishi_heavy_ir_encode_toggle(
    const MitsubishiHeavyRequest* req,
    MitsubishiHeavyToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= MitsubishiHeavyToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(
        req, toggle != MitsubishiHeavyTogglePowerOff, swing_v_for(req), MH_SWINGH_AUTO, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool mitsubishi_heavy_ir_encode_extra(
    const MitsubishiHeavyRequest* req,
    MitsubishiHeavyExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= MitsubishiHeavyExtraCount) return false;

    static const uint8_t VANE[5] = {
        MH_SWINGV_HIGHEST, MH_SWINGV_HIGH, MH_SWINGV_MIDDLE, MH_SWINGV_LOW, MH_SWINGV_LOWEST};

    uint8_t st[STATE_LEN];
    if(extra <= MitsubishiHeavyExtraVaneLowest) {
        build_state(req, true, VANE[extra], MH_SWINGH_AUTO, st);
    } else {
        uint8_t h = extra == MitsubishiHeavyExtraSwingHAuto ? MH_SWINGH_AUTO : MH_SWINGH_OFF;
        build_state(req, true, swing_v_for(req), h, st);
    }
    return encode_state_bytes(st, timings, timings_count);
}

void mitsubishi_heavy_ir_format_state(const MitsubishiHeavyRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == MitsubishiHeavyModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, swing_v_for(req), MH_SWINGH_AUTO, st);
    snprintf(out, len, "%02X %02X %02X", st[5], st[7], st[9]);
}

void mitsubishi_heavy_ir_format_toggle(
    const MitsubishiHeavyRequest* req,
    MitsubishiHeavyToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= MitsubishiHeavyToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(
        req, toggle != MitsubishiHeavyTogglePowerOff, swing_v_for(req), MH_SWINGH_AUTO, st);
    snprintf(out, len, "%02X %02X %02X", st[5], st[11], st[15]);
}

void mitsubishi_heavy_ir_format_extra(
    const MitsubishiHeavyRequest* req,
    MitsubishiHeavyExtra extra,
    char* out,
    size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= MitsubishiHeavyExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "%s", EXTRA_NAMES[extra]);
}

bool mitsubishi_heavy_ir_toggle_is_momentary(MitsubishiHeavyToggle toggle) {
    (void)toggle;
    return false;
}

bool mitsubishi_heavy_ir_mode_locks_fan(MitsubishiHeavyMode mode) {
    (void)mode;
    return false;
}

bool mitsubishi_heavy_ir_mode_has_no_temp(MitsubishiHeavyMode mode) {
    return mode == MitsubishiHeavyModeFan;
}

const char* mitsubishi_heavy_ir_get_mode_name(MitsubishiHeavyMode mode) {
    return mode < MitsubishiHeavyModeCount ? MODE_NAMES[mode] : "?";
}

const char* mitsubishi_heavy_ir_get_fan_name(MitsubishiHeavyFan fan) {
    return fan < MitsubishiHeavyFanCount ? FAN_NAMES[fan] : "?";
}

const char* mitsubishi_heavy_ir_get_toggle_name(MitsubishiHeavyToggle toggle) {
    return toggle < MitsubishiHeavyToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* mitsubishi_heavy_ir_get_extra_name(MitsubishiHeavyExtra extra) {
    return extra < MitsubishiHeavyExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t mitsubishi_heavy_ir_get_option_count(void) {
    return 0; // one variant only
}

const char* mitsubishi_heavy_ir_get_option_label(void) {
    return "Model";
}

const char* mitsubishi_heavy_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* mitsubishi_heavy_ir_get_protocol_name(void) {
    return "MitsuHvy";
}
