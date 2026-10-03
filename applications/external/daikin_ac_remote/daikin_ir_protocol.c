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

/// Named after the handset, which is what is printed on the remote in the
/// user's hand. The detector prints the same names on its Model page.
static const char* MODEL_NAMES[DaikinModelCount] =
    {"ARC433", "ARC477", "ARC484", "ARC423", "BRC4C15", "ARC480", "BRC52B", "DGS01"};

static const char* MODE_NAMES[DaikinModeCount] = {"Off", "Cool", "Auto", "Dry", "Heat", "Fan"};
static const char* FAN_NAMES[DaikinFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[DaikinToggleCount] =
    {"Power", "Swing", "Turbo", "Quiet", "Econo", "Mold"};
static const char* EXTRA_NAMES[DaikinExtraCount] = {"SwingH on", "SwingH off", "Sensor"};

/// Each section carries its own sum over its own bytes only.
// ==========================================================================
// The other seven frame formats, selected on the Setup screen.
//
// They share almost nothing but the 11 DA preamble: different headers,
// different bit spaces, different section splits, different checksums, and in
// two cases a different mode and fan encoding altogether. Each one is built
// from the reset state IRremoteESP8266 uses, so the bits nobody has decoded
// keep their documented values.
// ==========================================================================

#define SUM_BYTES(dst, src, n)              \
    do {                                    \
        uint8_t s_ = 0;                     \
        for(uint8_t i_ = 0; i_ < (n); i_++) \
            s_ = (uint8_t)(s_ + (src)[i_]); \
        (dst) = s_;                         \
    } while(0)

/// Nibble sum, the form IRutils::sumNibbles uses.
static uint8_t sum_nibbles(const uint8_t* p, uint8_t len, uint8_t init) {
    uint8_t sum = init;
    for(uint8_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + (p[i] >> 4) + (p[i] & 0x0F));
    }
    return sum;
}

static uint8_t to_bcd(uint8_t v) {
    return (uint8_t)(((v / 10) << 4) + (v % 10));
}

static uint8_t clamp_temp(uint8_t t, uint8_t lo, uint8_t hi) {
    if(t < lo) return lo;
    if(t > hi) return hi;
    return t;
}

// ---- ARC477A1 / Daikin2, 39 bytes ---------------------------------------
#define D2_LEN           39
#define D2_SEC1          20
#define D2_LEADER_MARK   10024
#define D2_LEADER_SPACE  25180
#define D2_HDR_MARK      3500
#define D2_HDR_SPACE     1728
#define D2_BIT_MARK      460
#define D2_ONE_SPACE     1270
#define D2_ZERO_SPACE    420
#define D2_GAP           (D2_LEADER_MARK + D2_LEADER_SPACE)
#define D2_SWINGV_AUTO   0xF
#define D2_SWINGV_OFF    0xE
#define D2_SWINGH_AUTO   0xBE
#define D2_SWINGH_OFF    0xBF
#define D2_MIN_COOL_TEMP 18

static void build_2(const DaikinRequest* req, bool power, bool swing_h, uint8_t* st) {
    static const uint8_t base[D2_LEN] = {0x11, 0xDA, 0x27, 0x00, 0x01, 0x00, 0xC0, 0x70,
                                         0x08, 0x0C, 0x80, 0x04, 0xB0, 0x16, 0x24, 0x00,
                                         0x00, 0xBE, 0xD0, 0x00, 0x11, 0xDA, 0x27, 0x00,
                                         0x00, 0x08, 0x00, 0x00, 0xA0, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0xC1, 0x80, 0x60, 0x00};
    memcpy(st, base, D2_LEN);

    DaikinMode mode = req->mode < DaikinModeCount ? req->mode : DaikinModeAuto;
    uint8_t mode_code = MODE_CODES[mode];

    // Cool has a higher floor than the other modes on this handset.
    uint8_t lo = (mode_code == D_COOL) ? D2_MIN_COOL_TEMP : DAIKIN_TEMP_MIN;
    uint8_t temp = clamp_temp(req->temp, lo, DAIKIN_TEMP_MAX);

    st[17] = swing_h ? D2_SWINGH_AUTO : D2_SWINGH_OFF;
    st[18] = (uint8_t)((st[18] & 0xF0) |
                       (((req->toggle_bits >> DaikinToggleSwing) & 1) ? D2_SWINGV_AUTO :
                                                                        D2_SWINGV_OFF));

    st[25] = (uint8_t)(st[25] & ~0x71);
    if(power) st[25] |= 1;
    st[25] |= (uint8_t)((mode_code & 0x07) << 4);

    st[26] = (uint8_t)((st[26] & ~(0x3F << 1)) | ((temp & 0x3F) << 1));
    st[28] = (uint8_t)((st[28] & 0x0F) | ((FAN_CODES[req->fan % DaikinFanCount] & 0x0F) << 4));

    SUM_BYTES(st[D2_SEC1 - 1], st, D2_SEC1 - 1);
    SUM_BYTES(st[D2_LEN - 1], st + D2_SEC1, D2_LEN - D2_SEC1 - 1);
}

