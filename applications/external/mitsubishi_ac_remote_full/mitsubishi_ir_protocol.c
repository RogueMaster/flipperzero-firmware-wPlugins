#include "mitsubishi_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>
#include <string.h>

// ==========================================================================
// Mitsubishi Electric, 18-byte frame (IRremoteESP8266 MITSUBISHI_AC).
//
// Not to be confused with Mitsubishi Heavy Industries, which is a different
// company with a different protocol and its own app in this repo.
//
//   byte 0-4   fixed preamble 23 CB 26 01 00
//   byte 5     bit 5 power
//   byte 6     bits 3-5 mode, bit 6 i-see
//   byte 7     bits 0-3 temperature - 16, bit 4 half degree
//   byte 8     low nibble mode-dependent, high nibble wide vane (swing H)
//   byte 9     bits 0-2 fan, bits 3-5 vane (swing V), bit 6 vane valid,
//              bit 7 fan auto
//   byte 10-12 clock and timers, left at zero
//   byte 13    timer flags
//   byte 14    bit 5 Ecocool
//   byte 16    bit 1 natural flow
//   byte 17    sum of bytes 0..16
//
// Sent twice, least significant bit first, with a 15.5 ms gap between the
// copies.
//
// The low nibble of byte 8 has no named field in IRremoteESP8266's bitfield
// union, but its setMode() writes the whole byte from a mode-dependent table.
// Leaving it at zero produces a frame some units ignore, so the table is
// reproduced here.
// ==========================================================================

#define STATE_LEN 18

// Line coding (microseconds)
#define HDR_MARK   3400
#define HDR_SPACE  1750
#define BIT_MARK   450
#define ONE_SPACE  1300
#define ZERO_SPACE 420
#define RPT_MARK   440
#define RPT_SPACE  15500

// Mode field (byte 6, bits 3-5)
#define M_AUTO 0b100
#define M_COOL 0b011
#define M_DRY  0b010
#define M_HEAT 0b001
#define M_FAN  0b111

// Fan field (byte 9, bits 0-2). Speed 5 does not exist on the wire: the
// library decrements anything at or above it, so the usable range is 0-4 with
// 6 reserved for silent.
#define F_AUTO   0
#define F_LOW    1
#define F_MEDIUM 2
#define F_HIGH   3
#define F_SILENT 6

// Vane, byte 9 bits 3-5
#define V_AUTO    0b000
#define V_HIGHEST 0b001
#define V_HIGH    0b010
#define V_MIDDLE  0b011
#define V_LOW     0b100
#define V_LOWEST  0b101
#define V_SWING   0b111

// Wide vane, byte 8 high nibble
#define W_LEFT_MAX  0b0001
#define W_LEFT      0b0010
#define W_MIDDLE    0b0011
#define W_RIGHT     0b0100
#define W_RIGHT_MAX 0b0101
#define W_WIDE      0b0110
#define W_AUTO      0b1000

// ---- MITSUBISHI112, 14 bytes -------------------------------------------
#define S112_LEN        14
#define S112_HDR_MARK   3450
#define S112_HDR_SPACE  1696
#define S112_BIT_MARK   450
#define S112_ONE_SPACE  1250
#define S112_ZERO_SPACE 385

#define M112_COOL 0b011
#define M112_HEAT 0b001
#define M112_AUTO 0b111
#define M112_DRY  0b010

#define M112_FAN_MIN 0b010
#define M112_FAN_LOW 0b011
#define M112_FAN_MED 0b101
#define M112_FAN_MAX 0b000

#define M112_VANE_AUTO    0b111
#define M112_VANE_HIGHEST 0b001
#define M112_VANE_HIGH    0b010
#define M112_VANE_MIDDLE  0b011
#define M112_VANE_LOW     0b100
#define M112_VANE_LOWEST  0b101

#define M112_WIDE_LEFT_MAX  0b0001
#define M112_WIDE_LEFT      0b0010
#define M112_WIDE_MIDDLE    0b0011
#define M112_WIDE_RIGHT     0b0100
#define M112_WIDE_RIGHT_MAX 0b0101
#define M112_WIDE_WIDE      0b1000
#define M112_WIDE_AUTO      0b1100

#define M112_TEMP_MIN 16
#define M112_TEMP_MAX 31

// ---- MITSUBISHI136, 17 bytes -------------------------------------------
#define S136_LEN        17
#define S136_HDR_MARK   3324
#define S136_HDR_SPACE  1474
#define S136_BIT_MARK   467
#define S136_ONE_SPACE  1137
#define S136_ZERO_SPACE 351

