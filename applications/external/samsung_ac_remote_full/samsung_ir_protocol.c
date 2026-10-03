#include "samsung_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Samsung, 14-byte frame (IRremoteESP8266 SAMSUNG_AC).
//
// The wire format is unlike anything else in this repo. A short 690 us mark
// and a very long 17844 us space open the transmission, then the state goes
// out in 7-byte sections, each with its own 3086/8864 header and each closed
// by a bit mark and a 2886 us gap.
//
// The per-section checksum counts bits rather than summing bytes: population
// count of byte 0, of byte 1's low nibble, of byte 2's high nibble and of
// bytes 3 to 6, then inverted. The two nibbles it skips are exactly the two
// it is stored in - byte 1's high nibble and byte 2's low nibble - so the
// checksum never covers itself.
// ==========================================================================

#define STATE_LEN   14
#define SECTION_LEN 7

// Line coding (microseconds)
#define LEAD_MARK     690
#define LEAD_SPACE    17844
#define SECTION_MARK  3086
#define SECTION_SPACE 8864
#define SECTION_GAP   2886
#define BIT_MARK      586
#define ONE_SPACE     1432
#define ZERO_SPACE    436

// Mode field (byte 12, bits 4-6)
#define S_AUTO 0
#define S_COOL 1
#define S_DRY  2
#define S_FAN  3
#define S_HEAT 4

// Fan field (byte 12, bits 1-3)
#define S_FAN_AUTO  0
#define S_FAN_LOW   2
#define S_FAN_MED   4
#define S_FAN_HIGH  5
#define S_FAN_AUTO2 6 // the only fan value Auto mode accepts
#define S_FAN_TURBO 7

// Swing field (byte 9, bits 4-6)
#define S_SWING_V    0b010
#define S_SWING_H    0b011
#define S_SWING_BOTH 0b100
#define S_SWING_OFF  0b111

// Special fan field (byte 10, bits 1-3)
#define S_SPECIAL_OFF      0b000
#define S_SPECIAL_POWERFUL 0b011
#define S_SPECIAL_BREEZE   0b101
#define S_SPECIAL_ECONO    0b111

static const uint8_t MODE_CODES[SamsungModeCount] = {
    S_AUTO, // Off - the power field carries it
    S_COOL,
    S_AUTO,
    S_DRY,
    S_HEAT,
    S_FAN,
};

static const uint8_t FAN_CODES[SamsungFanCount] = {
    S_FAN_AUTO,
    S_FAN_LOW,
    S_FAN_MED,
    S_FAN_HIGH,
    S_FAN_TURBO,
};

