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

/// Named after the handset, matching what the detector prints.
static const char* MODEL_NAMES[HaierModelCount] = {"YR-W02", "HSU07", "AC160", "AC176"};

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

// ==========================================================================
// The three other frame formats, selected on the Setup screen.
//
// All four share the same line coding - a 3000/3000 lead burst, a 3000/4300
// header, then bytes most significant bit first. Only the payload differs.
//
// AC160 and AC176 are the YR-W02 frame with a tail bolted on: bytes 0 to 13
// are laid out identically and carry the same checksum, then a second block
// starts at byte 14 with its own prefix and its own sum. HSU07-HEA03 is a
// different animal entirely - nine bytes, a command nibble, and the timer
// fields interleaved with everything else.
// ==========================================================================

#define H160_LEN    20
#define H176_LEN    22
#define H160_PREFIX 0xB5
#define H176_PREFIX 0xB7

// ---- HSU07-HEA03, 9 bytes ----------------------------------------------
#define HSU_LEN    9
#define HSU_PREFIX 0xA5

#define HSU_AUTO 0
#define HSU_COOL 1
#define HSU_DRY  2
#define HSU_HEAT 3
#define HSU_FAN  4

#define HSU_FAN_AUTO 0
#define HSU_FAN_LOW  1
#define HSU_FAN_MED  2
#define HSU_FAN_HIGH 3

#define HSU_SWINGV_OFF 0b00
#define HSU_SWINGV_CHG 0b11

#define HSU_CMD_OFF 0b0000
#define HSU_CMD_ON  0b0001

static const uint8_t MODE_CODES_HSU[HaierModeCount] = {
    HSU_AUTO, // Off - the command nibble carries it
    HSU_COOL,
    HSU_AUTO,
    HSU_DRY,
    HSU_HEAT,
    HSU_FAN,
};

static const uint8_t FAN_CODES_HSU[HaierFanCount] = {
    HSU_FAN_AUTO,
    HSU_FAN_LOW,
    HSU_FAN_MED,
    HSU_FAN_HIGH,
};

static void build_state_hsu(const HaierRequest* req, bool power, bool swing_v, uint8_t* st) {
    HaierMode mode = req->mode;
    if(mode == HaierModeOff || mode >= HaierModeCount) mode = HaierModeCool;

    HaierFan fan = req->fan >= HaierFanCount ? HaierFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < HAIER_TEMP_MIN) temp = HAIER_TEMP_MIN;
    if(temp > HAIER_TEMP_MAX) temp = HAIER_TEMP_MAX;

    memset(st, 0, HSU_LEN);
    st[0] = HSU_PREFIX;
    st[1] =
        (uint8_t)((power ? HSU_CMD_ON : HSU_CMD_OFF) | (((temp - HAIER_TEMP_MIN) & 0x0F) << 4));

    // Bit 5 of byte 2 is documented only as "value=1".
    st[2] = (uint8_t)((1 << 5) | ((swing_v ? HSU_SWINGV_CHG : HSU_SWINGV_OFF) << 6));

    // The reset frame leaves the off-timer at twelve hours.
    st[4] = 12;
    if((req->toggle_bits >> HaierToggleHealth) & 1) st[4] |= 1 << 5;

    st[5] = (uint8_t)((FAN_CODES_HSU[fan] & 0x03) << 6);
    st[6] = (uint8_t)((MODE_CODES_HSU[mode] & 0x07) << 5);
    if((req->toggle_bits >> HaierToggleSleep) & 1) st[7] |= 1 << 6;

    uint8_t sum = 0;
    for(uint8_t i = 0; i < HSU_LEN - 1; i++) {
        sum = (uint8_t)(sum + st[i]);
    }
    st[HSU_LEN - 1] = sum;
}

