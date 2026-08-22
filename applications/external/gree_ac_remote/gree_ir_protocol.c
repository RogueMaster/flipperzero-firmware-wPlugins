#include "gree_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Gree 8-byte protocol (Amana, Cooper & Hunter, EKOKAI, Green, RusClimate,
// Soleus Air, Ultimate, Vaillant and other Gree OEMs).
//
// Layout from IRremoteESP8266 src/ir_Gree.h (GreeProtocol union) rather than
// ESPHome's gree.cpp: ESPHome writes absolute Celsius into byte 1, which also
// sets the TimerHalfHr bit. Harmless while the timer is disabled, but the
// union is the stricter description so it is what this follows.
//
//   byte 0  mode:3 power:1 fan:2 swingAuto:1 sleep:1
//   byte 1  temp-16 :4  (timer fields above, left clear)
//   byte 2  timerHours:4 turbo:1 light:1 modelA:1 xfan:1
//   byte 3  bits 4-7 fixed 0b0101
//   byte 4  swingV:4 swingH:3
//   byte 5  bits 3-5 fixed 0b100
//   byte 6  unused
//   byte 7  econo:1 (bit 2), checksum:4 (bits 4-7)
//
// Wire format: header, bytes 0-3 LSB-first, the three fixed bits 0/1/0, a
// mark and a long gap, then bytes 4-7 LSB-first and a closing mark.
// ==========================================================================

#define STATE_LEN 8

// Line coding (microseconds)
#define HDR_MARK      9000
#define HDR_SPACE     4000
#define BIT_MARK      620
#define ONE_SPACE     1600
#define ZERO_SPACE    540
#define MESSAGE_SPACE 19000

// Mode field (byte 0, bits 0-2)
#define G_AUTO 0
#define G_COOL 1
#define G_DRY  2
#define G_FAN  3
#define G_HEAT 4

// Gree locks the setpoint to 25C in Auto
#define G_AUTO_TEMP 25

// Vertical vane positions (byte 4, bits 0-3)
#define G_VANE_AUTO     0
#define G_VANE_SWING    1
#define G_VANE_UP       2
#define G_VANE_MID_UP   3
#define G_VANE_MIDDLE   4
#define G_VANE_MID_DOWN 5
#define G_VANE_DOWN     6

/// Kelvinator's block checksum, which Gree reuses.
#define CHECKSUM_START 10

static const uint8_t MODE_CODES[GreeModeCount] = {
    G_COOL, // Off - unused, the power bit carries it
    G_COOL,
    G_AUTO,
    G_DRY,
    G_HEAT,
    G_FAN,
};

static const uint8_t VANE_CODES[GreeExtraCount] = {
    G_VANE_AUTO,
    G_VANE_UP,
    G_VANE_MID_UP,
    G_VANE_MIDDLE,
    G_VANE_MID_DOWN,
    G_VANE_DOWN,
};