static const char* const MODE_NAMES[SamsungModeCount] =
    {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* const FAN_NAMES[SamsungFanCount] = {"Auto", "Low", "Med", "High", "Turbo"};
static const char* const TOGGLE_NAMES[SamsungToggleCount] =
    {"Off", "Swi.V", "Swi.H", "Quiet", "Ion", "Light", "Sleep"};
static const char* const EXTRA_NAMES[SamsungExtraCount] = {
    "Powerful",
    "WindFree",
    "Econo",
    "Special off",
    "Swing V+H",
    "Swing off",
    "Self clean",
    "Beep",
};

static uint8_t popcount8(uint8_t v) {
    uint8_t n = 0;
    while(v) {
        n = (uint8_t)(n + (v & 1));
        v >>= 1;
    }
    return n;
}

/// IRSamsungAc::calcSectionChecksum. Deliberately skips byte 1's high nibble
/// and byte 2's low nibble, which is where the result is stored.
static uint8_t samsung_section_checksum(const uint8_t* s) {
    uint8_t sum = 0;
    sum = (uint8_t)(sum + popcount8(s[0]));
    sum = (uint8_t)(sum + popcount8(s[1] & 0x0F));
    sum = (uint8_t)(sum + popcount8((uint8_t)(s[2] >> 4)));
    for(uint8_t i = 3; i < SECTION_LEN; i++) {
        sum = (uint8_t)(sum + popcount8(s[i]));
    }
    return (uint8_t)(sum ^ 0xFF);
}

/// Write one section's checksum into the two nibbles it lives in.
static void samsung_store_checksum(uint8_t* section) {
    uint8_t sum = samsung_section_checksum(section);
    section[1] = (uint8_t)((section[1] & 0x0F) | ((sum & 0x0F) << 4)); // low half
    section[2] = (uint8_t)((section[2] & 0xF0) | (sum >> 4)); // high half
}

static bool tog(const SamsungRequest* req, SamsungToggle t) {
    return (req->toggle_bits >> t) & 1;
}

static void build_state(
    const SamsungRequest* req,
    bool power,
    uint8_t swing,
    uint8_t special,
    bool clean,
    bool beep,
    uint8_t* st) {
    // Byte 6 is 0xF0 and byte 10's high bits are fixed; start from the known
    // good frame IRSamsungAc::stateReset() uses and overwrite the fields we
    // drive, so the bits nobody has decoded keep their documented values.
    static const uint8_t base[STATE_LEN] = {
        0x02, 0x92, 0x0F, 0x00, 0x00, 0x00, 0xF0, 0x01, 0x02, 0xAE, 0x71, 0x00, 0x15, 0xF0};
    memcpy(st, base, STATE_LEN);

    SamsungMode mode = req->mode < SamsungModeCount ? req->mode : SamsungModeAuto;
    uint8_t mode_code = MODE_CODES[mode];

    // Auto mode accepts exactly one fan value, and no other mode accepts it.
    uint8_t fan_code = (mode_code == S_AUTO) ? S_FAN_AUTO2 : FAN_CODES[req->fan % SamsungFanCount];

    uint8_t temp = req->temp;
    if(temp < SAMSUNG_TEMP_MIN) temp = SAMSUNG_TEMP_MIN;
    if(temp > SAMSUNG_TEMP_MAX) temp = SAMSUNG_TEMP_MAX;

    uint8_t p = power ? 0b11 : 0b00;
    st[6] = (uint8_t)((st[6] & ~(0b11 << 4)) | (p << 4));
    st[13] = (uint8_t)((st[13] & ~(0b11 << 4)) | (p << 4));

    st[5] = (uint8_t)(st[5] & ~((1 << 4) | (1 << 5)));
    if(tog(req, SamsungToggleSleep)) st[5] |= 1 << 4;
    if(tog(req, SamsungToggleQuiet)) st[5] |= 1 << 5;

    st[9] = (uint8_t)((st[9] & ~(0b111 << 4)) | ((swing & 0b111) << 4));

    st[10] = (uint8_t)(st[10] & ~((0b111 << 1) | (1 << 4) | (1 << 7)));
    st[10] |= (uint8_t)((special & 0b111) << 1);
    if(tog(req, SamsungToggleDisplay)) st[10] |= 1 << 4;
    if(clean) st[10] |= 1 << 7;

    st[11] = (uint8_t)(st[11] & ~((1 << 0) | (1 << 1) | 0xF0));
    if(tog(req, SamsungToggleIon)) st[11] |= 1 << 0;
    if(clean) st[11] |= 1 << 1;
    st[11] |= (uint8_t)((uint8_t)(temp - SAMSUNG_TEMP_MIN) << 4);

    st[12] = (uint8_t)((fan_code & 0b111) << 1);
    st[12] |= (uint8_t)((mode_code & 0b111) << 4);

    st[13] = (uint8_t)(st[13] & ~(1 << 2));
    if(beep) st[13] |= 1 << 2;

    samsung_store_checksum(st);
    samsung_store_checksum(st + SECTION_LEN);
}

static uint8_t swing_for(const SamsungRequest* req) {
    bool v = tog(req, SamsungToggleSwingV);
    bool h = tog(req, SamsungToggleSwingH);
    if(v && h) return S_SWING_BOTH;
    if(v) return S_SWING_V;
    if(h) return S_SWING_H;
    return S_SWING_OFF;
}

/// Lead-in, then each 7-byte section under its own header.
static bool encode_sections(const uint8_t* st, uint8_t len, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, SAMSUNG_IR_MAX_TIMINGS);

    ir_item(&b, LEAD_MARK, LEAD_SPACE);

    uint8_t sections = (uint8_t)(len / SECTION_LEN);
    for(uint8_t s = 0; s < sections; s++) {
        ir_item(&b, SECTION_MARK, SECTION_SPACE);
        ir_bytes_lsb(
            &b, st + (size_t)s * SECTION_LEN, SECTION_LEN, BIT_MARK, ONE_SPACE, ZERO_SPACE);
        if(s + 1 < sections) ir_item(&b, BIT_MARK, SECTION_GAP);
    }
    return ir_build_finish(&b, BIT_MARK, count);
}

static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    return encode_sections(st, STATE_LEN, timings, count);
}

// --------------------------------------------------------------------------
// The extended frame.
//
// A real Samsung handset does not always send the same length. Power changes,
// timers and sleep go out as a 21-byte message with a fixed extra section
// wedged in the middle; everything else is the 14-byte one. This is a message
// type rather than a model, so it belongs here rather than in the Setup
// picker - the app picks the right one the way the remote does.
// --------------------------------------------------------------------------

#define EXTENDED_LEN 21

