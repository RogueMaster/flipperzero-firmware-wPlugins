#include "goodweather_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Goodweather, 48-bit frame (IRremoteESP8266 GOODWEATHER). Also badged Rapid.
//
// Two quirks set it apart from every other protocol in this repo:
//
//  1. Logic 1 is the SHORT space (580 us) and logic 0 the long one (1860 us).
//     Everywhere else it is the other way round.
//  2. Every byte is sent twice - once as itself, then inverted - so the
//     48-bit payload takes 96 bits on the wire. There is no checksum; the
//     inverted copies are the integrity check.
//
// The frame closes with a bit mark, a header-length space and one more bit
// mark, which is a footer nothing else here uses.
//
// Bytes 0 and 5 and the top bit of byte 2 carry values IRremoteESP8266 does
// not model. Rather than leave them at zero, the base frame below is taken
// from a real capture off a Rapid handset, which decoded cleanly as
// "power on, cool, 23 C, fan high, swing off".
// ==========================================================================

#define STATE_LEN 6

// Line coding (microseconds). Note ONE_SPACE < ZERO_SPACE.
#define HDR_MARK   6820
#define HDR_SPACE  6820
#define BIT_MARK   580
#define ONE_SPACE  580
#define ZERO_SPACE 1860

// Mode field (byte 4, bits 5-7)
#define G_AUTO 0b000
#define G_COOL 0b001
#define G_DRY  0b010
#define G_FAN  0b011
#define G_HEAT 0b100

// Fan field (byte 3, bits 5-6). The wire order runs backwards from the
// user-facing one: high is 1 and low is 3.
#define G_FAN_AUTO 0b00
#define G_FAN_HIGH 0b01
#define G_FAN_MED  0b10
#define G_FAN_LOW  0b11

// Swing field (byte 3, bits 2-3)
#define G_SWING_FAST 0b00
#define G_SWING_SLOW 0b01
#define G_SWING_OFF  0b10

// The button the handset believes was pressed (byte 2, bits 0-3)
#define G_CMD_POWER   0x00
#define G_CMD_MODE    0x01
#define G_CMD_UP_TEMP 0x02
#define G_CMD_SWING   0x04
#define G_CMD_FAN     0x05
#define G_CMD_TIMER   0x06
#define G_CMD_AIRFLOW 0x07
#define G_CMD_HOLD    0x08
#define G_CMD_SLEEP   0x09
#define G_CMD_TURBO   0x0A
#define G_CMD_LIGHT   0x0B

static const uint8_t MODE_CODES[GoodweatherModeCount] = {
    G_AUTO, // Off - the power bit carries it
    G_COOL,
    G_AUTO,
    G_DRY,
    G_HEAT,
    G_FAN,
};

static const uint8_t FAN_CODES[GoodweatherFanCount] = {
    G_FAN_AUTO,
    G_FAN_LOW,
    G_FAN_MED,
    G_FAN_HIGH,
};

/// Which button each main-screen toggle claims to be.
static const uint8_t TOGGLE_CMDS[GoodweatherToggleCount] = {
    G_CMD_POWER,
    G_CMD_SWING,
    G_CMD_TURBO,
    G_CMD_LIGHT,
    G_CMD_SLEEP,
    G_CMD_AIRFLOW,
};