static bool encode_2(const uint8_t* st, uint32_t* t, size_t* n) {
    IrBuild b = ir_build_init(t, DAIKIN_IR_MAX_TIMINGS);
    ir_item(&b, D2_LEADER_MARK, D2_LEADER_SPACE);
    ir_item(&b, D2_HDR_MARK, D2_HDR_SPACE);
    ir_bytes_lsb(&b, st, D2_SEC1, D2_BIT_MARK, D2_ONE_SPACE, D2_ZERO_SPACE);
    ir_item(&b, D2_BIT_MARK, D2_GAP);
    ir_item(&b, D2_HDR_MARK, D2_HDR_SPACE);
    ir_bytes_lsb(&b, st + D2_SEC1, D2_LEN - D2_SEC1, D2_BIT_MARK, D2_ONE_SPACE, D2_ZERO_SPACE);
    return ir_build_finish(&b, D2_BIT_MARK, n);
}

// ---- ARC433B69 / ARC484A4 / Daikin216, 27 bytes -------------------------
#define D216_LEN        27
#define D216_SEC1       8
#define D216_HDR_MARK   3440
#define D216_HDR_SPACE  1750
#define D216_BIT_MARK   420
#define D216_ONE_SPACE  1300
#define D216_ZERO_SPACE 450
#define D216_GAP        29650

static void build_216(const DaikinRequest* req, bool power, bool swing_h, uint8_t* st) {
    static const uint8_t base[D216_LEN] = {0x11, 0xDA, 0x27, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x11,
                                           0xDA, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00};
    memcpy(st, base, D216_LEN);

    DaikinMode mode = req->mode < DaikinModeCount ? req->mode : DaikinModeAuto;
    uint8_t temp = clamp_temp(req->temp, DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX);
    bool swing_v = (req->toggle_bits >> DaikinToggleSwing) & 1;

    st[13] = (uint8_t)(power ? 1 : 0);
    st[13] |= (uint8_t)((MODE_CODES[mode] & 0x07) << 4);

    // Stored as plain degrees, not an offset.
    st[14] = (uint8_t)((temp & 0x3F) << 1);

    st[16] = (uint8_t)((swing_v ? D_SWING_ON : D_SWING_OFF) |
                       ((FAN_CODES[req->fan % DaikinFanCount] & 0x0F) << 4));
    st[17] = (uint8_t)(swing_h ? D_SWING_ON : D_SWING_OFF);

    if((req->toggle_bits >> DaikinTogglePowerful) & 1) st[21] |= 1;

    SUM_BYTES(st[D216_SEC1 - 1], st, D216_SEC1 - 1);
    SUM_BYTES(st[D216_LEN - 1], st + D216_SEC1, D216_LEN - D216_SEC1 - 1);
}

