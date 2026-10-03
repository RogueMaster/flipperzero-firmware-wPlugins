#include "kelvinator_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Kelvinator, 16-byte frame (IRremoteESP8266 KELVINATOR).
//
// The frame goes out as four 32-bit chunks in two blocks. Each block is
// header, 32 bits, a three-bit 0b010 marker, a gap, 32 more bits, a longer
// gap. Bytes 8-10 are copies of bytes 0-2, so the second block repeats the
// mode, power, fan and temperature and adds the settings the first block has
// no room for.
//
// Each block ends with its own four-bit checksum: seed 10, plus the low
// nibbles of the block's first four bytes, plus the high nibbles of the next
// three, modulo 16. The Gree app in this repo uses the same scheme, which is
// unsurprising - Gree builds these units.
// ==========================================================================

#define STATE_LEN     16
#define BLOCK_LEN     8
#define CHECKSUM_SEED 10

// Line coding (microseconds)
#define HDR_MARK   9010
#define HDR_SPACE  4505
#define BIT_MARK   680
#define ONE_SPACE  1530
#define ZERO_SPACE 510
#define GAP_SPACE  19975

// The three constant bits that close each command chunk, least significant
// bit first: 0, 1, 0.
#define CMD_FOOTER      0b010
#define CMD_FOOTER_BITS 3

// Mode field (byte 0, bits 0-2)
#define K_AUTO 0
#define K_COOL 1
#define K_DRY  2
#define K_FAN  3
#define K_HEAT 4

// Vertical vane (byte 4, bits 0-3)
#define K_VANE_OFF          0b0000
#define K_VANE_AUTO         0b0001
#define K_VANE_HIGHEST      0b0010
#define K_VANE_UPPER_MIDDLE 0b0011
#define K_VANE_MIDDLE       0b0100
#define K_VANE_LOWER_MIDDLE 0b0101
#define K_VANE_LOWEST       0b0110

/// Auto and Dry run at a fixed setpoint; the remote shows 25 C.
#define K_AUTO_TEMP 25

static const uint8_t MODE_CODES[KelvinatorModeCount] = {
    K_AUTO, // Off - the power bit carries it
    K_COOL,
    K_AUTO,
    K_DRY,
    K_HEAT,
    K_FAN,
};

/// The full-resolution fan value in byte 14. Byte 0's basic fan field is this
/// clamped to three.
static const uint8_t FAN_CODES[KelvinatorFanCount] = {0, 1, 2, 3};

static const char* const MODE_NAMES[KelvinatorModeCount] =
    {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* const FAN_NAMES[KelvinatorFanCount] = {"Auto", "Low", "Med", "High"};
static const char* const TOGGLE_NAMES[KelvinatorToggleCount] =
    {"Off", "Swi.V", "Swi.H", "Turbo", "X-Fan", "Light", "Quiet"};
static const char* const EXTRA_NAMES[KelvinatorExtraCount] = {
    "Vane top",
    "Vane up",
    "Vane mid",
    "Vane low",
    "Vane bot",
    "Vane off",
    "Ion on",
    "Ion off",
};

/// IRKelvinatorAC::calcBlockChecksum, for one eight-byte block.
static uint8_t kelvinator_block_checksum(const uint8_t* block) {
    uint8_t sum = CHECKSUM_SEED;
    for(uint8_t i = 0; i < 4; i++) {
        sum = (uint8_t)(sum + (block[i] & 0x0F));
    }
    for(uint8_t i = 4; i < BLOCK_LEN - 1; i++) {
        sum = (uint8_t)(sum + (block[i] >> 4));
    }
    return sum & 0x0F;
}

static bool tog(const KelvinatorRequest* req, KelvinatorToggle t) {
    return (req->toggle_bits >> t) & 1;
}

static void
    build_state(const KelvinatorRequest* req, bool power, uint8_t vane, bool ion, uint8_t* st) {
    memset(st, 0, STATE_LEN);

    KelvinatorMode mode = req->mode < KelvinatorModeCount ? req->mode : KelvinatorModeAuto;
    uint8_t mode_code = MODE_CODES[mode];

    KelvinatorFan fan = req->fan < KelvinatorFanCount ? req->fan : KelvinatorFanAuto;
    uint8_t fan_code = FAN_CODES[fan];
    uint8_t basic_fan = fan_code > 3 ? 3 : fan_code;

    bool swing_h = tog(req, KelvinatorToggleSwingH);
    bool swing_v = tog(req, KelvinatorToggleSwingV) || vane == K_VANE_AUTO;

    // Auto and Dry ignore the setpoint and display 25 C.
    uint8_t temp = req->temp;
    if(mode == KelvinatorModeAuto || mode == KelvinatorModeDry) temp = K_AUTO_TEMP;
    if(temp < KELVINATOR_TEMP_MIN) temp = KELVINATOR_TEMP_MIN;
    if(temp > KELVINATOR_TEMP_MAX) temp = KELVINATOR_TEMP_MAX;

    st[0] = (uint8_t)(mode_code & 0x07);
    if(power) st[0] |= 1 << 3;
    st[0] |= (uint8_t)((basic_fan & 0x03) << 4);
    // The auto-swing bit covers both axes.
    if(swing_v || swing_h) st[0] |= 1 << 6;

    st[1] = (uint8_t)((temp - KELVINATOR_TEMP_MIN) & 0x0F);

    // Turbo is dropped whenever the fan speed is chosen by hand, matching
    // IRKelvinatorAC::setFan.
    if(tog(req, KelvinatorToggleTurbo) && fan == KelvinatorFanAuto) st[2] |= 1 << 4;
    if(tog(req, KelvinatorToggleLight)) st[2] |= 1 << 5;
    if(ion) st[2] |= 1 << 6;
    // X-Fan only means anything in Cool and Dry.
    if(tog(req, KelvinatorToggleXfan) &&
       (mode == KelvinatorModeCool || mode == KelvinatorModeDry)) {
        st[2] |= 1 << 7;
    }

    st[3] = 0x50;

    st[4] = (uint8_t)(vane & 0x0F);
    if(swing_h) st[4] |= 1 << 4;

    st[7] = (uint8_t)(kelvinator_block_checksum(st) << 4);

    st[8] = st[0];
    st[9] = st[1];
    st[10] = st[2];
    st[11] = 0x70;

    if(tog(req, KelvinatorToggleQuiet)) st[12] |= 1 << 7;

    st[14] = (uint8_t)((fan_code & 0x07) << 4);

    st[15] = (uint8_t)(kelvinator_block_checksum(st + BLOCK_LEN) << 4);
}

static uint8_t vane_for(const KelvinatorRequest* req) {
    return tog(req, KelvinatorToggleSwingV) ? K_VANE_AUTO : K_VANE_OFF;
}

/// Header, 32 bits, the three-bit marker, a gap, 32 bits, a longer gap - and
/// then the same again for the second block.
static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, KELVINATOR_IR_MAX_TIMINGS);

    for(uint8_t block = 0; block < 2; block++) {
        const uint8_t* base = st + (size_t)block * BLOCK_LEN;

        ir_item(&b, HDR_MARK, HDR_SPACE);
        ir_bytes_lsb(&b, base, 4, BIT_MARK, ONE_SPACE, ZERO_SPACE);

        for(uint8_t i = 0; i < CMD_FOOTER_BITS; i++) {
            ir_bit(&b, (CMD_FOOTER >> i) & 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
        }
        ir_item(&b, BIT_MARK, GAP_SPACE);

        ir_bytes_lsb(&b, base + 4, 4, BIT_MARK, ONE_SPACE, ZERO_SPACE);
        if(block == 0) ir_item(&b, BIT_MARK, GAP_SPACE * 2);
    }
    return ir_build_finish(&b, BIT_MARK, count);
}

