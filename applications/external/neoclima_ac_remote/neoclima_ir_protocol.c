#include "neoclima_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Neoclima / Soleus Air, 12-byte frame.
// Ported from IRremoteESP8266 src/ir_Neoclima.{h,cpp}, NeoclimaProtocol union.
//
//   byte 1   cHeat:1(b1) ion:1(b2)
//   byte 3   light:1(b0) hold:1(b2) turbo:1(b3) econo:1(b4) eye:1(b6)
//   byte 5   button:5  fresh:1(b7)
//   byte 7   sleep:1 power:1 swingV:2(b2-3) swingH:1(b4) fan:2(b5-6) useFah:1
//   byte 8   follow-me
//   byte 9   temp:5 (degrees - 16)   mode:3 (b5-7)
//   byte 10  fixed 0xA5
//   byte 11  sum of bytes 0..10
//
// Byte 5's "button" field names the key the handset believes was pressed, so
// each command sets it to match rather than leaving it at zero.
//
// Sent least significant bit first, with a footer mark, a header-length space
// and one extra mark.
// ==========================================================================

#define STATE_LEN 12

// Line coding (microseconds)
#define HDR_MARK   6112
#define HDR_SPACE  7391
#define BIT_MARK   537
#define ONE_SPACE  1651
#define ZERO_SPACE 571

// Mode field (byte 9, bits 5-7)
#define N_AUTO 0b000
#define N_COOL 0b001
#define N_DRY  0b010
#define N_FAN  0b011
#define N_HEAT 0b100

// Fan field (byte 7, bits 5-6)
#define N_FAN_AUTO 0b00
#define N_FAN_HIGH 0b01
#define N_FAN_MED  0b10
#define N_FAN_LOW  0b11

// Vertical swing field (byte 7, bits 2-3)
#define N_SWINGV_ON  0b01
#define N_SWINGV_OFF 0b10

// Button codes (byte 5, bits 0-4)
#define N_BTN_POWER   0x00
#define N_BTN_MODE    0x01
#define N_BTN_SWING   0x04
#define N_BTN_AIRFLOW 0x07
#define N_BTN_HOLD    0x08
#define N_BTN_TURBO   0x0A
#define N_BTN_LIGHT   0x0B
#define N_BTN_ECONO   0x0D
#define N_BTN_EYE     0x0E
#define N_BTN_ION     0x14
#define N_BTN_FRESH   0x15
#define N_BTN_8CHEAT  0x1D

static const uint8_t MODE_CODES[NeoclimaModeCount] = {
    N_COOL, // Off - unused, the power bit carries it
    N_COOL,
    N_AUTO,
    N_DRY,
    N_HEAT,
    N_FAN,
};

static const uint8_t FAN_CODES[NeoclimaFanCount] = {
    N_FAN_AUTO,
    N_FAN_LOW,
    N_FAN_MED,
    N_FAN_HIGH,
};

static const uint8_t TOGGLE_BUTTONS[NeoclimaToggleCount] = {
    N_BTN_POWER,
    N_BTN_SWING,
    N_BTN_TURBO,
    N_BTN_LIGHT,
    N_BTN_ECONO,
    N_BTN_ION,
};

static const uint8_t EXTRA_BUTTONS[NeoclimaExtraCount] = {
    N_BTN_AIRFLOW,
    N_BTN_FRESH,
    N_BTN_EYE,
    N_BTN_HOLD,
    N_BTN_8CHEAT,
};