// ---- ARC423A5 / Daikin160, 20 bytes -------------------------------------
#define D160_LEN         20
#define D160_SEC1        7
#define D160_HDR_MARK    5000
#define D160_HDR_SPACE   2145
#define D160_BIT_MARK    342
#define D160_ONE_SPACE   1786
#define D160_ZERO_SPACE  700
#define D160_GAP         29650
#define D160_SWINGV_AUTO 0xF
#define D160_SWINGV_OFF  0x0

static void build_160(const DaikinRequest* req, bool power, uint8_t* st) {
    static const uint8_t base[D160_LEN] = {0x11, 0xDA, 0x27, 0xF0, 0x0D, 0x00, 0x00,
                                           0x11, 0xDA, 0x27, 0x00, 0xD3, 0x30, 0x11,
                                           0x00, 0x00, 0x1E, 0x0A, 0x08, 0x00};
    memcpy(st, base, D160_LEN);

    DaikinMode mode = req->mode < DaikinModeCount ? req->mode : DaikinModeAuto;
    uint8_t temp = clamp_temp(req->temp, DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX);
    bool swing_v = (req->toggle_bits >> DaikinToggleSwing) & 1;

    st[12] = (uint8_t)(power ? 1 : 0);
    st[12] |= (uint8_t)((MODE_CODES[mode] & 0x07) << 4);

    st[13] = (uint8_t)((st[13] & 0x0F) | ((swing_v ? D160_SWINGV_AUTO : D160_SWINGV_OFF) << 4));

    // Offset by ten, unlike Daikin216 which stores plain degrees.
    st[16] = (uint8_t)((st[16] & ~(0x3F << 1)) | (((temp - 10) & 0x3F) << 1));
    st[17] = (uint8_t)((st[17] & 0xF0) | (FAN_CODES[req->fan % DaikinFanCount] & 0x0F));

    SUM_BYTES(st[D160_SEC1 - 1], st, D160_SEC1 - 1);
    SUM_BYTES(st[D160_LEN - 1], st + D160_SEC1, D160_LEN - D160_SEC1 - 1);
}

// ---- BRC4C151 / BRC4C153 / Daikin176, 22 bytes --------------------------
#define D176_LEN        22
#define D176_SEC1       7
#define D176_HDR_MARK   5070
#define D176_HDR_SPACE  2140
#define D176_BIT_MARK   370
#define D176_ONE_SPACE  1780
#define D176_ZERO_SPACE 710
#define D176_GAP        29410

// This handset has its own mode encoding, unrelated to the shared one.
#define D176_FAN_MODE    0b000
#define D176_HEAT        0b001
#define D176_COOL        0b010
#define D176_AUTO        0b011
#define D176_DRY         0b111
#define D176_DRYFAN_TEMP 17
#define D176_FAN_MIN     1
#define D176_FAN_MAX     3
#define D176_SWINGH_AUTO 0x5
#define D176_SWINGH_OFF  0x6
#define D176_MODE_BUTTON 0b00000100

static const uint8_t MODE_CODES_176[DaikinModeCount] = {
    D176_AUTO, // Off
    D176_COOL,
    D176_AUTO,
    D176_DRY,
    D176_HEAT,
    D176_FAN_MODE,
};