static bool encode_extended(const uint8_t* st, uint32_t* timings, size_t* count) {
    static const uint8_t middle[SECTION_LEN] = {0x01, 0xD2, 0x0F, 0x00, 0x00, 0x00, 0x00};

    uint8_t ext[EXTENDED_LEN];
    memcpy(ext, st, SECTION_LEN);
    memcpy(ext + SECTION_LEN, middle, SECTION_LEN);
    memcpy(ext + 2 * SECTION_LEN, st + SECTION_LEN, SECTION_LEN);

    // Every section carries its own checksum, the inserted one included.
    for(uint8_t s = 0; s < EXTENDED_LEN / SECTION_LEN; s++) {
        samsung_store_checksum(ext + (size_t)s * SECTION_LEN);
    }
    return encode_sections(ext, EXTENDED_LEN, timings, count);
}

bool samsung_ir_encode_state(const SamsungRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == SamsungModeOff || req->mode >= SamsungModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, swing_for(req), S_SPECIAL_OFF, false, false, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool samsung_ir_encode_toggle(
    const SamsungRequest* req,
    SamsungToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= SamsungToggleCount) return false;

    uint8_t st[STATE_LEN];
    bool power = toggle != SamsungTogglePowerOff;
    build_state(req, power, swing_for(req), S_SPECIAL_OFF, false, false, st);

    // Power and sleep changes go out as the long message on a real remote.
    if(toggle == SamsungTogglePowerOff || toggle == SamsungToggleSleep) {
        return encode_extended(st, timings, timings_count);
    }
    return encode_state_bytes(st, timings, timings_count);
}

bool samsung_ir_encode_extra(
    const SamsungRequest* req,
    SamsungExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= SamsungExtraCount) return false;

    uint8_t swing = swing_for(req);
    uint8_t special = S_SPECIAL_OFF;
    bool clean = false;
    bool beep = false;

    switch(extra) {
    case SamsungExtraPowerful:
        special = S_SPECIAL_POWERFUL;
        break;
    case SamsungExtraBreeze:
        special = S_SPECIAL_BREEZE;
        break;
    case SamsungExtraEcono:
        special = S_SPECIAL_ECONO;
        break;
    case SamsungExtraSpecialOff:
        special = S_SPECIAL_OFF;
        break;
    case SamsungExtraSwingBoth:
        swing = S_SWING_BOTH;
        break;
    case SamsungExtraSwingNone:
        swing = S_SWING_OFF;
        break;
    case SamsungExtraClean:
        clean = true;
        break;
    default:
        beep = true;
        break;
    }

    uint8_t st[STATE_LEN];
    build_state(req, true, swing, special, clean, beep, st);
    return encode_state_bytes(st, timings, timings_count);
}

void samsung_ir_format_state(const SamsungRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == SamsungModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, swing_for(req), S_SPECIAL_OFF, false, false, st);
    snprintf(out, len, "%02X %02X %02X", st[11], st[12], st[9]);
}

void samsung_ir_format_toggle(
    const SamsungRequest* req,
    SamsungToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= SamsungToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(
        req, toggle != SamsungTogglePowerOff, swing_for(req), S_SPECIAL_OFF, false, false, st);
    snprintf(out, len, "%02X %02X %02X", st[5], st[6], st[10]);
}

void samsung_ir_format_extra(const SamsungRequest* req, SamsungExtra extra, char* out, size_t len) {
    (void)req;
    if(!out || !len) return;
    snprintf(out, len, "%s", extra < SamsungExtraCount ? EXTRA_NAMES[extra] : "-");
}

bool samsung_ir_toggle_is_momentary(SamsungToggle toggle) {
    (void)toggle;
    return false;
}

bool samsung_ir_mode_locks_fan(SamsungMode mode) {
    // Auto mode accepts exactly one fan value and ignores anything else.
    return mode == SamsungModeAuto;
}

bool samsung_ir_mode_has_no_temp(SamsungMode mode) {
    return mode == SamsungModeFan;
}

const char* samsung_ir_get_mode_name(SamsungMode mode) {
    return mode < SamsungModeCount ? MODE_NAMES[mode] : "?";
}

const char* samsung_ir_get_fan_name(SamsungFan fan) {
    return fan < SamsungFanCount ? FAN_NAMES[fan] : "?";
}

const char* samsung_ir_get_toggle_name(SamsungToggle toggle) {
    return toggle < SamsungToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* samsung_ir_get_extra_name(SamsungExtra extra) {
    return extra < SamsungExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t samsung_ir_get_option_count(void) {
    // The 21-byte extended frame is a message type, not a model: the app
    // sends it for power and sleep the way the handset does, so there is
    // nothing here for the user to choose.
    return 0;
}

const char* samsung_ir_get_option_label(void) {
    return "Model";
}

const char* samsung_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* samsung_ir_get_protocol_name(void) {
    return "Samsung";
}
