#include "daikin_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Daikin ARC433 (DaikinESP), 35-byte frame.
// Ported from IRremoteESP8266 src/ir_Daikin.{h,cpp}, DaikinESPProtocol union.
//
// The largest frame here, and the only one with THREE checksums - one per
// section, each covering just its own bytes:
//   byte 7   sum of bytes 0..6
//   byte 15  sum of bytes 8..14
//   byte 34  sum of bytes 16..33
//
// Fields:
//   byte 21  power:1(b0) onTimer:1(b1) offTimer:1(b2) always-1:(b3) mode:3(b4-6)
//   byte 22  temperature * 2
//   byte 24  swingV:4 (low)  fan:4 (high)
//   byte 25  swingH:4 (low)
//   byte 29  powerful:1(b0)  quiet:1(b5)
//   byte 32  sensor:1(b1)  econo:1(b2)
//   byte 33  mold:1(b1)
//
// Transmission is a 5-bit zero preamble with no header, then three sections
// each with its own header. Least significant bit first.
// ==========================================================================

#define STATE_LEN   35
#define SECTION1    8
#define SECTION2    8
#define SECTION3    (STATE_LEN - SECTION1 - SECTION2)
#define HEADER_BITS 5

// Line coding (microseconds)
#define HDR_MARK      3650
#define HDR_SPACE     1623
#define BIT_MARK      428
#define ONE_SPACE     1280
#define ZERO_SPACE    428
#define GAP           29000
#define SECTION_SPACE (ZERO_SPACE + GAP)

// Mode field (byte 21, bits 4-6)
#define D_AUTO 0b000
#define D_DRY  0b010
#define D_COOL 0b011
#define D_HEAT 0b100
#define D_FAN  0b110

// Fan field (byte 24, high nibble). Speeds 1..5 are stored as 2 + speed.
#define D_FAN_AUTO  0b1010
#define D_FAN_QUIET 0b1011
#define D_FAN_LOW   (2 + 1)
#define D_FAN_MED   (2 + 3)
#define D_FAN_HIGH  (2 + 5)

// Vane fields: all-zero is off, all-ones is on
#define D_SWING_ON  0xF
#define D_SWING_OFF 0x0

static const uint8_t MODE_CODES[DaikinModeCount] = {
    D_COOL, // Off - unused, the power bit carries it
    D_COOL,
    D_AUTO,
    D_DRY,
    D_HEAT,
    D_FAN,
};

static const uint8_t FAN_CODES[DaikinFanCount] = {
    D_FAN_AUTO,
    D_FAN_LOW,
    D_FAN_MED,
    D_FAN_HIGH,
};