static void build_176(const DaikinRequest* req, bool power, bool swing_h, uint8_t* st) {
    static const uint8_t base[D176_LEN] = {0x11, 0xDA, 0x17, 0x18, 0x04, 0x00, 0x00, 0x11,
                                           0xDA, 0x17, 0x18, 0x00, 0x73, 0x00, 0x20, 0x00,
                                           0x00, 0x00, 0x16, 0x00, 0x20, 0x00};
    memcpy(st, base, D176_LEN);

    DaikinMode mode = req->mode < DaikinModeCount ? req->mode : DaikinModeAuto;
    uint8_t mode_code = MODE_CODES_176[mode];

    // Dry and fan-only run at a fixed 17 C on this handset.
    uint8_t temp = clamp_temp(req->temp, DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX);
    if(mode_code == D176_DRY || mode_code == D176_FAN_MODE) temp = D176_DRYFAN_TEMP;

    uint8_t alt;
    switch(mode_code) {
    case D176_DRY:
        alt = 2;
        break;
    case D176_FAN_MODE:
        alt = 6;
        break;
    default:
        alt = 7;
        break;
    }
    st[12] = (uint8_t)((st[12] & ~(0x07 << 4)) | ((alt & 0x07) << 4));
    st[13] = D176_MODE_BUTTON;

    st[14] = (uint8_t)(power ? 1 : 0);
    st[14] |= (uint8_t)((mode_code & 0x07) << 4);

    // Offset by nine, which is neither of the other two conventions.
    st[17] = (uint8_t)((st[17] & ~(0x3F << 1)) | (((temp - 9) & 0x3F) << 1));

    // Only two speeds exist here.
    uint8_t fan = (req->fan == DaikinFanLow) ? D176_FAN_MIN : D176_FAN_MAX;
    st[18] = (uint8_t)((swing_h ? D176_SWINGH_AUTO : D176_SWINGH_OFF) | ((fan & 0x0F) << 4));

    SUM_BYTES(st[D176_SEC1 - 1], st, D176_SEC1 - 1);
    SUM_BYTES(st[D176_LEN - 1], st + D176_SEC1, D176_LEN - D176_SEC1 - 1);
}

// ---- ARC480A5 / Daikin152, 19 bytes -------------------------------------
#define D152_LEN           19
#define D152_HDR_MARK      3492
#define D152_HDR_SPACE     1718
#define D152_BIT_MARK      433
#define D152_ONE_SPACE     1529
#define D152_ZERO_SPACE    433
#define D152_GAP           25182
#define D152_LEADER_BITS   5
#define D152_MIN_COOL_TEMP 18

static void build_152(const DaikinRequest* req, bool power, uint8_t* st) {
    static const uint8_t base[D152_LEN] = {
        0x11,
        0xDA,
        0x27,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC5,
        0x00,
        0x00,
        0x00};
    memcpy(st, base, D152_LEN);

    DaikinMode mode = req->mode < DaikinModeCount ? req->mode : DaikinModeAuto;
    uint8_t mode_code = MODE_CODES[mode];
    bool swing_v = (req->toggle_bits >> DaikinToggleSwing) & 1;

    // Everything except Heat has the higher floor.
    uint8_t lo = (mode_code == D_HEAT) ? DAIKIN_TEMP_MIN : D152_MIN_COOL_TEMP;
    uint8_t temp = clamp_temp(req->temp, lo, DAIKIN_TEMP_MAX);

    st[5] = (uint8_t)(power ? 1 : 0);
    st[5] |= (uint8_t)((mode_code & 0x07) << 4);

    st[6] = (uint8_t)((temp & 0x7F) << 1);

    st[8] = (uint8_t)((swing_v ? D_SWING_ON : D_SWING_OFF) |
                      ((FAN_CODES[req->fan % DaikinFanCount] & 0x0F) << 4));

    if((req->toggle_bits >> DaikinTogglePowerful) & 1) st[13] |= 1;
    if((req->toggle_bits >> DaikinToggleQuiet) & 1) st[13] |= 1 << 5;
    if((req->toggle_bits >> DaikinToggleEcono) & 1) st[16] |= 1 << 2;

    SUM_BYTES(st[D152_LEN - 1], st, D152_LEN - 1);
}

// ---- BRC52B63 / 17 series / Daikin128, 16 bytes -------------------------
#define D128_LEN          16
#define D128_SEC          8
#define D128_LEADER_MARK  9800
#define D128_LEADER_SPACE 9800
#define D128_HDR_MARK     4600
#define D128_HDR_SPACE    2500
#define D128_BIT_MARK     350
#define D128_ONE_SPACE    954
#define D128_ZERO_SPACE   382
#define D128_GAP          20300
#define D128_MIN_TEMP     16
#define D128_MAX_TEMP     30

#define D128_DRY  0b0001
#define D128_COOL 0b0010
#define D128_FAN  0b0100
#define D128_HEAT 0b1000
#define D128_AUTO 0b1010