#define M136_FAN_MODE 0b000
#define M136_COOL     0b001
#define M136_HEAT     0b010
#define M136_AUTO     0b011
#define M136_DRY      0b101

#define M136_FAN_MIN 0b00
#define M136_FAN_LOW 0b01
#define M136_FAN_MED 0b10
#define M136_FAN_MAX 0b11

#define M136_VANE_LOWEST  0b0000
#define M136_VANE_LOW     0b0001
#define M136_VANE_HIGH    0b0010
#define M136_VANE_HIGHEST 0b0011
#define M136_VANE_AUTO    0b1100

#define M136_TEMP_MIN 17
#define M136_TEMP_MAX 30

/// Bytes 11..16 are the complements of bytes 5..10.
#define M136_MIRROR_FROM 5
#define M136_MIRROR_LEN  6

static const uint8_t MODE_CODES[MitsubishiModeCount] = {
    M_AUTO, // Off - the power bit carries it, the mode field stays valid
    M_COOL,
    M_AUTO,
    M_DRY,
    M_HEAT,
    M_FAN,
};

/// The undocumented low nibble of byte 8, per mode.
static const uint8_t MODE_BYTE8_LOW[MitsubishiModeCount] = {
    0b0000, // Off - same as Auto
    0b0110, // Cool
    0b0000, // Auto
    0b0010, // Dry
    0b0000, // Heat
    0b0111, // Fan
};

static const uint8_t FAN_CODES[MitsubishiFanCount] = {
    F_AUTO,
    F_LOW,
    F_MEDIUM,
    F_HIGH,
};

/// Frame size for each selectable format, in bytes.
static uint8_t model_state_len(uint8_t model) {
    switch(model) {
    case MitsubishiModel112:
        return S112_LEN;
    case MitsubishiModel136:
        return S136_LEN;
    default:
        return STATE_LEN;
    }
}

static const char* const MODEL_NAMES[MitsubishiModelCount] = {"144-bit", "112-bit", "136-bit"};

static const char* const MODE_NAMES[MitsubishiModeCount] =
    {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* const FAN_NAMES[MitsubishiFanCount] = {"Auto", "Low", "Med", "High"};
static const char* const TOGGLE_NAMES[MitsubishiToggleCount] =
    {"Off", "Swi.V", "Swi.H", "i-see", "Quiet", "Econo", "Natur."};
static const char* const EXTRA_NAMES[MitsubishiExtraCount] = {
    "Vane top",
    "Vane high",
    "Vane mid",
    "Vane low",
    "Vane bot",
    "Wide L.max",
    "Wide left",
    "Wide mid",
    "Wide right",
    "Wide R.max",
    "Wide both",
};

/// Byte 17 is the sum of everything before it.
static uint8_t mitsubishi_checksum(const uint8_t* st) {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < STATE_LEN - 1; i++) {
        sum = (uint8_t)(sum + st[i]);
    }
    return sum;
}

static bool tog(const MitsubishiRequest* req, MitsubishiToggle t) {
    return (req->toggle_bits >> t) & 1;
}

/// Build the 18 bytes. `vane` and `wide` override the swing toggles when the
/// Extra screen asks for a fixed position.
static void
    build_state(const MitsubishiRequest* req, bool power, uint8_t vane, uint8_t wide, uint8_t* st) {
    memset(st, 0, STATE_LEN);

    st[0] = 0x23;
    st[1] = 0xCB;
    st[2] = 0x26;
    st[3] = 0x01;
    st[4] = 0x00;

    if(power) st[5] |= 1 << 5;

    MitsubishiMode mode = req->mode < MitsubishiModeCount ? req->mode : MitsubishiModeAuto;
    st[6] = (uint8_t)((MODE_CODES[mode] & 0x07) << 3);
    if(tog(req, MitsubishiToggleISee)) st[6] |= 1 << 6;

    uint8_t temp = req->temp;
    if(temp < MITSUBISHI_TEMP_MIN) temp = MITSUBISHI_TEMP_MIN;
    if(temp > MITSUBISHI_TEMP_MAX) temp = MITSUBISHI_TEMP_MAX;
    st[7] = (uint8_t)((temp - MITSUBISHI_TEMP_MIN) & 0x0F);

    st[8] = (uint8_t)((MODE_BYTE8_LOW[mode] & 0x0F) | ((wide & 0x0F) << 4));

    uint8_t fan = tog(req, MitsubishiToggleQuiet) ? F_SILENT :
                                                    FAN_CODES[req->fan % MitsubishiFanCount];
    st[9] = (uint8_t)((fan & 0x07) | ((vane & 0x07) << 3) | (1 << 6));
    if(fan == F_AUTO) st[9] |= 1 << 7;

    if(tog(req, MitsubishiToggleEcono)) st[14] |= 1 << 5;
    if(tog(req, MitsubishiToggleNatural)) st[16] |= 1 << 1;

    st[STATE_LEN - 1] = mitsubishi_checksum(st);
}

