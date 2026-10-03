#include "kelon_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Kelon / Hisense, 48-bit frame (IRremoteESP8266 KELON).
//
// A single 48-bit word, least significant bit first, with no checksum. Power
// and vertical swing are toggle bits: the frame asks the unit to flip them
// rather than stating what they should be, so the app has no way to know or
// display which way they ended up.
//
// The fan field is stored backwards. IRKelonAc::setFan maps the sane range
// 0,1..3 onto 0,3..1 on the wire, and the comment in that function says as
// much: "Kelon fan speeds are backwards!".
// ==========================================================================

#define KELON_BITS 48

// Line coding (microseconds)
#define HDR_MARK   9000
#define HDR_SPACE  4600
#define BIT_MARK   560
#define ONE_SPACE  1680
#define ZERO_SPACE 600

// Bit positions within the 48-bit word
#define POS_FAN         16
#define POS_POWER       18
#define POS_SLEEP       19
#define POS_DRY_GRADE   20
#define POS_SWING_V     23
#define POS_MODE        24
#define POS_TEMP        28
#define POS_SMART       39
#define POS_SUPERCOOL_1 44
#define POS_SUPERCOOL_2 47

#define PREAMBLE 0x0683ULL // byte 0 = 0x83, byte 1 = 0x06

// Mode field
#define K_HEAT  0
#define K_SMART 1
#define K_COOL  2
#define K_DRY   3
#define K_FAN   4

/// Fixed setpoints the unit shows in the modes that ignore the user's.
#define K_SMART_TEMP  26
#define K_DRYFAN_TEMP 25

static const uint8_t MODE_CODES[KelonModeCount] = {
    K_COOL, // Off - power is a toggle, so the mode field just stays valid
    K_COOL,
    K_SMART,
    K_DRY,
    K_HEAT,
    K_FAN,
};

/// The wire runs backwards from the user-facing order: auto stays 0, but then
/// low is 3 and high is 1.
static const uint8_t FAN_CODES[KelonFanCount] = {0, 3, 2, 1};

/// Named after the handset, matching what the detector prints.
static const char* const MODEL_NAMES[KelonModelCount] = {"RCH", "DG11R2"};

static const char* const MODE_NAMES[KelonModeCount] =
    {"Off", "Cool", "Smart", "Dry", "Heat", "Fan"};
static const char* const FAN_NAMES[KelonFanCount] = {"Auto", "Low", "Med", "High"};
static const char* const TOGGLE_NAMES[KelonToggleCount] = {"Off", "Swing V", "Sleep", "Super"};
static const char* const EXTRA_NAMES[KelonExtraCount] =
    {"Dry -2", "Dry -1", "Dry 0", "Dry +1", "Dry +2"};

static void put(uint64_t* raw, uint8_t pos, uint8_t width, uint64_t value) {
    uint64_t mask = ((1ULL << width) - 1ULL) << pos;
    *raw = (*raw & ~mask) | ((value << pos) & mask);
}

static bool tog(const KelonRequest* req, KelonToggle t) {
    return (req->toggle_bits >> t) & 1;
}

/// Build the 48-bit word. `power_toggle` asks the unit to flip its power;
/// `swing_toggle` does the same for the vertical vane.
static uint64_t
    build_raw(const KelonRequest* req, bool power_toggle, bool swing_toggle, uint8_t dry_grade) {
    uint64_t raw = PREAMBLE;

    KelonMode mode = req->mode < KelonModeCount ? req->mode : KelonModeCool;
    uint8_t mode_code = MODE_CODES[mode];

    // Smart, Dry and Fan run at a fixed setpoint the remote displays.
    uint8_t temp = req->temp;
    if(mode == KelonModeSmart) {
        temp = K_SMART_TEMP;
    } else if(mode == KelonModeDry || mode == KelonModeFan) {
        temp = K_DRYFAN_TEMP;
    }
    if(temp < KELON_TEMP_MIN) temp = KELON_TEMP_MIN;
    if(temp > KELON_TEMP_MAX) temp = KELON_TEMP_MAX;

    put(&raw, POS_FAN, 2, FAN_CODES[req->fan % KelonFanCount]);
    put(&raw, POS_POWER, 1, power_toggle ? 1 : 0);
    put(&raw, POS_SLEEP, 1, tog(req, KelonToggleSleep) ? 1 : 0);
    put(&raw, POS_DRY_GRADE, 3, dry_grade & 0x07);
    put(&raw, POS_SWING_V, 1, swing_toggle ? 1 : 0);
    put(&raw, POS_MODE, 3, mode_code);
    put(&raw, POS_TEMP, 4, (uint64_t)(temp - KELON_TEMP_MIN));
    put(&raw, POS_SMART, 1, mode == KelonModeSmart ? 1 : 0);

    // Super cool is stored twice and cancels itself in anything but Cool.
    bool super = tog(req, KelonToggleSuperCool) && mode == KelonModeCool;
    put(&raw, POS_SUPERCOOL_1, 1, super ? 1 : 0);
    put(&raw, POS_SUPERCOOL_2, 1, super ? 1 : 0);

    return raw;
}

