#include "toshiba_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Toshiba A/C, 9-byte (72-bit) message.
// Ported from IRremoteESP8266 src/ir_Toshiba.{h,cpp}, ToshibaProtocol union.
//
//   byte 0  0xF2                 byte 1  0x0D  (byte 0 inverted)
//   byte 2  length:4 model:4     byte 3  byte 2 inverted
//   byte 4  0x01
//   byte 5  swing:3  temp-17:4 (bits 4-7)
//   byte 6  mode:3   fan:3 (bits 5-7)
//   byte 7  filter:1 (bit 4)
//   byte 8  XOR of bytes 0..7
//
// Power off is not a bit: it is mode value 7. The whole message is sent
// twice, separated by a gap.
// ==========================================================================

#define STATE_LEN 9

// Line coding (microseconds)
#define HDR_MARK   4400
#define HDR_SPACE  4300
#define BIT_MARK   580
#define ONE_SPACE  1600
#define ZERO_SPACE 490
#define USUAL_GAP  7400

// Mode field (byte 6, bits 0-2)
#define T_AUTO 0
#define T_COOL 1
#define T_DRY  2
#define T_HEAT 3
#define T_FAN  4
#define T_OFF  7

// Fan field (byte 6, bits 5-7). The unit has five speeds; we expose four.
#define T_FAN_AUTO 0
#define T_FAN_MIN  1
#define T_FAN_MED  3
#define T_FAN_MAX  5

// Swing field (byte 5, bits 0-2)
#define T_SWING_STEP   0
#define T_SWING_ON     1
#define T_SWING_OFF    2
#define T_SWING_TOGGLE 4

// byte 2 low nibble: payload length 3 past byte 4. High nibble: model.
#define T_LENGTH 0x03

static const uint8_t MODE_CODES[ToshibaModeCount] = {
    T_OFF,
    T_COOL,
    T_AUTO,
    T_DRY,
    T_HEAT,
    T_FAN,
};

static const uint8_t FAN_CODES[ToshibaFanCount] = {
    T_FAN_AUTO,
    T_FAN_MIN,
    T_FAN_MED,
    T_FAN_MAX,
};

static const uint8_t EXTRA_SWING[ToshibaExtraCount] = {T_SWING_STEP, T_SWING_TOGGLE};

static const char* MODE_NAMES[ToshibaModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[ToshibaFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[ToshibaToggleCount] = {"Power", "Swing", "Filter"};
static const char* EXTRA_NAMES[ToshibaExtraCount] = {"Vane step", "Swing tgl"};
// IRremoteESP8266 calls these Generic Remote A and B. A is the long-standing
// default; B is the later handset generation (WA-TH03A, WA-TH04A and kin).
static const char* MODEL_NAMES[ToshibaModelCount] = {"Generic", "WA-TH0x"};

static uint8_t toshiba_checksum(const uint8_t* st) {
    uint8_t x = 0;
    for(uint8_t i = 0; i < STATE_LEN - 1; i++) {
        x ^= st[i];
    }
    return x;
}

static void build_state(const ToshibaRequest* req, bool power, uint8_t swing, uint8_t* st) {
    ToshibaMode mode = req->mode;
    if(mode >= ToshibaModeCount) mode = ToshibaModeCool;
    if(!power) mode = ToshibaModeOff;

    ToshibaFan fan = req->fan >= ToshibaFanCount ? ToshibaFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < TOSHIBA_TEMP_MIN) temp = TOSHIBA_TEMP_MIN;
    if(temp > TOSHIBA_TEMP_MAX) temp = TOSHIBA_TEMP_MAX;

    memset(st, 0, STATE_LEN);
    st[0] = 0xF2;
    st[1] = (uint8_t)~0xF2;
    uint8_t model = req->option < ToshibaModelCount ? req->option : ToshibaModelRemoteA;
    st[2] = (uint8_t)(T_LENGTH | (model << 4));
    st[3] = (uint8_t)~st[2];
    st[4] = 0x01;
    st[5] = (uint8_t)((swing & 0x07) | (((temp - TOSHIBA_TEMP_MIN) & 0x0F) << 4));
    st[6] = (uint8_t)((MODE_CODES[mode] & 0x07) | ((FAN_CODES[fan] & 0x07) << 5));
    st[7] = ((req->toggle_bits >> ToshibaToggleFilter) & 1) ? (1 << 4) : 0;
    st[8] = toshiba_checksum(st);
}

static uint8_t swing_for(const ToshibaRequest* req) {
    return ((req->toggle_bits >> ToshibaToggleSwing) & 1) ? T_SWING_ON : T_SWING_OFF;
}

/// The message twice, separated by a gap, ending on a mark.
static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, TOSHIBA_IR_MAX_TIMINGS);
    for(uint8_t pass = 0; pass < 2; pass++) {
        ir_item(&b, HDR_MARK, HDR_SPACE);
        ir_bytes_msb(&b, st, STATE_LEN, BIT_MARK, ONE_SPACE, ZERO_SPACE);
        if(pass == 0) ir_item(&b, BIT_MARK, USUAL_GAP);
    }
    return ir_build_finish(&b, BIT_MARK, count);
}