static uint8_t vane_for(const MitsubishiRequest* req) {
    return tog(req, MitsubishiToggleSwingV) ? V_SWING : V_AUTO;
}

static uint8_t wide_for(const MitsubishiRequest* req) {
    return tog(req, MitsubishiToggleSwingH) ? W_AUTO : W_MIDDLE;
}

// ==========================================================================
// MITSUBISHI112, 14 bytes.
//
// Shares the 23 CB 26 preamble with the 144-bit frame and with TCL112, and
// borrows TCL112's checksum - a plain byte sum - which is why IRremoteESP8266
// literally calls IRTcl112Ac::calcChecksum() here.
//
// The temperature field runs backwards: it stores 31 minus the setpoint, not
// the setpoint minus 16. There is no fan-only mode.
// ==========================================================================

static const uint8_t MODE_CODES_112[MitsubishiModeCount] = {
    M112_AUTO, // Off
    M112_COOL,
    M112_AUTO,
    M112_DRY,
    M112_HEAT,
    M112_AUTO, // no fan-only mode on this frame; auto is the nearest thing
};

static const uint8_t FAN_CODES_112[MitsubishiFanCount] = {
    M112_FAN_MAX, // "auto" is not offered; max is the library's fallback
    M112_FAN_MIN,
    M112_FAN_MED,
    M112_FAN_MAX,
};

static void build_state_112(
    const MitsubishiRequest* req,
    bool power,
    uint8_t vane,
    uint8_t wide,
    uint8_t* st) {
    static const uint8_t base[S112_LEN] = {
        0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03, 0x0B, 0x10, 0x00, 0x00, 0x00, 0x30, 0x00};
    memcpy(st, base, S112_LEN);

    MitsubishiMode mode = req->mode < MitsubishiModeCount ? req->mode : MitsubishiModeAuto;

    uint8_t temp = req->temp;
    if(temp < M112_TEMP_MIN) temp = M112_TEMP_MIN;
    if(temp > M112_TEMP_MAX) temp = M112_TEMP_MAX;

    st[5] = (uint8_t)(st[5] & ~(1 << 2));
    if(power) st[5] |= 1 << 2;

    st[6] = (uint8_t)((st[6] & ~0x07) | (MODE_CODES_112[mode] & 0x07));

    // Stored as 31 - setpoint, the opposite way round from every other frame
    // in this app.
    st[7] = (uint8_t)((st[7] & 0xF0) | ((uint8_t)(M112_TEMP_MAX - temp) & 0x0F));

    st[8] =
        (uint8_t)((FAN_CODES_112[req->fan % MitsubishiFanCount] & 0x07) | ((vane & 0x07) << 3));

    st[12] = (uint8_t)((st[12] & ~(0x0F << 2)) | ((wide & 0x0F) << 2));

    uint8_t sum = 0;
    for(uint8_t i = 0; i < S112_LEN - 1; i++) {
        sum = (uint8_t)(sum + st[i]);
    }
    st[S112_LEN - 1] = sum;
}

// ==========================================================================
// MITSUBISHI136, 17 bytes.
//
// No checksum as such: bytes 11 to 16 are the bitwise complements of bytes 5
// to 10, the same trick Mitsubishi Heavy uses.
// ==========================================================================

static const uint8_t MODE_CODES_136[MitsubishiModeCount] = {
    M136_AUTO, // Off
    M136_COOL,
    M136_AUTO,
    M136_DRY,
    M136_HEAT,
    M136_FAN_MODE,
};

static const uint8_t FAN_CODES_136[MitsubishiFanCount] = {
    M136_FAN_MAX, // no auto on this frame
    M136_FAN_LOW,
    M136_FAN_MED,
    M136_FAN_MAX,
};