static const char* MODE_NAMES[NeoclimaModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[NeoclimaFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[NeoclimaToggleCount] =
    {"Power", "Swing", "Turbo", "Light", "Econo", "Ion"};
static const char* EXTRA_NAMES[NeoclimaExtraCount] = {"Swing H", "Fresh", "Eye", "Hold", "8C heat"};

/// Extra one-shot flags that are bits rather than buttons
typedef struct {
    bool swing_h;
    bool fresh;
    bool eye;
    bool hold;
    bool cheat;
} ExtraFlags;

static void
    build_state(const NeoclimaRequest* req, bool power, uint8_t button, ExtraFlags ef, uint8_t* st) {
    NeoclimaMode mode = req->mode;
    if(mode == NeoclimaModeOff || mode >= NeoclimaModeCount) mode = NeoclimaModeCool;

    NeoclimaFan fan = req->fan >= NeoclimaFanCount ? NeoclimaFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < NEOCLIMA_TEMP_MIN) temp = NEOCLIMA_TEMP_MIN;
    if(temp > NEOCLIMA_TEMP_MAX) temp = NEOCLIMA_TEMP_MAX;

    uint32_t tb = req->toggle_bits;

    memset(st, 0, STATE_LEN);

    if(ef.cheat) st[1] |= 1 << 1;
    if((tb >> NeoclimaToggleIon) & 1) st[1] |= 1 << 2;

    if((tb >> NeoclimaToggleLight) & 1) st[3] |= 1 << 0;
    if(ef.hold) st[3] |= 1 << 2;
    if((tb >> NeoclimaToggleTurbo) & 1) st[3] |= 1 << 3;
    if((tb >> NeoclimaToggleEcono) & 1) st[3] |= 1 << 4;
    if(ef.eye) st[3] |= 1 << 6;

    st[5] = (uint8_t)(button & 0x1F);
    if(ef.fresh) st[5] |= 1 << 7;

    st[7] = 0;
    if(power) st[7] |= 1 << 1;
    st[7] |= (uint8_t)((((tb >> NeoclimaToggleSwing) & 1) ? N_SWINGV_ON : N_SWINGV_OFF) << 2);
    if(ef.swing_h) st[7] |= 1 << 4;
    st[7] |= (uint8_t)((FAN_CODES[fan] & 0x03) << 5);

    st[9] = (uint8_t)(((temp - NEOCLIMA_TEMP_MIN) & 0x1F) | ((MODE_CODES[mode] & 0x07) << 5));

    st[10] = 0xA5;

    uint8_t sum = 0;
    for(uint8_t i = 0; i < STATE_LEN - 1; i++) {
        sum += st[i];
    }
    st[11] = sum;
}

static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, NEOCLIMA_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, STATE_LEN, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    // Footer mark plus a header-length space, then one extra mark
    ir_item(&b, BIT_MARK, HDR_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

static const ExtraFlags NO_EXTRAS = {false, false, false, false, false};

bool neoclima_ir_encode_state(const NeoclimaRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == NeoclimaModeOff || req->mode >= NeoclimaModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, N_BTN_MODE, NO_EXTRAS, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool neoclima_ir_encode_toggle(
    const NeoclimaRequest* req,
    NeoclimaToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= NeoclimaToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, toggle != NeoclimaTogglePowerOff, TOGGLE_BUTTONS[toggle], NO_EXTRAS, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool neoclima_ir_encode_extra(
    const NeoclimaRequest* req,
    NeoclimaExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= NeoclimaExtraCount) return false;

    ExtraFlags ef = NO_EXTRAS;
    switch(extra) {
    case NeoclimaExtraSwingH:
        ef.swing_h = true;
        break;
    case NeoclimaExtraFresh:
        ef.fresh = true;
        break;
    case NeoclimaExtraEye:
        ef.eye = true;
        break;
    case NeoclimaExtraHold:
        ef.hold = true;
        break;
    case NeoclimaExtra8CHeat:
        ef.cheat = true;
        break;
    default:
        break;
    }

    uint8_t st[STATE_LEN];
    build_state(req, true, EXTRA_BUTTONS[extra], ef, st);
    return encode_state_bytes(st, timings, timings_count);
}

void neoclima_ir_format_state(const NeoclimaRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == NeoclimaModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, N_BTN_MODE, NO_EXTRAS, st);
    snprintf(out, len, "%02X %02X %02X", st[7], st[9], st[11]);
}

void neoclima_ir_format_toggle(
    const NeoclimaRequest* req,
    NeoclimaToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= NeoclimaToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "btn %02X", TOGGLE_BUTTONS[toggle]);
}

void neoclima_ir_format_extra(
    const NeoclimaRequest* req,
    NeoclimaExtra extra,
    char* out,
    size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= NeoclimaExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "btn %02X", EXTRA_BUTTONS[extra]);
}

bool neoclima_ir_toggle_is_momentary(NeoclimaToggle toggle) {
    (void)toggle;
    return false;
}

bool neoclima_ir_mode_locks_fan(NeoclimaMode mode) {
    (void)mode;
    return false;
}

bool neoclima_ir_mode_has_no_temp(NeoclimaMode mode) {
    return mode == NeoclimaModeFan;
}

const char* neoclima_ir_get_mode_name(NeoclimaMode mode) {
    return mode < NeoclimaModeCount ? MODE_NAMES[mode] : "?";
}

const char* neoclima_ir_get_fan_name(NeoclimaFan fan) {
    return fan < NeoclimaFanCount ? FAN_NAMES[fan] : "?";
}

const char* neoclima_ir_get_toggle_name(NeoclimaToggle toggle) {
    return toggle < NeoclimaToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* neoclima_ir_get_extra_name(NeoclimaExtra extra) {
    return extra < NeoclimaExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t neoclima_ir_get_option_count(void) {
    return 0; // one variant only
}

const char* neoclima_ir_get_option_label(void) {
    return "Model";
}

const char* neoclima_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* neoclima_ir_get_protocol_name(void) {
    return "Neoclima";
}
