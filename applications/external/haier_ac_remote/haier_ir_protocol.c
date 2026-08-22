#include "haier_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Haier YR-W02 remote, 14-byte frame. Also Daichi and Mabe units.
// Ported from IRremoteESP8266 src/ir_Haier.{h,cpp}. IRHaierACYRW02 derives
// from IRHaierAC176, so the 176-bit union describes this layout too - YRW02
// simply stops after byte 13.
//
//   byte 0   model, 0xA6
//   byte 1   swingV:4  temp:4 (degrees - 16)
//   byte 2   swingH:3 (bits 5-7)
//   byte 3   health:1(b1)  timerMode:3(b5-7)
//   byte 4   power:1 (b6)
//   byte 5   offTimerHrs:5  fan:3 (b5-7)
//   byte 6   offTimerMins:6  turbo:1(b6)  quiet:1(b7)
//   byte 7   onTimerHrs:5  mode:3 (b5-7)
//   byte 8   onTimerMins:6  sleep:1(b7)
//   byte 12  button:5  lock:1(b5)
//   byte 13  sum of bytes 0..12
//
// The frame is preceded by an extra 3000us mark and space before the real
// header, and is sent most significant bit first.
// ==========================================================================

#define STATE_LEN 14

// Line coding (microseconds)
#define LEAD_MARK  3000
#define LEAD_SPACE 3000
#define HDR_MARK   3000
#define HDR_SPACE  4300
#define BIT_MARK   520
#define ONE_SPACE  1650
#define ZERO_SPACE 650

#define HAIER_MODEL_A 0xA6

// Mode field (byte 7, bits 5-7)
#define H_AUTO 0b000
#define H_COOL 0b001
#define H_DRY  0b010
#define H_HEAT 0b100
#define H_FAN  0b110

// Fan field (byte 5, bits 5-7)
#define H_FAN_HIGH 0b001
#define H_FAN_MED  0b010
#define H_FAN_LOW  0b011
#define H_FAN_AUTO 0b101

// Vertical vane (byte 1, bits 0-3)
#define H_SWINGV_OFF    0x0
#define H_SWINGV_TOP    0x1
#define H_SWINGV_MIDDLE 0x2
#define H_SWINGV_BOTTOM 0x3
#define H_SWINGV_DOWN   0xA
#define H_SWINGV_AUTO   0xC

// Horizontal vane (byte 2, bits 5-7)
#define H_SWINGH_MIDDLE 0x0
#define H_SWINGH_AUTO   0x7

// Button codes (byte 12, bits 0-4)
#define H_BTN_SWINGV 0b00010
#define H_BTN_SWINGH 0b00011
#define H_BTN_POWER  0b00101
#define H_BTN_MODE   0b00110
#define H_BTN_HEALTH 0b00111
#define H_BTN_TURBO  0b01000
#define H_BTN_SLEEP  0b01011

static const uint8_t MODE_CODES[HaierModeCount] = {
    H_COOL, // Off - unused, the power bit carries it
    H_COOL,
    H_AUTO,
    H_DRY,
    H_HEAT,
    H_FAN,
};

static const uint8_t FAN_CODES[HaierFanCount] = {
    H_FAN_AUTO,
    H_FAN_LOW,
    H_FAN_MED,
    H_FAN_HIGH,
};

static const uint8_t TOGGLE_BUTTONS[HaierToggleCount] = {
    H_BTN_POWER,
    H_BTN_SWINGV,
    H_BTN_TURBO,
    H_BTN_TURBO, // quiet shares the turbo key on this handset
    H_BTN_HEALTH,
    H_BTN_SLEEP,
};