static const char* MODE_NAMES[DaikinModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[DaikinFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[DaikinToggleCount] =
    {"Power", "Swing", "Turbo", "Quiet", "Econo", "Mold"};
static const char* EXTRA_NAMES[DaikinExtraCount] = {"SwingH on", "SwingH off", "Sensor"};

/// Each section carries its own sum over its own bytes only.
static void daikin_checksums(uint8_t* st) {
    uint8_t s = 0;
    for(uint8_t i = 0; i < SECTION1 - 1; i++)
        s += st[i];
    st[SECTION1 - 1] = s;

    s = 0;
    for(uint8_t i = SECTION1; i < SECTION1 + SECTION2 - 1; i++)
        s += st[i];
    st[SECTION1 + SECTION2 - 1] = s;

    s = 0;
    for(uint8_t i = SECTION1 + SECTION2; i < STATE_LEN - 1; i++)
        s += st[i];
    st[STATE_LEN - 1] = s;
}

static void
    build_state(const DaikinRequest* req, bool power, uint8_t swing_h, bool sensor, uint8_t* st) {
    DaikinMode mode = req->mode;
    if(mode == DaikinModeOff || mode >= DaikinModeCount) mode = DaikinModeCool;

    DaikinFan fan = req->fan >= DaikinFanCount ? DaikinFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < DAIKIN_TEMP_MIN) temp = DAIKIN_TEMP_MIN;
    if(temp > DAIKIN_TEMP_MAX) temp = DAIKIN_TEMP_MAX;

    uint32_t tb = req->toggle_bits;

    // Start from IRDaikinESP::stateReset()'s fixed bytes
    memset(st, 0, STATE_LEN);
    st[0] = 0x11;
    st[1] = 0xDA;
    st[2] = 0x27;
    st[4] = 0xC5;
    st[8] = 0x11;
    st[9] = 0xDA;
    st[10] = 0x27;
    st[12] = 0x42;
    st[16] = 0x11;
    st[17] = 0xDA;
    st[18] = 0x27;
    st[27] = 0x06;
    st[28] = 0x60;
    st[31] = 0xC0;

    st[21] = (uint8_t)(1 << 3); // bit 3 is always set
    if(power) st[21] |= 1 << 0;
    st[21] |= (uint8_t)((MODE_CODES[mode] & 0x07) << 4);

    st[22] = (uint8_t)(temp * 2);

    uint8_t fan_field = FAN_CODES[fan];
    if((tb >> DaikinToggleQuiet) & 1) fan_field = D_FAN_QUIET;
    st[24] = (uint8_t)((((tb >> DaikinToggleSwing) & 1) ? D_SWING_ON : D_SWING_OFF) |
                       ((fan_field & 0x0F) << 4));

    st[25] = (uint8_t)(swing_h & 0x0F);

    st[29] = 0;
    if((tb >> DaikinTogglePowerful) & 1) st[29] |= 1 << 0;
    if((tb >> DaikinToggleQuiet) & 1) st[29] |= 1 << 5;

    st[32] = 0;
    if(sensor) st[32] |= 1 << 1;
    if((tb >> DaikinToggleEcono) & 1) st[32] |= 1 << 2;

    st[33] = 0;
    if((tb >> DaikinToggleMold) & 1) st[33] |= 1 << 1;

    daikin_checksums(st);
}

/// Five zero bits with no header, then three headed sections.
static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, DAIKIN_IR_MAX_TIMINGS);

    for(uint8_t i = 0; i < HEADER_BITS; i++) {
        ir_bit(&b, false, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    }
    ir_item(&b, BIT_MARK, SECTION_SPACE);

    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, SECTION1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    ir_item(&b, BIT_MARK, SECTION_SPACE);

    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st + SECTION1, SECTION2, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    ir_item(&b, BIT_MARK, SECTION_SPACE);

    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st + SECTION1 + SECTION2, SECTION3, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool daikin_ir_encode_state(const DaikinRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == DaikinModeOff || req->mode >= DaikinModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, D_SWING_OFF, false, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool daikin_ir_encode_toggle(
    const DaikinRequest* req,
    DaikinToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= DaikinToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, toggle != DaikinTogglePowerOff, D_SWING_OFF, false, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool daikin_ir_encode_extra(
    const DaikinRequest* req,
    DaikinExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= DaikinExtraCount) return false;

    uint8_t st[STATE_LEN];
    uint8_t h = extra == DaikinExtraSwingHOn ? D_SWING_ON : D_SWING_OFF;
    build_state(req, true, h, extra == DaikinExtraSensor, st);
    return encode_state_bytes(st, timings, timings_count);
}

void daikin_ir_format_state(const DaikinRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == DaikinModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, D_SWING_OFF, false, st);
    snprintf(out, len, "%02X %02X %02X", st[21], st[22], st[24]);
}

void daikin_ir_format_toggle(const DaikinRequest* req, DaikinToggle toggle, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= DaikinToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, toggle != DaikinTogglePowerOff, D_SWING_OFF, false, st);
    snprintf(out, len, "%02X %02X %02X", st[21], st[24], st[29]);
}

void daikin_ir_format_extra(const DaikinRequest* req, DaikinExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= DaikinExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "%s", EXTRA_NAMES[extra]);
}

bool daikin_ir_toggle_is_momentary(DaikinToggle toggle) {
    (void)toggle;
    return false;
}

bool daikin_ir_mode_locks_fan(DaikinMode mode) {
    (void)mode;
    return false;
}

bool daikin_ir_mode_has_no_temp(DaikinMode mode) {
    return mode == DaikinModeFan;
}

const char* daikin_ir_get_mode_name(DaikinMode mode) {
    return mode < DaikinModeCount ? MODE_NAMES[mode] : "?";
}

const char* daikin_ir_get_fan_name(DaikinFan fan) {
    return fan < DaikinFanCount ? FAN_NAMES[fan] : "?";
}

const char* daikin_ir_get_toggle_name(DaikinToggle toggle) {
    return toggle < DaikinToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* daikin_ir_get_extra_name(DaikinExtra extra) {
    return extra < DaikinExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t daikin_ir_get_option_count(void) {
    return 0; // one variant only
}

const char* daikin_ir_get_option_label(void) {
    return "Model";
}

const char* daikin_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* daikin_ir_get_protocol_name(void) {
    return "Daikin";
}