static const char* MODE_NAMES[GoodweatherModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[GoodweatherFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[GoodweatherToggleCount] =
    {"Power", "Swing", "Turbo", "Light", "Sleep", "Flow"};
static const char* EXTRA_NAMES[GoodweatherExtraCount] =
    {"Swing fast", "Swing slow", "Swing off", "Hold", "Timer"};

static bool tog(const GoodweatherRequest* req, GoodweatherToggle t) {
    return (req->toggle_bits >> t) & 1;
}

static void build_state(
    const GoodweatherRequest* req,
    bool power,
    uint8_t swing,
    uint8_t command,
    uint8_t* st) {
    // Bytes 0 and 5, and byte 2's top bit, are undocumented. These values are
    // what a real Rapid handset sends.
    static const uint8_t base[STATE_LEN] = {0x00, 0x00, 0x80, 0x00, 0x00, 0x55};
    memcpy(st, base, STATE_LEN);

    GoodweatherMode mode = req->mode < GoodweatherModeCount ? req->mode : GoodweatherModeAuto;

    uint8_t temp = req->temp;
    if(temp < GOODWEATHER_TEMP_MIN) temp = GOODWEATHER_TEMP_MIN;
    if(temp > GOODWEATHER_TEMP_MAX) temp = GOODWEATHER_TEMP_MAX;

    if(tog(req, GoodweatherToggleLight)) st[1] |= 1 << 0;
    if(tog(req, GoodweatherToggleTurbo)) st[1] |= 1 << 3;

    st[2] = (uint8_t)((st[2] & 0xF0) | (command & 0x0F));

    st[3] = 0;
    if(tog(req, GoodweatherToggleSleep)) st[3] |= 1 << 0;
    if(power) st[3] |= 1 << 1;
    st[3] |= (uint8_t)((swing & 0x03) << 2);
    if(tog(req, GoodweatherToggleAirFlow)) st[3] |= 1 << 4;
    st[3] |= (uint8_t)((FAN_CODES[req->fan % GoodweatherFanCount] & 0x03) << 5);

    st[4] = (uint8_t)((temp - GOODWEATHER_TEMP_MIN) & 0x0F);
    st[4] |= (uint8_t)((MODE_CODES[mode] & 0x07) << 5);
}

static uint8_t swing_for(const GoodweatherRequest* req) {
    return tog(req, GoodweatherToggleSwing) ? G_SWING_SLOW : G_SWING_OFF;
}

/// Header, then each byte followed by its own inverse, then the three-part
/// footer. Sent least significant bit first.
static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, GOODWEATHER_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);

    for(uint8_t i = 0; i < STATE_LEN; i++) {
        ir_byte_lsb(&b, st[i], BIT_MARK, ONE_SPACE, ZERO_SPACE);
        ir_byte_lsb(&b, (uint8_t)~st[i], BIT_MARK, ONE_SPACE, ZERO_SPACE);
    }

    // Footer: a bit mark, a header-length space, and one more mark.
    ir_item(&b, BIT_MARK, HDR_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool goodweather_ir_encode_state(
    const GoodweatherRequest* req,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == GoodweatherModeOff || req->mode >= GoodweatherModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, swing_for(req), G_CMD_MODE, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool goodweather_ir_encode_toggle(
    const GoodweatherRequest* req,
    GoodweatherToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= GoodweatherToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, toggle != GoodweatherTogglePowerOff, swing_for(req), TOGGLE_CMDS[toggle], st);
    return encode_state_bytes(st, timings, timings_count);
}

bool goodweather_ir_encode_extra(
    const GoodweatherRequest* req,
    GoodweatherExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= GoodweatherExtraCount) return false;

    static const uint8_t SWINGS[3] = {G_SWING_FAST, G_SWING_SLOW, G_SWING_OFF};

    uint8_t swing = swing_for(req);
    uint8_t cmd = G_CMD_SWING;
    if(extra <= GoodweatherExtraSwingOff) {
        swing = SWINGS[extra];
    } else {
        cmd = extra == GoodweatherExtraHold ? G_CMD_HOLD : G_CMD_TIMER;
    }

    uint8_t st[STATE_LEN];
    build_state(req, true, swing, cmd, st);
    return encode_state_bytes(st, timings, timings_count);
}

void goodweather_ir_format_state(const GoodweatherRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == GoodweatherModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, swing_for(req), G_CMD_MODE, st);
    snprintf(out, len, "%02X %02X %02X", st[2], st[3], st[4]);
}

void goodweather_ir_format_toggle(
    const GoodweatherRequest* req,
    GoodweatherToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= GoodweatherToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, toggle != GoodweatherTogglePowerOff, swing_for(req), TOGGLE_CMDS[toggle], st);
    snprintf(out, len, "%02X %02X %02X", st[1], st[2], st[3]);
}

void goodweather_ir_format_extra(
    const GoodweatherRequest* req,
    GoodweatherExtra extra,
    char* out,
    size_t len) {
    (void)req;
    if(!out || !len) return;
    snprintf(out, len, "%s", extra < GoodweatherExtraCount ? EXTRA_NAMES[extra] : "-");
}

bool goodweather_ir_toggle_is_momentary(GoodweatherToggle toggle) {
    (void)toggle;
    return false;
}

bool goodweather_ir_mode_locks_fan(GoodweatherMode mode) {
    (void)mode;
    return false;
}

bool goodweather_ir_mode_has_no_temp(GoodweatherMode mode) {
    return mode == GoodweatherModeFan;
}

const char* goodweather_ir_get_mode_name(GoodweatherMode mode) {
    return mode < GoodweatherModeCount ? MODE_NAMES[mode] : "?";
}

const char* goodweather_ir_get_fan_name(GoodweatherFan fan) {
    return fan < GoodweatherFanCount ? FAN_NAMES[fan] : "?";
}

const char* goodweather_ir_get_toggle_name(GoodweatherToggle toggle) {
    return toggle < GoodweatherToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* goodweather_ir_get_extra_name(GoodweatherExtra extra) {
    return extra < GoodweatherExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t goodweather_ir_get_option_count(void) {
    return 0; // one frame format only
}

const char* goodweather_ir_get_option_label(void) {
    return "Model";
}

const char* goodweather_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* goodweather_ir_get_protocol_name(void) {
    return "GoodWthr";
}