#define D128_FAN_AUTO     0b0001
#define D128_FAN_HIGH     0b0010
#define D128_FAN_MED      0b0100
#define D128_FAN_LOW      0b1000
#define D128_FAN_POWERFUL 0b0011
#define D128_FAN_QUIET    0b1001

static const uint8_t MODE_CODES_128[DaikinModeCount] = {
    D128_AUTO, // Off
    D128_COOL,
    D128_AUTO,
    D128_DRY,
    D128_HEAT,
    D128_FAN,
};

static const uint8_t FAN_CODES_128[DaikinFanCount] = {
    D128_FAN_AUTO,
    D128_FAN_LOW,
    D128_FAN_MED,
    D128_FAN_HIGH,
};

static void build_128(const DaikinRequest* req, bool power, uint8_t* st) {
    static const uint8_t base[D128_LEN] = {
        0x16,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x04,
        0xA1,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00};
    memcpy(st, base, D128_LEN);

    DaikinMode mode = req->mode < DaikinModeCount ? req->mode : DaikinModeAuto;
    uint8_t mode_code = MODE_CODES_128[mode];
    uint8_t fan = FAN_CODES_128[req->fan % DaikinFanCount];

    // Powerful and quiet live in the fan field, and neither is valid in Auto.
    if(mode_code != D128_AUTO) {
        if((req->toggle_bits >> DaikinTogglePowerful) & 1)
            fan = D128_FAN_POWERFUL;
        else if((req->toggle_bits >> DaikinToggleQuiet) & 1)
            fan = D128_FAN_QUIET;
    }

    st[1] = (uint8_t)((mode_code & 0x0F) | ((fan & 0x0F) << 4));
    st[6] = to_bcd(clamp_temp(req->temp, D128_MIN_TEMP, D128_MAX_TEMP));

    st[7] = (uint8_t)(st[7] & 0x0F);
    if((req->toggle_bits >> DaikinToggleSwing) & 1) st[7] |= 1;
    if(power) st[7] |= 1 << 3;

    if((req->toggle_bits >> DaikinToggleEcono) & 1) st[9] |= 1 << 2;

    // The first checksum is a nibble and folds in the low nibble of the byte
    // it is stored in; the second is a plain byte of nibble sums.
    st[7] =
        (uint8_t)((st[7] & 0x0F) | ((sum_nibbles(st, D128_SEC - 1, st[7] & 0x0F) & 0x0F) << 4));
    st[D128_LEN - 1] = sum_nibbles(st + D128_SEC, D128_SEC - 1, 0);
}

// ---- DGS01 / Daikin64, one 64-bit word ----------------------------------
#define D64_BITS            64
#define D64_MIN_TEMP        16
#define D64_MAX_TEMP        30
#define D64_CHECKSUM_OFFSET 60

#define D64_DRY  0b0001
#define D64_COOL 0b0010
#define D64_FAN  0b0100
#define D64_HEAT 0b1000

#define D64_FAN_AUTO  0b0001
#define D64_FAN_LOW   0b1000
#define D64_FAN_MED   0b0100
#define D64_FAN_HIGH  0b0010
#define D64_FAN_QUIET 0b1001
#define D64_FAN_TURBO 0b0011

static const uint8_t MODE_CODES_64[DaikinModeCount] = {
    D64_COOL, // Off
    D64_COOL,
    D64_COOL, // no auto on this handset
    D64_DRY,
    D64_HEAT,
    D64_FAN,
};

static const uint8_t FAN_CODES_64[DaikinFanCount] = {
    D64_FAN_AUTO,
    D64_FAN_LOW,
    D64_FAN_MED,
    D64_FAN_HIGH,
};