bool kelvinator_ir_encode_state(
    const KelvinatorRequest* req,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == KelvinatorModeOff || req->mode >= KelvinatorModeCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, true, vane_for(req), false, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool kelvinator_ir_encode_toggle(
    const KelvinatorRequest* req,
    KelvinatorToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= KelvinatorToggleCount) return false;

    uint8_t st[STATE_LEN];
    build_state(req, toggle != KelvinatorTogglePowerOff, vane_for(req), false, st);
    return encode_state_bytes(st, timings, timings_count);
}

bool kelvinator_ir_encode_extra(
    const KelvinatorRequest* req,
    KelvinatorExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= KelvinatorExtraCount) return false;

    static const uint8_t VANES[] = {
        K_VANE_HIGHEST,
        K_VANE_UPPER_MIDDLE,
        K_VANE_MIDDLE,
        K_VANE_LOWER_MIDDLE,
        K_VANE_LOWEST,
        K_VANE_OFF,
    };

    uint8_t vane = vane_for(req);
    bool ion = false;
    if(extra <= KelvinatorExtraVaneOff) {
        vane = VANES[extra];
    } else {
        ion = extra == KelvinatorExtraIonOn;
    }

    uint8_t st[STATE_LEN];
    build_state(req, true, vane, ion, st);
    return encode_state_bytes(st, timings, timings_count);
}

void kelvinator_ir_format_state(const KelvinatorRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == KelvinatorModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, true, vane_for(req), false, st);
    snprintf(out, len, "%02X %02X %02X", st[0], st[1], st[14]);
}

void kelvinator_ir_format_toggle(
    const KelvinatorRequest* req,
    KelvinatorToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= KelvinatorToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t st[STATE_LEN];
    build_state(req, toggle != KelvinatorTogglePowerOff, vane_for(req), false, st);
    snprintf(out, len, "%02X %02X %02X", st[0], st[2], st[4]);
}

void kelvinator_ir_format_extra(
    const KelvinatorRequest* req,
    KelvinatorExtra extra,
    char* out,
    size_t len) {
    (void)req;
    if(!out || !len) return;
    snprintf(out, len, "%s", extra < KelvinatorExtraCount ? EXTRA_NAMES[extra] : "-");
}

bool kelvinator_ir_toggle_is_momentary(KelvinatorToggle toggle) {
    (void)toggle;
    return false;
}

bool kelvinator_ir_mode_locks_fan(KelvinatorMode mode) {
    (void)mode;
    return false;
}

bool kelvinator_ir_mode_has_no_temp(KelvinatorMode mode) {
    // Auto and Dry run at a fixed 25 C, and Fan carries no setpoint at all.
    return mode == KelvinatorModeFan || mode == KelvinatorModeAuto || mode == KelvinatorModeDry;
}

const char* kelvinator_ir_get_mode_name(KelvinatorMode mode) {
    return mode < KelvinatorModeCount ? MODE_NAMES[mode] : "?";
}

const char* kelvinator_ir_get_fan_name(KelvinatorFan fan) {
    return fan < KelvinatorFanCount ? FAN_NAMES[fan] : "?";
}

const char* kelvinator_ir_get_toggle_name(KelvinatorToggle toggle) {
    return toggle < KelvinatorToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* kelvinator_ir_get_extra_name(KelvinatorExtra extra) {
    return extra < KelvinatorExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t kelvinator_ir_get_option_count(void) {
    return 0; // one frame format only
}

const char* kelvinator_ir_get_option_label(void) {
    return "Model";
}

const char* kelvinator_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* kelvinator_ir_get_protocol_name(void) {
    return "Kelvinator";
}
