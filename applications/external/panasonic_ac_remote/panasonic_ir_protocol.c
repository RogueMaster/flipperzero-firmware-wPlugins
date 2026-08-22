#include "panasonic_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Panasonic A/C, 27-byte frame.
// Ported from IRremoteESP8266 src/ir_Panasonic.{h,cpp}. This protocol has no
// bitfield union in the library; the fields are byte offsets, transcribed here
// from the setters.
//
//   byte 13  power:1 (b0)   mode:4 (high nibble)
//   byte 14  temperature in whole degrees, bits 1-5
//   byte 16  swingV (low nibble)   fan + 3 (high nibble)
//   byte 21  quiet:1 (b0)   powerful:1 (b5)
//   byte 26  sum of bytes 0..25
//
// Everything else is carried over from the library's known-good state so the
// model and header bytes stay exactly as a real handset sends them.
//
// The frame goes out as TWO sections, each with its own header: 8 bytes, a
// gap, then the remaining 19. Least significant bit first.
// ==========================================================================

#define STATE_LEN 27
#define SECTION1  8

// Line coding (microseconds)
#define HDR_MARK    3456
#define HDR_SPACE   1728
#define BIT_MARK    432
#define ONE_SPACE   1296
#define ZERO_SPACE  432
#define SECTION_GAP 10000

// Mode field (byte 13, high nibble)
#define P_AUTO 0
#define P_DRY  2
#define P_COOL 3
#define P_HEAT 4
#define P_FAN  6

// Fan field (byte 16, high nibble) is the speed plus this delta
#define P_FAN_DELTA 3
#define P_FAN_LOW   1
#define P_FAN_MED   2
#define P_FAN_HIGH  3
#define P_FAN_AUTO  7

// Vertical vane (byte 16, low nibble)
#define P_SWINGV_HIGHEST 0x1
#define P_SWINGV_HIGH    0x2
#define P_SWINGV_MIDDLE  0x3
#define P_SWINGV_LOW     0x4
#define P_SWINGV_LOWEST  0x5
#define P_SWINGV_AUTO    0xF

/// Fan-only mode is documented as always reporting 27C
#define P_FAN_MODE_TEMP 27

/// IRPanasonicAc::stateReset()'s kPanasonicKnownGoodState
static const uint8_t KNOWN_GOOD[STATE_LEN] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00,
                                              0x06, 0x02, 0x20, 0xE0, 0x04, 0x00, 0x00,
                                              0x00, 0x80, 0x00, 0x00, 0x00, 0x0E, 0xE0,
                                              0x00, 0x00, 0x81, 0x00, 0x00, 0x00};

static const uint8_t MODE_CODES[PanasonicModeCount] = {
    P_COOL, // Off - unused, the power bit carries it
    P_COOL,
    P_AUTO,
    P_DRY,
    P_HEAT,
    P_FAN,
};

static const uint8_t FAN_CODES[PanasonicFanCount] = {
    P_FAN_AUTO,
    P_FAN_LOW,
    P_FAN_MED,
    P_FAN_HIGH,
};

static const uint8_t VANE_CODES[PanasonicExtraCount] = {
    P_SWINGV_HIGHEST,
    P_SWINGV_HIGH,
    P_SWINGV_MIDDLE,
    P_SWINGV_LOW,
    P_SWINGV_LOWEST,
};

