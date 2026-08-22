#include "fujitsu_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Fujitsu / Fujitsu General / OGeneral A/C, model ARRAH2E.
// Ported from IRremoteESP8266 src/ir_Fujitsu.{h,cpp}, FujitsuProtocol union.
//
// Two frame shapes share one preamble:
//   long  (16 bytes) carries mode, fan, temperature and the feature bits
//   short (7 bytes)  carries a single command byte and its complement
//
// Long frame:
//   0-1  0x14 0x63      2  device id (0)      3-4  0x10 0x10
//   5    0xFE           6  rest length (9)    7    protocol (0x30)
//   8    power:1 fahrenheit:1 temp:6   -- temp is (degrees - 16) * 4
//   9    mode:3 clean:1 timerType:2
//   10   fan:3 swing:2 (bits 4-5)
//   11-13 timers        14  filter:1(b3) unknown:1(b5)=1 outsideQuiet:1(b7)
//   15   (0 - sum of bytes 8..14)
//
// Short frame: 0x14 0x63 0x00 0x10 0x10 cmd ~cmd
//
// Both are sent least significant bit first.
// ==========================================================================

// ARRAH2E-family lengths; ARDB1 and ARJW2 are one byte shorter in both forms
#define LONG_LEN  16
#define SHORT_LEN 7

/// ARDB1 / ARJW2 add this instead of negating the sum
#define ALT_CHECKSUM_COMPLEMENT 0x9B

// Line coding (microseconds)
#define HDR_MARK   3324
#define HDR_SPACE  1574
#define BIT_MARK   448
#define ONE_SPACE  1182
#define ZERO_SPACE 390

// Mode field (byte 9, bits 0-2)
#define F_AUTO 0x0
#define F_COOL 0x1
#define F_DRY  0x2
#define F_FAN  0x3
#define F_HEAT 0x4

// Fan field (byte 10, bits 0-2). Note the ordering: high is 1, low is 3.
#define F_FAN_AUTO 0x00
#define F_FAN_HIGH 0x01
#define F_FAN_MED  0x02
#define F_FAN_LOW  0x03

// Swing field (byte 10, bits 4-5)
#define F_SWING_OFF  0x00
#define F_SWING_VERT 0x01

// Short command bytes
#define F_CMD_TURN_OFF    0x02
#define F_CMD_ECONO       0x09
#define F_CMD_POWERFUL    0x39
#define F_CMD_STEP_VERT   0x6C
#define F_CMD_SWING_VERT  0x6D
#define F_CMD_STEP_HORIZ  0x79
#define F_CMD_SWING_HORIZ 0x7A

// Long-frame fixed bytes
#define F_LONG_CMD    0xFE
#define F_REST_LENGTH (LONG_LEN - 7)
#define F_PROTOCOL    0x30

static const uint8_t MODE_CODES[FujitsuModeCount] = {
    F_COOL, // Off - unused, power off is its own short frame
    F_COOL,
    F_AUTO,
    F_DRY,
    F_HEAT,
    F_FAN,
};

static const uint8_t FAN_CODES[FujitsuFanCount] = {
    F_FAN_AUTO,
    F_FAN_LOW,
    F_FAN_MED,
    F_FAN_HIGH,
};

static const uint8_t EXTRA_CMDS[FujitsuExtraCount] = {
    F_CMD_STEP_VERT,
    F_CMD_SWING_VERT,
    F_CMD_STEP_HORIZ,
    F_CMD_SWING_HORIZ,
};