static void build_state_136(const MitsubishiRequest* req, bool power, uint8_t vane, uint8_t* st) {
    static const uint8_t base[S136_LEN] = {
        0x23,
        0xCB,
        0x26,
        0x21,
        0x00,
        0x40,
        0xC2,
        0xC7,
        0x04,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00};
    memcpy(st, base, S136_LEN);

    MitsubishiMode mode = req->mode < MitsubishiModeCount ? req->mode : MitsubishiModeAuto;

    uint8_t temp = req->temp;
    if(temp < M136_TEMP_MIN) temp = M136_TEMP_MIN;
    if(temp > M136_TEMP_MAX) temp = M136_TEMP_MAX;

    st[5] = (uint8_t)(st[5] & ~(1 << 6));
    if(power) st[5] |= 1 << 6;

    st[6] = (uint8_t)(MODE_CODES_136[mode] & 0x07);
    st[6] |= (uint8_t)((uint8_t)(temp - MITSUBISHI_TEMP_MIN) << 4);

    st[7] = (uint8_t)((FAN_CODES_136[req->fan % MitsubishiFanCount] & 0x03) << 1);
    st[7] |= (uint8_t)((vane & 0x0F) << 4);

    for(uint8_t i = 0; i < M136_MIRROR_LEN; i++) {
        st[M136_MIRROR_FROM + M136_MIRROR_LEN + i] = (uint8_t)~st[M136_MIRROR_FROM + i];
    }
}

/// Header, 18 bytes, footer, gap, then the whole thing again.
static bool encode_state_bytes(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, MITSUBISHI_IR_MAX_TIMINGS);

    for(uint8_t copy = 0; copy < 2; copy++) {
        ir_item(&b, HDR_MARK, HDR_SPACE);
        ir_bytes_lsb(&b, st, STATE_LEN, BIT_MARK, ONE_SPACE, ZERO_SPACE);
        if(copy == 0) ir_item(&b, RPT_MARK, RPT_SPACE);
    }
    return ir_build_finish(&b, RPT_MARK, count);
}

/// One 14-byte MITSUBISHI112 frame.
static bool encode_112(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, MITSUBISHI_IR_MAX_TIMINGS);
    ir_item(&b, S112_HDR_MARK, S112_HDR_SPACE);
    ir_bytes_lsb(&b, st, S112_LEN, S112_BIT_MARK, S112_ONE_SPACE, S112_ZERO_SPACE);
    return ir_build_finish(&b, S112_BIT_MARK, count);
}

/// One 17-byte MITSUBISHI136 frame.
static bool encode_136(const uint8_t* st, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, MITSUBISHI_IR_MAX_TIMINGS);
    ir_item(&b, S136_HDR_MARK, S136_HDR_SPACE);
    ir_bytes_lsb(&b, st, S136_LEN, S136_BIT_MARK, S136_ONE_SPACE, S136_ZERO_SPACE);
    return ir_build_finish(&b, S136_BIT_MARK, count);
}

/// Build and encode whichever frame format the Setup screen has selected.
/// `vane` and `wide` are in the 144-bit frame's terms; the other two formats
/// translate them, since only the swing-versus-fixed distinction survives.
static bool encode_for_model(
    const MitsubishiRequest* req,
    bool power,
    uint8_t vane,
    uint8_t wide,
    uint32_t* timings,
    size_t* count) {
    uint8_t st[S136_LEN > STATE_LEN ? S136_LEN : STATE_LEN];

    switch(req->option) {
    case MitsubishiModel112: {
        // The 144-bit vane codes happen to be numerically different from the
        // 112-bit ones, so map through the shared meaning rather than reusing
        // the number.
        static const uint8_t VANE[8] = {
            M112_VANE_AUTO, // V_AUTO
            M112_VANE_HIGHEST,
            M112_VANE_HIGH,
            M112_VANE_MIDDLE,
            M112_VANE_LOW,
            M112_VANE_LOWEST,
            M112_VANE_AUTO,
            M112_VANE_AUTO, // V_SWING
        };
        static const uint8_t WIDE[9] = {
            M112_WIDE_MIDDLE,
            M112_WIDE_LEFT_MAX,
            M112_WIDE_LEFT,
            M112_WIDE_MIDDLE,
            M112_WIDE_RIGHT,
            M112_WIDE_RIGHT_MAX,
            M112_WIDE_WIDE,
            M112_WIDE_MIDDLE,
            M112_WIDE_AUTO, // W_AUTO
        };
        uint8_t w = (uint8_t)(wide & 0x0F);
        build_state_112(req, power, VANE[vane & 7], WIDE[w <= 8 ? w : 0], st);
        return encode_112(st, timings, count);
    }
    case MitsubishiModel136: {
        static const uint8_t VANE[8] = {
            M136_VANE_AUTO, // V_AUTO
            M136_VANE_HIGHEST,
            M136_VANE_HIGH,
            M136_VANE_HIGH, // no middle position on this frame
            M136_VANE_LOW,
            M136_VANE_LOWEST,
            M136_VANE_AUTO,
            M136_VANE_AUTO, // V_SWING
        };
        build_state_136(req, power, VANE[vane & 7], st);
        return encode_136(st, timings, count);
    }
    default:
        build_state(req, power, vane, wide, st);
        return encode_state_bytes(st, timings, count);
    }
}

