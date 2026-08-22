#include "midea_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>

// ==========================================================================
// Midea 48-bit protocol (Comfee, Danby, Kaysun, Keystone, Lennox, MrCool,
// Pioneer, Trotec and other Midea-family OEMs).
// Ported from IRremoteESP8266 src/ir_Midea.{h,cpp}, MideaProtocol union.
//
// Hybrid protocol: mode/fan/temperature is a full state frame, while the
// feature buttons are complete standalone words.
//
// Wire format: header, then 48 bits most significant byte first and MSB first
// within each byte, a closing mark and a gap. The whole thing is then sent a
// second time with every bit inverted.
//
// State layout, byte 0 being the least significant:
//   byte 0  checksum
//   byte 1  sensor temperature / on-timer   (0xFF when unused)
//   byte 2  off-timer                       (0xFF when unused)
//   byte 3  temperature - 17, bit 5 = Fahrenheit
//   byte 4  mode:3 fan:2 -:1 sleep:1 power:1
//   byte 5  type:3 header:5 (0b10100)
// ==========================================================================

// Line coding (microseconds); every value is a multiple of the 80us tick
#define HDR_MARK   4480
#define HDR_SPACE  4480
#define BIT_MARK   560
#define ONE_SPACE  1680
#define ZERO_SPACE 560
#define MIN_GAP    5600

#define MIDEA_BITS 48

/// IRMideaAC::stateReset(), which is a Fahrenheit frame; we rebuild in Celsius
#define MIDEA_RESET_STATE 0xA1826FFFFFULL

#define MIDEA_HEADER 0b10100
#define MIDEA_TYPE_COMMAND 0b001

// Mode field
#define M_COOL 0
#define M_DRY  1
#define M_AUTO 2
#define M_HEAT 3
#define M_FAN  4

// Fan field
#define M_FAN_AUTO 0
#define M_FAN_LOW  1
#define M_FAN_MED  2
#define M_FAN_HIGH 3

// Complete standalone command words
#define MIDEA_SWINGV_TOGGLE 0xA201FFFFFF7CULL
#define MIDEA_ECONO_TOGGLE  0xA202FFFFFF7EULL
#define MIDEA_LIGHT_TOGGLE  0xA208FFFFFF75ULL
#define MIDEA_TURBO_TOGGLE  0xA209FFFFFF74ULL
#define MIDEA_SELF_CLEAN    0xA20DFFFFFF70ULL
#define MIDEA_8C_HEAT       0xA20FFFFFFF73ULL
#define MIDEA_QUIET_ON      0xA212FFFFFF6EULL
#define MIDEA_QUIET_OFF     0xA213FFFFFF6FULL

static const uint8_t MODE_CODES[MideaModeCount] = {
    M_COOL, // Off - unused, the power bit carries it
    M_COOL,
    M_AUTO,
    M_DRY,
    M_HEAT,
    M_FAN,
};

static const uint8_t FAN_CODES[MideaFanCount] = {
    M_FAN_AUTO,
    M_FAN_LOW,
    M_FAN_MED,
    M_FAN_HIGH,
};

static const uint64_t EXTRA_CODES[MideaExtraCount] = {
    MIDEA_QUIET_ON,
    MIDEA_QUIET_OFF,
    MIDEA_SELF_CLEAN,
    MIDEA_8C_HEAT,
};