static const char* MODE_NAMES[FujitsuModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[FujitsuFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[FujitsuToggleCount] =
    {"Power", "Swing", "Turbo", "Econo", "Filter", "Clean"};
static const char* EXTRA_NAMES[FujitsuExtraCount] =
    {"Step vert", "Swing V", "Step horiz", "Swing H"};

static const char* MODEL_NAMES[FujitsuModelCount] =
    {"ARRAH2E", "ARREB1E", "ARRY4", "ARREW4E", "ARDB1", "ARJW2"};

/// ARDB1 and ARJW2 use a shorter frame and a different checksum.
static bool is_short_family(FujitsuModel model) {
    return model == FujitsuModelARDB1 || model == FujitsuModelARJW2;
}

/// Only these carry the byte-14 marker bit and the short-frame complement.
static bool has_marker_bit(FujitsuModel model) {
    return model == FujitsuModelARRAH2E || model == FujitsuModelARREB1E ||
           model == FujitsuModelARRY4;
}

static FujitsuModel model_of(const FujitsuRequest* req) {
    return req->option < FujitsuModelCount ? (FujitsuModel)req->option : FujitsuModelARRAH2E;
}

static uint8_t long_len(FujitsuModel model) {
    return is_short_family(model) ? LONG_LEN - 1 : LONG_LEN;
}

static uint8_t short_len(FujitsuModel model) {
    return is_short_family(model) ? SHORT_LEN - 1 : SHORT_LEN;
}

static void build_long(const FujitsuRequest* req, uint8_t* st) {
    FujitsuModel model = model_of(req);
    uint8_t llen = long_len(model);
    uint8_t slen = short_len(model);
    FujitsuMode mode = req->mode;
    if(mode == FujitsuModeOff || mode >= FujitsuModeCount) mode = FujitsuModeCool;

    FujitsuFan fan = req->fan >= FujitsuFanCount ? FujitsuFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < FUJITSU_TEMP_MIN) temp = FUJITSU_TEMP_MIN;
    if(temp > FUJITSU_TEMP_MAX) temp = FUJITSU_TEMP_MAX;

    uint32_t tb = req->toggle_bits;

    memset(st, 0, LONG_LEN);
    st[0] = 0x14;
    st[1] = 0x63;
    st[2] = 0x00;
    st[3] = 0x10;
    st[4] = 0x10;
    st[5] = F_LONG_CMD;
    st[6] = (uint8_t)(llen - 7);
    st[7] = (model == FujitsuModelARREW4E) ? 0x31 : F_PROTOCOL;

    // Power on, Celsius. ARREW4E packs the setpoint in half-degree steps from
    // 8C; every other model uses quarter-degree steps from 16C.
    st[8] = 0x01;
    uint8_t temp_field = (model == FujitsuModelARREW4E) ?
                             (uint8_t)((temp - FUJITSU_TEMP_MIN / 2) * 2) :
                             (uint8_t)((temp - FUJITSU_TEMP_MIN) * 4);
    st[8] |= (uint8_t)((temp_field & 0x3F) << 2);

    st[9] = (uint8_t)(MODE_CODES[mode] & 0x07);
    if((tb >> FujitsuToggleClean) & 1) st[9] |= 1 << 3;

    st[10] = (uint8_t)(FAN_CODES[fan] & 0x07);
    // ARDB1 and ARJW2 have no swing field at all
    if(!is_short_family(model) && ((tb >> FujitsuToggleSwing) & 1)) {
        st[10] |= (uint8_t)(F_SWING_VERT << 4);
    }

    if(llen > 14) {
        if(has_marker_bit(model)) st[14] = 1 << 5;
        if((tb >> FujitsuToggleFilter) & 1) st[14] |= 1 << 3;
    }

    if(is_short_family(model)) {
        // sumBytes(longcode, len - 1), then 0x9B - sum
        uint8_t sum = 0;
        for(uint8_t i = 0; i < llen - 1; i++)
            sum += st[i];
        st[llen - 1] = (uint8_t)(ALT_CHECKSUM_COMPLEMENT - sum);
    } else {
        // sumBytes(longcode + short_len, len - short_len - 1), then negate.
        // The protocol byte at index 7 is inside that range.
        uint8_t sum = 0;
        for(uint8_t i = slen; i < llen - 1; i++)
            sum += st[i];
        st[llen - 1] = (uint8_t)(0 - sum);
    }
}

/// @return the number of bytes written
static uint8_t build_short(FujitsuModel model, uint8_t cmd, uint8_t* st) {
    uint8_t slen = short_len(model);
    st[0] = 0x14;
    st[1] = 0x63;
    st[2] = 0x00;
    st[3] = 0x10;
    st[4] = 0x10;
    st[5] = cmd;
    // Only the ARRAH2E family closes with the command's complement
    if(slen > 6) st[6] = (uint8_t)~cmd;
    return slen;
}

static bool encode_bytes(const uint8_t* st, size_t len, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, FUJITSU_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, len, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool fujitsu_ir_encode_state(const FujitsuRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == FujitsuModeOff || req->mode >= FujitsuModeCount) return false;

    uint8_t st[LONG_LEN];
    build_long(req, st);
    return encode_bytes(st, long_len(model_of(req)), timings, timings_count);
}

bool fujitsu_ir_encode_toggle(
    const FujitsuRequest* req,
    FujitsuToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= FujitsuToggleCount) return false;

    FujitsuModel model = model_of(req);
    uint8_t sh[SHORT_LEN];
    uint8_t cmd;
    switch(toggle) {
    case FujitsuTogglePowerOff:
        cmd = F_CMD_TURN_OFF;
        break;
    case FujitsuTogglePowerful:
        cmd = F_CMD_POWERFUL;
        break;
    case FujitsuToggleEcono:
        cmd = F_CMD_ECONO;
        break;
    default: {
        // Swing, Filter and Clean are bits inside the long frame
        uint8_t st[LONG_LEN];
        build_long(req, st);
        return encode_bytes(st, long_len(model), timings, timings_count);
    }
    }
    uint8_t n = build_short(model, cmd, sh);
    return encode_bytes(sh, n, timings, timings_count);
}