static const char* MODE_NAMES[HaierModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[HaierFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[HaierToggleCount] =
    {"Power", "Swing", "Turbo", "Quiet", "Health", "Sleep"};
static const char* EXTRA_NAMES[HaierExtraCount] =
    {"Vane top", "Vane mid", "Vane bot", "Vane down", "SwingH on", "SwingH mid"};

static void build_state(
    const HaierRequest* req,
    bool power,
    uint8_t swing_v,
    uint8_t swing_h,
    uint8_t button,
    uint8_t* st) {
    HaierMode mode = req->mode;
    if(mode == HaierModeOff || mode >= HaierModeCount) mode = HaierModeCool;

    HaierFan fan = req->fan >= HaierFanCount ? HaierFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < HAIER_TEMP_MIN) temp = HAIER_TEMP_MIN;
    if(temp > HAIER_TEMP_MAX) temp = HAIER_TEMP_MAX;

    uint32_t tb = req->toggle_bits;

    memset(st, 0, STATE_LEN);
    st[0] = HAIER_MODEL_A;
    st[1] = (uint8_t)((swing_v & 0x0F) | (((temp - HAIER_TEMP_MIN) & 0x0F) << 4));
    st[2] = (uint8_t)((swing_h & 0x07) << 5);

    st[3] = 0;
    if((tb >> HaierToggleHealth) & 1) st[3] |= 1 << 1;

    st[4] = power ? (1 << 6) : 0;
    st[5] = (uint8_t)((FAN_CODES[fan] & 0x07) << 5);

    st[6] = 0;
    if((tb >> HaierToggleTurbo) & 1) st[6] |= 1 << 6;
    if((tb >> HaierToggleQuiet) & 1) st[6] |= 1 << 7;

    st[7] = (uint8_t)((MODE_CODES[mode] & 0x07) << 5);

    st[8] = 0;
    if((tb >> HaierToggleSleep) & 1) st[8] |= 1 << 7;

    st[12] = (uint8_t)(button & 0x1F);

    uint8_t sum = 0;
    for(uint8_t i = 0; i < STATE_LEN - 1; i++) {
        sum += st[i];
    }
    st[13] = sum;
}

static uint8_t swing_v_for(const HaierRequest* req) {
    return ((req->toggle_bits >> HaierToggleSwing) & 1) ? H_SWINGV_AUTO : H_SWINGV_OFF;
}

static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, HAIER_IR_MAX_TIMINGS);
    // Haier prefixes the header with an extra equal-length mark and space
    ir_item(&b, LEAD_MARK, LEAD_SPACE);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_msb(&b, st, STATE_LEN, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool haier_ir_encode_state(const HaierRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == HaierModeOff || req->mode >= HaierModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, swing_v_for(req), H_SWINGH_AUTO, H_BTN_MODE, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool haier_ir_encode_toggle(
    const HaierRequest* req,
    HaierToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= HaierToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(
        req,
        toggle != HaierTogglePowerOff,
        swing_v_for(req),
        H_SWINGH_AUTO,
        TOGGLE_BUTTONS[toggle],
        st);
    return encode_state_bytes(st, timings, timings_count);
}

bool haier_ir_encode_extra(
    const HaierRequest* req,
    HaierExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= HaierExtraCount) return false;

    static const uint8_t VANE[4] = {H_SWINGV_TOP, H_SWINGV_MIDDLE, H_SWINGV_BOTTOM, H_SWINGV_DOWN};

    uint8_t st[STATE_LEN];
    if(extra <= HaierExtraVaneDown) {
        build_state(req, true, VANE[extra], H_SWINGH_AUTO, H_BTN_SWINGV, st);
    } else {
        uint8_t h = extra == HaierExtraSwingHAuto ? H_SWINGH_AUTO : H_SWINGH_MIDDLE;
        build_state(req, true, swing_v_for(req), h, H_BTN_SWINGH, st);
    }
    return encode_state_bytes(st, timings, timings_count);
}

void haier_ir_format_state(const HaierRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == HaierModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, swing_v_for(req), H_SWINGH_AUTO, H_BTN_MODE, st);
    snprintf(out, len, "%02X %02X %02X", st[1], st[5], st[7]);
}

void haier_ir_format_toggle(const HaierRequest* req, HaierToggle toggle, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(toggle >= HaierToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "btn %02X", TOGGLE_BUTTONS[toggle]);
}

void haier_ir_format_extra(const HaierRequest* req, HaierExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= HaierExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "%s", EXTRA_NAMES[extra]);
}

bool haier_ir_toggle_is_momentary(HaierToggle toggle) {
    (void)toggle;
    return false;
}

bool haier_ir_mode_locks_fan(HaierMode mode) {
    (void)mode;
    return false;
}

bool haier_ir_mode_has_no_temp(HaierMode mode) {
    return mode == HaierModeFan;
}

const char* haier_ir_get_mode_name(HaierMode mode) {
    return mode < HaierModeCount ? MODE_NAMES[mode] : "?";
}

const char* haier_ir_get_fan_name(HaierFan fan) {
    return fan < HaierFanCount ? FAN_NAMES[fan] : "?";
}

const char* haier_ir_get_toggle_name(HaierToggle toggle) {
    return toggle < HaierToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* haier_ir_get_extra_name(HaierExtra extra) {
    return extra < HaierExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t haier_ir_get_option_count(void) {
    return 0; // one variant only
}

const char* haier_ir_get_option_label(void) {
    return "Model";
}

const char* haier_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* haier_ir_get_protocol_name(void) {
    return "Haier";
}