static uint64_t build_64(const DaikinRequest* req, bool power) {
    uint64_t raw = 0x7C16161607204216ULL; // IRDaikin64's known good state

    DaikinMode mode = req->mode < DaikinModeCount ? req->mode : DaikinModeCool;
    uint8_t fan = FAN_CODES_64[req->fan % DaikinFanCount];
    if((req->toggle_bits >> DaikinTogglePowerful) & 1)
        fan = D64_FAN_TURBO;
    else if((req->toggle_bits >> DaikinToggleQuiet) & 1)
        fan = D64_FAN_QUIET;

    raw &= ~(0xFULL << 8);
    raw |= (uint64_t)(MODE_CODES_64[mode] & 0x0F) << 8;
    raw &= ~(0xFULL << 12);
    raw |= (uint64_t)(fan & 0x0F) << 12;

    raw &= ~(0xFFULL << 40);
    raw |= (uint64_t)to_bcd(clamp_temp(req->temp, D64_MIN_TEMP, D64_MAX_TEMP)) << 40;

    raw &= ~(1ULL << 48);
    if((req->toggle_bits >> DaikinToggleSwing) & 1) raw |= 1ULL << 48;
    raw &= ~(1ULL << 51);
    if(power) raw |= 1ULL << 51;

    // Four-bit sum of every nibble below the checksum itself.
    raw &= ~(0xFULL << D64_CHECKSUM_OFFSET);
    uint64_t data = raw & ((1ULL << D64_CHECKSUM_OFFSET) - 1);
    uint8_t sum = 0;
    for(; data; data >>= 4) {
        sum = (uint8_t)(sum + (data & 0x0F));
    }
    raw |= (uint64_t)(sum & 0x0F) << D64_CHECKSUM_OFFSET;
    return raw;
}

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

/// Two headed sections separated by a gap - the shape 216, 160 and 176 share.
static bool encode_two_sections(
    const uint8_t* st,
    uint8_t len,
    uint8_t sec1,
    uint16_t hdr_mark,
    uint16_t hdr_space,
    uint16_t bit_mark,
    uint16_t one_space,
    uint16_t zero_space,
    uint32_t gap,
    uint32_t* t,
    size_t* n) {
    IrBuild b = ir_build_init(t, DAIKIN_IR_MAX_TIMINGS);
    ir_item(&b, hdr_mark, hdr_space);
    ir_bytes_lsb(&b, st, sec1, bit_mark, one_space, zero_space);
    ir_item(&b, bit_mark, gap);
    ir_item(&b, hdr_mark, hdr_space);
    ir_bytes_lsb(&b, st + sec1, (size_t)(len - sec1), bit_mark, one_space, zero_space);
    return ir_build_finish(&b, bit_mark, n);
}

/// ARC480A5: five zero bits, a gap, then one headed section.
static bool encode_152(const uint8_t* st, uint32_t* t, size_t* n) {
    IrBuild b = ir_build_init(t, DAIKIN_IR_MAX_TIMINGS);
    for(uint8_t i = 0; i < D152_LEADER_BITS; i++) {
        ir_bit(&b, false, D152_BIT_MARK, D152_ONE_SPACE, D152_ZERO_SPACE);
    }
    ir_item(&b, D152_BIT_MARK, D152_GAP);
    ir_item(&b, D152_HDR_MARK, D152_HDR_SPACE);
    ir_bytes_lsb(&b, st, D152_LEN, D152_BIT_MARK, D152_ONE_SPACE, D152_ZERO_SPACE);
    return ir_build_finish(&b, D152_BIT_MARK, n);
}

/// BRC52B63: two leader bursts, a headed section, then a bare one.
static bool encode_128(const uint8_t* st, uint32_t* t, size_t* n) {
    IrBuild b = ir_build_init(t, DAIKIN_IR_MAX_TIMINGS);
    for(uint8_t i = 0; i < 2; i++) {
        ir_item(&b, D128_LEADER_MARK, D128_LEADER_SPACE);
    }
    ir_item(&b, D128_HDR_MARK, D128_HDR_SPACE);
    ir_bytes_lsb(&b, st, D128_SEC, D128_BIT_MARK, D128_ONE_SPACE, D128_ZERO_SPACE);
    ir_item(&b, D128_BIT_MARK, D128_GAP);
    ir_bytes_lsb(&b, st + D128_SEC, D128_SEC, D128_BIT_MARK, D128_ONE_SPACE, D128_ZERO_SPACE);
    return ir_build_finish(&b, D128_HDR_MARK, n);
}