bool fujitsu_ir_encode_extra(
    const FujitsuRequest* req,
    FujitsuExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= FujitsuExtraCount) return false;

    uint8_t sh[SHORT_LEN];
    uint8_t n = build_short(model_of(req), EXTRA_CMDS[extra], sh);
    return encode_bytes(sh, n, timings, timings_count);
}

void fujitsu_ir_format_state(const FujitsuRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == FujitsuModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[LONG_LEN];
    build_long(req, st);
    snprintf(out, len, "%02X %02X %02X", st[8], st[9], st[10]);
}

void fujitsu_ir_format_toggle(
    const FujitsuRequest* req,
    FujitsuToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    switch(toggle) {
    case FujitsuTogglePowerOff:
        snprintf(out, len, "cmd %02X", F_CMD_TURN_OFF);
        return;
    case FujitsuTogglePowerful:
        snprintf(out, len, "cmd %02X", F_CMD_POWERFUL);
        return;
    case FujitsuToggleEcono:
        snprintf(out, len, "cmd %02X", F_CMD_ECONO);
        return;
    default:
        break;
    }
    if(toggle >= FujitsuToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[LONG_LEN];
    build_long(req, st);
    snprintf(out, len, "%02X %02X %02X", st[8], st[9], st[10]);
}

void fujitsu_ir_format_extra(const FujitsuRequest* req, FujitsuExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= FujitsuExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "cmd %02X", EXTRA_CMDS[extra]);
}

bool fujitsu_ir_toggle_is_momentary(FujitsuToggle toggle) {
    // These fire a one-shot command frame rather than setting a bit, so the
    // app has no on/off state to show for them.
    return toggle == FujitsuTogglePowerful || toggle == FujitsuToggleEcono;
}

bool fujitsu_ir_mode_locks_fan(FujitsuMode mode) {
    (void)mode;
    return false;
}

bool fujitsu_ir_mode_has_no_temp(FujitsuMode mode) {
    return mode == FujitsuModeFan;
}

const char* fujitsu_ir_get_mode_name(FujitsuMode mode) {
    return mode < FujitsuModeCount ? MODE_NAMES[mode] : "?";
}

const char* fujitsu_ir_get_fan_name(FujitsuFan fan) {
    return fan < FujitsuFanCount ? FAN_NAMES[fan] : "?";
}

const char* fujitsu_ir_get_toggle_name(FujitsuToggle toggle) {
    return toggle < FujitsuToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* fujitsu_ir_get_extra_name(FujitsuExtra extra) {
    return extra < FujitsuExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t fujitsu_ir_get_option_count(void) {
    return FujitsuModelCount;
}

const char* fujitsu_ir_get_option_label(void) {
    return "Model";
}

const char* fujitsu_ir_get_option_name(uint8_t option) {
    return option < FujitsuModelCount ? MODEL_NAMES[option] : "?";
}

const char* fujitsu_ir_get_protocol_name(void) {
    return "Fujitsu";
}