static const char* MODE_NAMES[GreeModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[GreeFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[GreeToggleCount] =
    {"Power", "Swing", "Turbo", "Light", "X-Fan", "Sleep", "Econo"};
static const char* EXTRA_NAMES[GreeExtraCount] =
    {"Vane auto", "Vane up", "Vane m-up", "Vane mid", "Vane m-dn", "Vane down"};
static const char* MODEL_NAMES[GreeModelCount] = {"YAW1F", "YBOFB", "YX1FSF"};

/// Low nibbles of bytes 0-3 plus high nibbles of bytes 4-6, seeded with 10.
static uint8_t gree_checksum(const uint8_t* st) {
    uint8_t sum = CHECKSUM_START;
    for(uint8_t i = 0; i < 4; i++) {
        sum += st[i] & 0x0F;
    }
    for(uint8_t i = 4; i < STATE_LEN - 1; i++) {
        sum += st[i] >> 4;
    }
    return sum & 0x0F;
}

static void build_state(const GreeRequest* req, bool power, uint8_t vane, uint8_t* st) {
    GreeMode mode = req->mode;
    if(mode == GreeModeOff || mode >= GreeModeCount) mode = GreeModeCool;

    GreeFan fan = req->fan >= GreeFanCount ? GreeFanAuto : req->fan;

    uint8_t temp = (mode == GreeModeAuto) ? G_AUTO_TEMP : req->temp;
    if(temp < GREE_TEMP_MIN) temp = GREE_TEMP_MIN;
    if(temp > GREE_TEMP_MAX) temp = GREE_TEMP_MAX;

    uint32_t tb = req->toggle_bits;

    memset(st, 0, STATE_LEN);

    st[0] = (uint8_t)(MODE_CODES[mode] & 0x07);
    if(power) st[0] |= 1 << 3;
    st[0] |= (uint8_t)((fan & 0x03) << 4);
    if((tb >> GreeToggleSwing) & 1) st[0] |= 1 << 6;
    if((tb >> GreeToggleSleep) & 1) st[0] |= 1 << 7;

    st[1] = (uint8_t)((temp - GREE_TEMP_MIN) & 0x0F);

    if((tb >> GreeToggleTurbo) & 1) st[2] |= 1 << 4;
    if((tb >> GreeToggleLight) & 1) st[2] |= 1 << 5;
    if((tb >> GreeToggleXfan) & 1) st[2] |= 1 << 7;
    // IRGreeAC ties this bit to YAW1F while powered on
    if(power && req->option == GreeModelYAW1F) st[2] |= 1 << 6;

    st[3] = 0x50; // unknown1 = 0b0101
    st[4] = (uint8_t)(vane & 0x0F);
    st[5] = 0x20; // unknown2 = 0b100
    st[6] = 0x00;

    st[7] = 0;
    if((tb >> GreeToggleEcono) & 1) st[7] |= 1 << 2;
    st[7] |= (uint8_t)(gree_checksum(st) << 4);
}

/// Bytes 0-3, three fixed bits, a long gap, then bytes 4-7.
static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, GREE_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, 4, BIT_MARK, ONE_SPACE, ZERO_SPACE);

    // Three constant bits between the halves: 0, 1, 0
    ir_bit(&b, false, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    ir_bit(&b, true, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    ir_bit(&b, false, BIT_MARK, ONE_SPACE, ZERO_SPACE);

    ir_item(&b, BIT_MARK, MESSAGE_SPACE);
    ir_bytes_lsb(&b, st + 4, 4, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, count);
}

bool gree_ir_encode_state(const GreeRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == GreeModeOff || req->mode >= GreeModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(
        req, true, ((req->toggle_bits >> GreeToggleSwing) & 1) ? G_VANE_SWING : G_VANE_AUTO, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool gree_ir_encode_toggle(
    const GreeRequest* req,
    GreeToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= GreeToggleCount) return false;

    uint8_t st[STATE_LEN];
    bool power = toggle != GreeTogglePowerOff;
    build_state(
        req, power, ((req->toggle_bits >> GreeToggleSwing) & 1) ? G_VANE_SWING : G_VANE_AUTO, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool gree_ir_encode_extra(
    const GreeRequest* req,
    GreeExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= GreeExtraCount) return false;

    // Parking the vane at a fixed position means the frame must not also ask
    // for auto-swing.
    uint8_t st[STATE_LEN];
    GreeRequest r = *req;
    r.toggle_bits &= ~(1UL << GreeToggleSwing);
    build_state(&r, true, VANE_CODES[extra], st);
    return encode_state_bytes(st, timings, timings_count);
}

void gree_ir_format_state(const GreeRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == GreeModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(
        req, true, ((req->toggle_bits >> GreeToggleSwing) & 1) ? G_VANE_SWING : G_VANE_AUTO, st);
    snprintf(out, len, "%02X %02X %02X", st[0], st[1], st[2]);
}

void gree_ir_format_toggle(const GreeRequest* req, GreeToggle toggle, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= GreeToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(
        req,
        toggle != GreeTogglePowerOff,
        ((req->toggle_bits >> GreeToggleSwing) & 1) ? G_VANE_SWING : G_VANE_AUTO,
        st);
    snprintf(out, len, "%02X %02X %02X", st[0], st[1], st[2]);
}

void gree_ir_format_extra(const GreeRequest* req, GreeExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= GreeExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "vane %u", (unsigned)VANE_CODES[extra]);
}

bool gree_ir_toggle_is_momentary(GreeToggle toggle) {
    (void)toggle;
    return false;
}

bool gree_ir_mode_locks_fan(GreeMode mode) {
    (void)mode;
    return false;
}

bool gree_ir_mode_has_no_temp(GreeMode mode) {
    // Auto pins the setpoint to 25C, Fan-only carries none the user can see
    return mode == GreeModeAuto || mode == GreeModeFan;
}

const char* gree_ir_get_mode_name(GreeMode mode) {
    return mode < GreeModeCount ? MODE_NAMES[mode] : "?";
}

const char* gree_ir_get_fan_name(GreeFan fan) {
    return fan < GreeFanCount ? FAN_NAMES[fan] : "?";
}

const char* gree_ir_get_toggle_name(GreeToggle toggle) {
    return toggle < GreeToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* gree_ir_get_extra_name(GreeExtra extra) {
    return extra < GreeExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t gree_ir_get_option_count(void) {
    return GreeModelCount;
}

const char* gree_ir_get_option_label(void) {
    return "Model";
}

const char* gree_ir_get_option_name(uint8_t option) {
    return option < GreeModelCount ? MODEL_NAMES[option] : "?";
}

const char* gree_ir_get_protocol_name(void) {
    return "Gree";
}