/// DGS01: two leader bursts, a header, 64 bits, then a closing header mark.
static bool encode_64(uint64_t raw, uint32_t* t, size_t* n) {
    IrBuild b = ir_build_init(t, DAIKIN_IR_MAX_TIMINGS);
    for(uint8_t i = 0; i < 2; i++) {
        ir_item(&b, D128_LEADER_MARK, D128_LEADER_SPACE);
    }
    ir_item(&b, D128_HDR_MARK, D128_HDR_SPACE);
    for(uint8_t i = 0; i < D64_BITS; i++) {
        ir_bit(&b, (raw >> i) & 1, D128_BIT_MARK, D128_ONE_SPACE, D128_ZERO_SPACE);
    }
    ir_item(&b, D128_BIT_MARK, D128_GAP);
    return ir_build_finish(&b, D128_HDR_MARK, n);
}

/// Build and encode whichever format the Setup screen has selected.
static bool encode_for_model(
    const DaikinRequest* req,
    bool power,
    bool swing_h,
    bool sensor,
    uint32_t* t,
    size_t* n) {
    uint8_t st[D2_LEN]; // the largest of the eight

    switch(req->option) {
    case DaikinModelArc477:
        build_2(req, power, swing_h, st);
        return encode_2(st, t, n);
    case DaikinModel216:
        build_216(req, power, swing_h, st);
        return encode_two_sections(
            st,
            D216_LEN,
            D216_SEC1,
            D216_HDR_MARK,
            D216_HDR_SPACE,
            D216_BIT_MARK,
            D216_ONE_SPACE,
            D216_ZERO_SPACE,
            D216_GAP,
            t,
            n);
    case DaikinModel160:
        build_160(req, power, st);
        return encode_two_sections(
            st,
            D160_LEN,
            D160_SEC1,
            D160_HDR_MARK,
            D160_HDR_SPACE,
            D160_BIT_MARK,
            D160_ONE_SPACE,
            D160_ZERO_SPACE,
            D160_GAP,
            t,
            n);
    case DaikinModel176:
        build_176(req, power, swing_h, st);
        return encode_two_sections(
            st,
            D176_LEN,
            D176_SEC1,
            D176_HDR_MARK,
            D176_HDR_SPACE,
            D176_BIT_MARK,
            D176_ONE_SPACE,
            D176_ZERO_SPACE,
            D176_GAP,
            t,
            n);
    case DaikinModel152:
        build_152(req, power, st);
        return encode_152(st, t, n);
    case DaikinModel128:
        build_128(req, power, st);
        return encode_128(st, t, n);
    case DaikinModel64:
        return encode_64(build_64(req, power), t, n);
    default:
        build_state(req, power, swing_h ? D_SWING_ON : D_SWING_OFF, sensor, st);
        return encode_state_bytes(st, t, n);
    }
}

bool daikin_ir_encode_state(const DaikinRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == DaikinModeOff || req->mode >= DaikinModeCount) return false;
    if(req->option >= DaikinModelCount) return false;

    return encode_for_model(req, true, false, false, timings, timings_count);
}

bool daikin_ir_encode_toggle(
    const DaikinRequest* req,
    DaikinToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= DaikinToggleCount) return false;
    if(req->option >= DaikinModelCount) return false;

    return encode_for_model(
        req, toggle != DaikinTogglePowerOff, false, false, timings, timings_count);
}

bool daikin_ir_encode_extra(
    const DaikinRequest* req,
    DaikinExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= DaikinExtraCount) return false;
    if(req->option >= DaikinModelCount) return false;

    return encode_for_model(
        req,
        true,
        extra == DaikinExtraSwingHOn,
        extra == DaikinExtraSensor,
        timings,
        timings_count);
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
    return DaikinModelCount;
}

const char* daikin_ir_get_option_label(void) {
    return "Model";
}

const char* daikin_ir_get_option_name(uint8_t option) {
    return option < DaikinModelCount ? MODEL_NAMES[option] : "?";
}

const char* daikin_ir_get_protocol_name(void) {
    return "Daikin";
}