// ==========================================================================
// DG11R2-01, 21 bytes in three sections (KELON168).
//
// Same header and bit spaces as the 48-bit frame, but the payload is split
// 6 / 8 / 7 with an 8 ms gap between the pieces and only the first section
// carrying a header. Unlike almost everything else in this repo the checksums
// are XORs, not sums, and unlike the 48-bit frame the power bit is absolute
// state rather than a toggle.
// ==========================================================================

#define K168_LEN          21
#define K168_SEC1         6
#define K168_SEC2         8
#define K168_SEC3         7
#define K168_FOOTER_SPACE 8000
#define K168_MIN_TEMP     16
#define K168_MAX_TEMP     32

#define K168_HEAT  0
#define K168_SMART 1
#define K168_COOL  2
#define K168_DRY   3
#define K168_FAN   4

#define K168_CMD_POWER 0x01
#define K168_CMD_MODE  0x06
#define K168_CMD_SWING 0x07
#define K168_CMD_FAN   0x11

static const uint8_t MODE_CODES_168[KelonModeCount] = {
    K168_COOL, // Off - power is a real bit here, so the mode stays valid
    K168_COOL,
    K168_SMART,
    K168_DRY,
    K168_HEAT,
    K168_FAN,
};

/// The fan speed is split across two fields, and the pair is not a simple
/// encoding of the speed: see IRKelon168Ac::setFan.
static const uint8_t FAN_CODES_168[KelonFanCount] = {0b00, 0b11, 0b10, 0b01};
static const uint8_t FAN2_CODES_168[KelonFanCount] = {0, 1, 0, 0};

static uint8_t xor_bytes(const uint8_t* p, uint8_t len) {
    uint8_t x = 0;
    for(uint8_t i = 0; i < len; i++) {
        x ^= p[i];
    }
    return x;
}

static void build_168(const KelonRequest* req, bool power, bool swing, uint8_t cmd, uint8_t* st) {
    memset(st, 0, K168_LEN);
    st[0] = 0x83;
    st[1] = 0x06;
    st[6] = 0x80;
    st[18] = 0x28; // remote model bits

    KelonMode mode = req->mode < KelonModeCount ? req->mode : KelonModeCool;
    uint8_t fan_idx = (uint8_t)(req->fan % KelonFanCount);

    uint8_t temp = req->temp;
    if(mode == KelonModeSmart) temp = 26;
    temp = temp < K168_MIN_TEMP ? K168_MIN_TEMP : (temp > K168_MAX_TEMP ? K168_MAX_TEMP : temp);

    st[2] = (uint8_t)(FAN_CODES_168[fan_idx] & 0x03);
    if(power) st[2] |= 1 << 2;
    if((req->toggle_bits >> KelonToggleSleep) & 1) st[2] |= 1 << 3;
    if(swing) st[2] |= 1 << 7;

    st[3] = (uint8_t)((MODE_CODES_168[mode] & 0x07) | ((uint8_t)(temp - K168_MIN_TEMP) << 4));

    st[15] = cmd;
    if(FAN2_CODES_168[fan_idx]) st[16] |= 1 << 1;

    // Power lives twice: as a bit in byte 2 and as the "On" bit in byte 18.
    if(power) st[18] |= 1 << 4;

    st[13] = xor_bytes(st + 2, 10); // bytes 2..11
    st[20] = xor_bytes(st + 14, 6); // bytes 14..19
}