bool mitsubishi_ir_encode_state(
    const MitsubishiRequest* req,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == MitsubishiModeOff || req->mode >= MitsubishiModeCount) return false;
    if(req->option >= MitsubishiModelCount) return false;

    return encode_for_model(req, true, vane_for(req), wide_for(req), timings, timings_count);
}

bool mitsubishi_ir_encode_toggle(
    const MitsubishiRequest* req,
    MitsubishiToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= MitsubishiToggleCount) return false;
    if(req->option >= MitsubishiModelCount) return false;

    return encode_for_model(
        req,
        toggle != MitsubishiTogglePowerOff,
        vane_for(req),
        wide_for(req),
        timings,
        timings_count);
}

bool mitsubishi_ir_encode_extra(
    const MitsubishiRequest* req,
    MitsubishiExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= MitsubishiExtraCount) return false;

    static const uint8_t VANES[] = {V_HIGHEST, V_HIGH, V_MIDDLE, V_LOW, V_LOWEST};
    static const uint8_t WIDES[] = {W_LEFT_MAX, W_LEFT, W_MIDDLE, W_RIGHT, W_RIGHT_MAX, W_WIDE};

    uint8_t vane = vane_for(req);
    uint8_t wide = wide_for(req);
    if(extra <= MitsubishiExtraVaneLowest) {
        vane = VANES[extra];
    } else {
        wide = WIDES[extra - MitsubishiExtraWideLeftMax];
    }

    return encode_for_model(req, true, vane, wide, timings, timings_count);
}

void mitsubishi_ir_format_state(const MitsubishiRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == MitsubishiModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint8_t model = (uint8_t)(req->option % MitsubishiModelCount);
    snprintf(out, len, "%s %u B", MODEL_NAMES[model], model_state_len(model));
}

void mitsubishi_ir_format_toggle(
    const MitsubishiRequest* req,
    MitsubishiToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= MitsubishiToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    uint8_t model = (uint8_t)(req->option % MitsubishiModelCount);
    snprintf(
        out, len, "%s %s", MODEL_NAMES[model], toggle == MitsubishiTogglePowerOff ? "off" : "on");
}

void mitsubishi_ir_format_extra(
    const MitsubishiRequest* req,
    MitsubishiExtra extra,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(extra >= MitsubishiExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    (void)req;
    snprintf(out, len, "%s", EXTRA_NAMES[extra]);
}

bool mitsubishi_ir_toggle_is_momentary(MitsubishiToggle toggle) {
    (void)toggle;
    return false;
}

bool mitsubishi_ir_mode_locks_fan(MitsubishiMode mode) {
    (void)mode;
    return false;
}

bool mitsubishi_ir_mode_has_no_temp(MitsubishiMode mode) {
    return mode == MitsubishiModeFan;
}

const char* mitsubishi_ir_get_mode_name(MitsubishiMode mode) {
    return mode < MitsubishiModeCount ? MODE_NAMES[mode] : "?";
}

const char* mitsubishi_ir_get_fan_name(MitsubishiFan fan) {
    return fan < MitsubishiFanCount ? FAN_NAMES[fan] : "?";
}

const char* mitsubishi_ir_get_toggle_name(MitsubishiToggle toggle) {
    return toggle < MitsubishiToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* mitsubishi_ir_get_extra_name(MitsubishiExtra extra) {
    return extra < MitsubishiExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t mitsubishi_ir_get_option_count(void) {
    return MitsubishiModelCount;
}

const char* mitsubishi_ir_get_option_label(void) {
    return "Model";
}

const char* mitsubishi_ir_get_option_name(uint8_t option) {
    return option < MitsubishiModelCount ? MODEL_NAMES[option] : "?";
}

const char* mitsubishi_ir_get_protocol_name(void) {
    return "Mitsubishi";
}