static const char* MODE_NAMES[PanasonicModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[PanasonicFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[PanasonicToggleCount] = {"Power", "Swing", "Quiet", "Turbo"};
static const char* EXTRA_NAMES[PanasonicExtraCount] =
    {"Vane top", "Vane high", "Vane mid", "Vane low", "Vane bot"};
static const char* MODEL_NAMES[PanasonicModelCount] = {"JKE", "LKE", "NKE", "DKE", "CKP", "RKR"};

/// IRPanasonicAc::setModel() clears these, then stamps per-model values.
static void apply_model(uint8_t model, uint8_t* st) {
    st[13] &= 0xF0;
    st[17] = 0x00;
    st[21] &= 0xEF;
    st[23] = 0x81;
    st[25] = 0x00;

    switch(model) {
    case PanasonicModelLke:
        st[13] |= 0x02;
        st[17] = 0x06;
        break;
    case PanasonicModelNke:
        st[17] = 0x06;
        break;
    case PanasonicModelDke:
        st[23] = 0x01;
        st[25] = 0x06;
        break;
    case PanasonicModelCkp:
        st[21] |= 0x10;
        st[23] = 0x01;
        break;
    case PanasonicModelRkr:
        st[13] |= 0x08;
        st[23] = 0x89;
        break;
    case PanasonicModelJke:
    default:
        break;
    }
}

/// CKP and RKR swap the two bits over.
static bool model_swaps_quiet_powerful(uint8_t model) {
    return model == PanasonicModelCkp || model == PanasonicModelRkr;
}

static void build_state(const PanasonicRequest* req, bool power, uint8_t swing_v, uint8_t* st) {
    PanasonicMode mode = req->mode;
    if(mode == PanasonicModeOff || mode >= PanasonicModeCount) mode = PanasonicModeCool;

    PanasonicFan fan = req->fan >= PanasonicFanCount ? PanasonicFanAuto : req->fan;

    uint8_t temp = (mode == PanasonicModeFan) ? P_FAN_MODE_TEMP : req->temp;
    if(temp < PANASONIC_TEMP_MIN) temp = PANASONIC_TEMP_MIN;
    if(temp > PANASONIC_TEMP_MAX) temp = PANASONIC_TEMP_MAX;

    uint32_t tb = req->toggle_bits;

    memcpy(st, KNOWN_GOOD, STATE_LEN);
    uint8_t model = req->option < PanasonicModelCount ? req->option : PanasonicModelJke;
    apply_model(model, st);

    st[13] = (uint8_t)((st[13] & 0x0F) & ~1u);
    st[13] = (uint8_t)((st[13] & 0x0F) | ((MODE_CODES[mode] & 0x0F) << 4));
    if(power) st[13] |= 1;

    st[14] = (uint8_t)((st[14] & ~(0x1F << 1)) | ((temp & 0x1F) << 1));

    st[16] = (uint8_t)((swing_v & 0x0F) | (((FAN_CODES[fan] + P_FAN_DELTA) & 0x0F) << 4));

    // Keep whatever apply_model() put in byte 21, then set the feature bits.
    uint8_t quiet_bit = model_swaps_quiet_powerful(model) ? 5 : 0;
    uint8_t powerful_bit = model_swaps_quiet_powerful(model) ? 0 : 5;
    st[21] &= (uint8_t) ~((1u << 0) | (1u << 5));
    // The handset treats these as mutually exclusive; Powerful wins.
    if((tb >> PanasonicTogglePowerful) & 1) {
        st[21] |= (uint8_t)(1u << powerful_bit);
    } else if((tb >> PanasonicToggleQuiet) & 1) {
        st[21] |= (uint8_t)(1u << quiet_bit);
    }

    uint8_t sum = 0;
    for(uint8_t i = 0; i < STATE_LEN - 1; i++) {
        sum += st[i];
    }
    st[26] = sum;
}

static uint8_t swing_v_for(const PanasonicRequest* req) {
    return ((req->toggle_bits >> PanasonicToggleSwing) & 1) ? P_SWINGV_AUTO : P_SWINGV_MIDDLE;
}

/// Two sections, each with its own header, separated by a gap.
static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, PANASONIC_IR_MAX_TIMINGS);

    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, SECTION1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    ir_item(&b, BIT_MARK, SECTION_GAP);

    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st + SECTION1, STATE_LEN - SECTION1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool panasonic_ir_encode_state(
    const PanasonicRequest* req,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == PanasonicModeOff || req->mode >= PanasonicModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, swing_v_for(req), st);
    return encode_state_bytes(st, timings, timings_count);
}

bool panasonic_ir_encode_toggle(
    const PanasonicRequest* req,
    PanasonicToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= PanasonicToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, toggle != PanasonicTogglePowerOff, swing_v_for(req), st);
    return encode_state_bytes(st, timings, timings_count);
}

bool panasonic_ir_encode_extra(
    const PanasonicRequest* req,
    PanasonicExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= PanasonicExtraCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, VANE_CODES[extra], st);
    return encode_state_bytes(st, timings, timings_count);
}

void panasonic_ir_format_state(const PanasonicRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == PanasonicModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, swing_v_for(req), st);
    snprintf(out, len, "%02X %02X %02X", st[13], st[14], st[16]);
}

void panasonic_ir_format_toggle(
    const PanasonicRequest* req,
    PanasonicToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= PanasonicToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, toggle != PanasonicTogglePowerOff, swing_v_for(req), st);
    snprintf(out, len, "%02X %02X %02X", st[13], st[16], st[21]);
}

void panasonic_ir_format_extra(
    const PanasonicRequest* req,
    PanasonicExtra extra,
    char* out,
    size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= PanasonicExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "vane %u", (unsigned)VANE_CODES[extra]);
}

bool panasonic_ir_toggle_is_momentary(PanasonicToggle toggle) {
    (void)toggle;
    return false;
}

bool panasonic_ir_mode_locks_fan(PanasonicMode mode) {
    (void)mode;
    return false;
}

bool panasonic_ir_mode_has_no_temp(PanasonicMode mode) {
    // Fan-only always reports 27C, so there is nothing for the user to set
    return mode == PanasonicModeFan;
}

const char* panasonic_ir_get_mode_name(PanasonicMode mode) {
    return mode < PanasonicModeCount ? MODE_NAMES[mode] : "?";
}

const char* panasonic_ir_get_fan_name(PanasonicFan fan) {
    return fan < PanasonicFanCount ? FAN_NAMES[fan] : "?";
}

const char* panasonic_ir_get_toggle_name(PanasonicToggle toggle) {
    return toggle < PanasonicToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* panasonic_ir_get_extra_name(PanasonicExtra extra) {
    return extra < PanasonicExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t panasonic_ir_get_option_count(void) {
    return PanasonicModelCount;
}

const char* panasonic_ir_get_option_label(void) {
    return "Model";
}

const char* panasonic_ir_get_option_name(uint8_t option) {
    return option < PanasonicModelCount ? MODEL_NAMES[option] : "?";
}

const char* panasonic_ir_get_protocol_name(void) {
    return "Panasonic";
}