static const char* MODE_NAMES[MideaModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[MideaFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[MideaToggleCount] = {"Power", "Swing", "Turbo", "Light", "Econo"};
static const char* EXTRA_NAMES[MideaExtraCount] = {
    "Quiet on", "Quiet off", "Self clean", "8C heat"};
static const char* MODEL_NAMES[MideaModelCount] = {"Standard", "Kaysun"};

/// Kaysun swaps which word means vertical swing.
static uint64_t swing_code(const MideaRequest* req) {
    return (req->option == MideaModelKaysun) ? MIDEA_ECONO_TOGGLE : MIDEA_SWINGV_TOGGLE;
}

static uint8_t reverse8(uint8_t v) {
    v = (uint8_t)(((v & 0xF0) >> 4) | ((v & 0x0F) << 4));
    v = (uint8_t)(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
    v = (uint8_t)(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
    return v;
}

/// Sum the bit-reversed bytes 1..5, negate, then bit-reverse the result.
static uint8_t midea_checksum(uint64_t state) {
    uint8_t sum = 0;
    uint64_t tmp = state;
    for(uint8_t i = 0; i < 5; i++) {
        tmp >>= 8;
        sum = (uint8_t)(sum + reverse8((uint8_t)(tmp & 0xFF)));
    }
    sum = (uint8_t)(256 - sum);
    return reverse8(sum);
}

static uint64_t build_raw(const MideaRequest* req, bool power) {
    MideaMode mode = req->mode;
    if(mode == MideaModeOff || mode >= MideaModeCount) mode = MideaModeCool;

    MideaFan fan = req->fan >= MideaFanCount ? MideaFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < MIDEA_TEMP_MIN) temp = MIDEA_TEMP_MIN;
    if(temp > MIDEA_TEMP_MAX) temp = MIDEA_TEMP_MAX;

    // Bytes 1 and 2 stay at their "unused" 0xFF, matching a plain command
    // frame with no sensor temperature and no timers.
    uint64_t raw = 0;
    raw |= (uint64_t)(((MIDEA_HEADER << 3) | MIDEA_TYPE_COMMAND) & 0xFF) << 40;

    uint8_t byte4 = (uint8_t)(MODE_CODES[mode] & 0x07);
    byte4 |= (uint8_t)((FAN_CODES[fan] & 0x03) << 3);
    if(power) byte4 |= 0x80;
    raw |= (uint64_t)byte4 << 32;

    // Celsius: bit 5 (Fahrenheit) stays clear
    raw |= (uint64_t)((temp - MIDEA_TEMP_MIN) & 0x1F) << 24;

    raw |= (uint64_t)0xFF << 16; // byte 2, no off-timer
    raw |= (uint64_t)0xFF << 8; // byte 1, no sensor temperature
    raw |= midea_checksum(raw);
    return raw;
}

/// One phase: header, 48 bits MSB-first, closing mark, gap.
static void encode_phase(IrBuild* b, uint64_t data, bool with_gap) {
    ir_item(b, HDR_MARK, HDR_SPACE);
    for(uint8_t i = MIDEA_BITS; i-- > 0;) {
        ir_bit(b, (data >> i) & 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    }
    if(with_gap) {
        ir_item(b, BIT_MARK, MIN_GAP);
    }
}

/// The message, then the same message with every bit inverted.
static bool encode_raw(uint64_t raw, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, MIDEA_IR_MAX_TIMINGS);
    encode_phase(&b, raw, true);
    encode_phase(&b, (~raw) & 0xFFFFFFFFFFFFULL, false);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool midea_ir_encode_state(const MideaRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == MideaModeOff || req->mode >= MideaModeCount) return false;
    return encode_raw(build_raw(req, true), timings, timings_count);
}

bool midea_ir_encode_toggle(
    const MideaRequest* req,
    MideaToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= MideaToggleCount) return false;

    // Power is a state frame; the feature buttons are standalone words.
    switch(toggle) {
    case MideaTogglePowerOff:
        return encode_raw(build_raw(req, false), timings, timings_count);
    case MideaToggleSwing:
        return encode_raw(swing_code(req), timings, timings_count);
    case MideaToggleTurbo:
        return encode_raw(MIDEA_TURBO_TOGGLE, timings, timings_count);
    case MideaToggleLight:
        return encode_raw(MIDEA_LIGHT_TOGGLE, timings, timings_count);
    case MideaToggleEcono:
        return encode_raw(MIDEA_ECONO_TOGGLE, timings, timings_count);
    default:
        return false;
    }
}

bool midea_ir_encode_extra(
    const MideaRequest* req,
    MideaExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    (void)req;
    if(!timings || !timings_count || extra >= MideaExtraCount) return false;
    return encode_raw(EXTRA_CODES[extra], timings, timings_count);
}

void midea_ir_format_state(const MideaRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == MideaModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint64_t raw = build_raw(req, true);
    // Bytes 3 and 4 hold everything the user set
    snprintf(out, len, "%04lX", (unsigned long)((raw >> 24) & 0xFFFF));
}

void midea_ir_format_toggle(const MideaRequest* req, MideaToggle toggle, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= MideaToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    if(toggle == MideaTogglePowerOff) {
        snprintf(out, len, "%04lX", (unsigned long)((build_raw(req, false) >> 24) & 0xFFFF));
        return;
    }
    const uint64_t codes[MideaToggleCount] = {
        0, swing_code(req), MIDEA_TURBO_TOGGLE, MIDEA_LIGHT_TOGGLE, MIDEA_ECONO_TOGGLE};
    snprintf(out, len, "%03lX..", (unsigned long)(codes[toggle] >> 36));
}

void midea_ir_format_extra(const MideaRequest* req, MideaExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= MideaExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "%03lX..", (unsigned long)(EXTRA_CODES[extra] >> 36));
}

bool midea_ir_toggle_is_momentary(MideaToggle toggle) {
    (void)toggle;
    return false;
}

bool midea_ir_mode_locks_fan(MideaMode mode) {
    // The protocol accepts any fan speed in any mode
    (void)mode;
    return false;
}

bool midea_ir_mode_has_no_temp(MideaMode mode) {
    return mode == MideaModeFan;
}

const char* midea_ir_get_mode_name(MideaMode mode) {
    return mode < MideaModeCount ? MODE_NAMES[mode] : "?";
}

const char* midea_ir_get_fan_name(MideaFan fan) {
    return fan < MideaFanCount ? FAN_NAMES[fan] : "?";
}

const char* midea_ir_get_toggle_name(MideaToggle toggle) {
    return toggle < MideaToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* midea_ir_get_extra_name(MideaExtra extra) {
    return extra < MideaExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t midea_ir_get_option_count(void) {
    return MideaModelCount;
}

const char* midea_ir_get_option_label(void) {
    return "Model";
}

const char* midea_ir_get_option_name(uint8_t option) {
    return option < MideaModelCount ? MODEL_NAMES[option] : "?";
}

const char* midea_ir_get_protocol_name(void) {
    return "Midea";
}