bool toshiba_ir_encode_state(const ToshibaRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == ToshibaModeOff || req->mode >= ToshibaModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, swing_for(req), st);
    return encode_state_bytes(st, timings, timings_count);
}

bool toshiba_ir_encode_toggle(
    const ToshibaRequest* req,
    ToshibaToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= ToshibaToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, toggle != ToshibaTogglePowerOff, swing_for(req), st);
    return encode_state_bytes(st, timings, timings_count);
}

bool toshiba_ir_encode_extra(
    const ToshibaRequest* req,
    ToshibaExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= ToshibaExtraCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, EXTRA_SWING[extra], st);
    return encode_state_bytes(st, timings, timings_count);
}

void toshiba_ir_format_state(const ToshibaRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == ToshibaModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, swing_for(req), st);
    snprintf(out, len, "%02X %02X %02X", st[5], st[6], st[8]);
}

void toshiba_ir_format_toggle(
    const ToshibaRequest* req,
    ToshibaToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= ToshibaToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, toggle != ToshibaTogglePowerOff, swing_for(req), st);
    snprintf(out, len, "%02X %02X %02X", st[5], st[6], st[8]);
}

void toshiba_ir_format_extra(const ToshibaRequest* req, ToshibaExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= ToshibaExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "swing %u", (unsigned)EXTRA_SWING[extra]);
}

bool toshiba_ir_toggle_is_momentary(ToshibaToggle toggle) {
    (void)toggle;
    return false;
}

bool toshiba_ir_mode_locks_fan(ToshibaMode mode) {
    (void)mode;
    return false;
}

bool toshiba_ir_mode_has_no_temp(ToshibaMode mode) {
    return mode == ToshibaModeFan;
}

const char* toshiba_ir_get_mode_name(ToshibaMode mode) {
    return mode < ToshibaModeCount ? MODE_NAMES[mode] : "?";
}

const char* toshiba_ir_get_fan_name(ToshibaFan fan) {
    return fan < ToshibaFanCount ? FAN_NAMES[fan] : "?";
}

const char* toshiba_ir_get_toggle_name(ToshibaToggle toggle) {
    return toggle < ToshibaToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* toshiba_ir_get_extra_name(ToshibaExtra extra) {
    return extra < ToshibaExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t toshiba_ir_get_option_count(void) {
    return ToshibaModelCount;
}

const char* toshiba_ir_get_option_label(void) {
    return "Remote";
}

const char* toshiba_ir_get_option_name(uint8_t option) {
    return option < ToshibaModelCount ? MODEL_NAMES[option] : "?";
}

const char* toshiba_ir_get_protocol_name(void) {
    return "Toshiba";
}