/// Header, then three sections separated by an 8 ms gap. Only the first
/// section carries a header.
static bool encode_168(const uint8_t* st, uint32_t* t, size_t* n) {
    IrBuild b = ir_build_init(t, KELON_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    ir_bytes_lsb(&b, st, K168_SEC1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    ir_item(&b, BIT_MARK, K168_FOOTER_SPACE);
    ir_bytes_lsb(&b, st + K168_SEC1, K168_SEC2, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    ir_item(&b, BIT_MARK, K168_FOOTER_SPACE);
    ir_bytes_lsb(&b, st + K168_SEC1 + K168_SEC2, K168_SEC3, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    return ir_build_finish(&b, BIT_MARK, n);
}

static bool encode_raw(uint64_t raw, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, KELON_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    for(uint8_t i = 0; i < KELON_BITS; i++) {
        ir_bit(&b, (raw >> i) & 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    }
    return ir_build_finish(&b, BIT_MARK, count);
}

bool kelon_ir_encode_state(const KelonRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == KelonModeOff || req->mode >= KelonModeCount) return false;
    if(req->option >= KelonModelCount) return false;

    if(req->option == KelonModel168) {
        // Power is absolute state on this frame, so a settings change says on.
        uint8_t st[K168_LEN];
        build_168(req, true, (req->toggle_bits >> KelonToggleSwingV) & 1, K168_CMD_MODE, st);
        return encode_168(st, timings, timings_count);
    }

    // A settings change leaves power alone: the bit would flip the unit off.
    return encode_raw(build_raw(req, false, false, 0), timings, timings_count);
}

bool kelon_ir_encode_toggle(
    const KelonRequest* req,
    KelonToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= KelonToggleCount) return false;
    if(req->option >= KelonModelCount) return false;

    bool power = toggle == KelonTogglePowerOff;
    bool swing = toggle == KelonToggleSwingV;

    if(req->option == KelonModel168) {
        // Here power is state, not a toggle: a power press means "off".
        uint8_t st[K168_LEN];
        uint8_t cmd = power ? K168_CMD_POWER : (swing ? K168_CMD_SWING : K168_CMD_FAN);
        build_168(req, !power, swing || ((req->toggle_bits >> KelonToggleSwingV) & 1), cmd, st);
        return encode_168(st, timings, timings_count);
    }
    return encode_raw(build_raw(req, power, swing, 0), timings, timings_count);
}

bool kelon_ir_encode_extra(
    const KelonRequest* req,
    KelonExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= KelonExtraCount) return false;
    if(req->option >= KelonModelCount) return false;

    if(req->option == KelonModel168) {
        // The 168-bit frame has no dehumidifier grade field, so the Extra
        // entries all resend the current state rather than doing nothing
        // surprising.
        uint8_t st[K168_LEN];
        build_168(req, true, (req->toggle_bits >> KelonToggleSwingV) & 1, K168_CMD_MODE, st);
        return encode_168(st, timings, timings_count);
    }

    // Grades -2..+2 are stored biased by two, so -2 is 0 and +2 is 4.
    return encode_raw(build_raw(req, false, false, (uint8_t)extra), timings, timings_count);
}

void kelon_ir_format_state(const KelonRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == KelonModeOff) {
        snprintf(out, len, "power toggle");
        return;
    }
    uint64_t raw = build_raw(req, false, false, 0);
    snprintf(out, len, "%06lX", (unsigned long)((raw >> 16) & 0xFFFFFFFFUL));
}

void kelon_ir_format_toggle(const KelonRequest* req, KelonToggle toggle, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= KelonToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint64_t raw = build_raw(req, toggle == KelonTogglePowerOff, toggle == KelonToggleSwingV, 0);
    snprintf(out, len, "%06lX", (unsigned long)((raw >> 16) & 0xFFFFFFFFUL));
}

void kelon_ir_format_extra(const KelonRequest* req, KelonExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    snprintf(out, len, "%s", extra < KelonExtraCount ? EXTRA_NAMES[extra] : "-");
}

bool kelon_ir_toggle_is_momentary(KelonToggle toggle) {
    // On the 48-bit frame power and swing are toggle bits: it says "flip it",
    // so the app cannot know which way the unit ended up and must not draw a
    // latched indicator. The 168-bit frame carries real state, but the option
    // is not visible from here, so keep the conservative answer.
    return toggle == KelonTogglePowerOff || toggle == KelonToggleSwingV;
}

bool kelon_ir_mode_locks_fan(KelonMode mode) {
    return mode == KelonModeSmart;
}

bool kelon_ir_mode_has_no_temp(KelonMode mode) {
    // Smart, Dry and Fan all run at a setpoint the unit chooses.
    return mode == KelonModeSmart || mode == KelonModeDry || mode == KelonModeFan;
}

const char* kelon_ir_get_mode_name(KelonMode mode) {
    return mode < KelonModeCount ? MODE_NAMES[mode] : "?";
}

const char* kelon_ir_get_fan_name(KelonFan fan) {
    return fan < KelonFanCount ? FAN_NAMES[fan] : "?";
}

const char* kelon_ir_get_toggle_name(KelonToggle toggle) {
    return toggle < KelonToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* kelon_ir_get_extra_name(KelonExtra extra) {
    return extra < KelonExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t kelon_ir_get_option_count(void) {
    return KelonModelCount;
}

const char* kelon_ir_get_option_label(void) {
    return "Model";
}

const char* kelon_ir_get_option_name(uint8_t option) {
    return option < KelonModelCount ? MODEL_NAMES[option] : "?";
}

const char* kelon_ir_get_protocol_name(void) {
    return "Kelon";
}