/// AC160 and AC176: the YR-W02 frame, then a second block with its own sum.
static void build_state_long(
    const HaierRequest* req,
    bool power,
    uint8_t swing_v,
    uint8_t swing_h,
    uint8_t button,
    uint8_t len,
    uint8_t* st) {
    memset(st, 0, len);
    build_state(req, power, swing_v, swing_h, button, st);

    HaierFan fan = req->fan >= HaierFanCount ? HaierFanAuto : req->fan;
    uint8_t fan_code = FAN_CODES[fan];

    st[14] = (len == H160_LEN) ? H160_PREFIX : H176_PREFIX;

    // Fan2 mirrors the main fan field but drops to zero for auto, and the two
    // formats put it in different bits of byte 16.
    uint8_t fan2 = (fan_code == H_FAN_AUTO) ? 0 : fan_code;
    if(len == H160_LEN) {
        st[16] = (uint8_t)((fan2 & 0x07) << 5);
    } else {
        st[16] = (uint8_t)((fan2 & 0x03) << 6);
    }

    uint8_t sum = 0;
    for(uint8_t i = STATE_LEN; i < len - 1; i++) {
        sum = (uint8_t)(sum + st[i]);
    }
    st[len - 1] = sum;
}

static uint8_t swing_v_for(const HaierRequest* req) {
    return ((req->toggle_bits >> HaierToggleSwing) & 1) ? H_SWINGV_AUTO : H_SWINGV_OFF;
}

static bool encode_bytes(const uint8_t* st, uint8_t len, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, HAIER_IR_MAX_TIMINGS);
    // Haier prefixes the header with an extra equal-length mark and space
    ir_item(&b, LEAD_MARK, LEAD_SPACE);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_msb(&b, st, len, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

/// Build and encode whichever format the Setup screen has selected.
static bool encode_for_model(
    const HaierRequest* req,
    bool power,
    uint8_t swing_v,
    uint8_t swing_h,
    uint8_t button,
    uint32_t* t,
    size_t* n) {
    uint8_t st[H176_LEN]; // the largest of the four

    switch(req->option) {
    case HaierModelHsu07:
        // Nine bytes with no swing-H field and only an on/off vane bit.
        build_state_hsu(req, power, swing_v != H_SWINGV_OFF, st);
        return encode_bytes(st, HSU_LEN, t, n);
    case HaierModel160:
        build_state_long(req, power, swing_v, swing_h, button, H160_LEN, st);
        return encode_bytes(st, H160_LEN, t, n);
    case HaierModel176:
        build_state_long(req, power, swing_v, swing_h, button, H176_LEN, st);
        return encode_bytes(st, H176_LEN, t, n);
    default:
        build_state(req, power, swing_v, swing_h, button, st);
        return encode_bytes(st, STATE_LEN, t, n);
    }
}

bool haier_ir_encode_state(const HaierRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == HaierModeOff || req->mode >= HaierModeCount) return false;

    if(req->option >= HaierModelCount) return false;

    return encode_for_model(
        req, true, swing_v_for(req), H_SWINGH_AUTO, H_BTN_MODE, timings, timings_count);
}

bool haier_ir_encode_toggle(
    const HaierRequest* req,
    HaierToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= HaierToggleCount) return false;

    if(req->option >= HaierModelCount) return false;

    return encode_for_model(
        req,
        toggle != HaierTogglePowerOff,
        swing_v_for(req),
        H_SWINGH_AUTO,
        TOGGLE_BUTTONS[toggle],
        timings,
        timings_count);
}

bool haier_ir_encode_extra(
    const HaierRequest* req,
    HaierExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= HaierExtraCount) return false;

    if(req->option >= HaierModelCount) return false;

    static const uint8_t VANE[4] = {H_SWINGV_TOP, H_SWINGV_MIDDLE, H_SWINGV_BOTTOM, H_SWINGV_DOWN};

    if(extra <= HaierExtraVaneDown) {
        return encode_for_model(
            req, true, VANE[extra], H_SWINGH_AUTO, H_BTN_SWINGV, timings, timings_count);
    }
    uint8_t h = extra == HaierExtraSwingHAuto ? H_SWINGH_AUTO : H_SWINGH_MIDDLE;
    return encode_for_model(req, true, swing_v_for(req), h, H_BTN_SWINGH, timings, timings_count);
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
    return HaierModelCount;
}

const char* haier_ir_get_option_label(void) {
    return "Model";
}

const char* haier_ir_get_option_name(uint8_t option) {
    return option < HaierModelCount ? MODEL_NAMES[option] : "?";
}

const char* haier_ir_get_protocol_name(void) {
    return "Haier";
}
