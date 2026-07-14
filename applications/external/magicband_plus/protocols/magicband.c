#include <stdlib.h>
#include <string.h>
#include "magicband.h"
#include "_protocols.h"

// ─── Palettes ────────────────────────────────────────────────────────────────

static const char* const COLORS[32] = {
    "Cyan",      "Purple",     "Blue",       "Midnight", "Blue II", "Violet",     "Lavender",
    "Purple II", "Pink",       "Pink II",    "Pink III", "Pink IV", "Pink V",     "Pink VI",
    "Pink VII",  "Yel-Orange", "Off-Yellow", "Yellow",   "Lime",    "Orange",     "Red-Orange",
    "Red",       "Cyan II",    "Cyan III",   "Cyan IV",  "Green",   "Lime Green", "White",
    "White II",  "Off",        "Unique",     "Random"};

typedef struct {
    uint8_t val;
    const char* name;
} VibePair;
static const VibePair VIBES[] = {
    {0x0, "Off"},
    {0xB, "Pulse (1s)"},
    {0xA, "Short (0.5s)"},
    {0x7, "Long (2s)"},
    {0x5, "Fast"},
    {0x6, "Complex"},
    {0x8, "Rapid"},
};
#define VIBE_COUNT ((uint8_t)(sizeof(VIBES) / sizeof(VIBES[0])))

static const char* const MASKS[] = {
    "All LEDs",
    "Top Right",
    "Bot Right",
    "Bot Left",
    "Top Left",
};
#define MASK_COUNT 5

static const char* const TIMING_NAMES[] = {"Always On", "~20s", "~10s", "~5s", "~3s"};
static const uint8_t TIMING_VALS[] = {0x8F, 0x09, 0x06, 0x04, 0x03};
#define TIMING_COUNT 5

// LED position labels for E9-09
static const char* const LED_LABELS[] = {"Center", "Top-R", "Bot-R", "Bot-L", "Top-L"};

const char* mb_color_name(uint8_t c) {
    return COLORS[c & 0x1F];
}
const char* mb_vibe_name(uint8_t v) {
    for(uint8_t i = 0; i < VIBE_COUNT; i++)
        if(VIBES[i].val == v) return VIBES[i].name;
    return "Off";
}

// ─── Packet builders ─────────────────────────────────────────────────────────

// Wrap mfg-specific data in a BLE adv payload (Disney company ID 0x0183 LE).
static size_t build_adv(const uint8_t* mfg, size_t mlen, uint8_t* out) {
    uint8_t* p = out;
    *p++ = 0x02;
    *p++ = 0x01;
    *p++ = 0x06; // Flags: LE general discoverable
    *p++ = (uint8_t)(3 + mlen); // AD length
    *p++ = 0xFF; // AD type: manufacturer specific
    *p++ = 0x83;
    *p++ = 0x01; // Disney company ID (little-endian)
    for(size_t i = 0; i < mlen; i++)
        *p++ = mfg[i];
    return (size_t)(p - out);
}

// CC fast-mode ping: cc03000100 keeps band in high-duty BLE scan mode
size_t mb_build_fast_ping(uint8_t* out) {
    static const uint8_t mfg[] = {0xE1, 0x00, 0xCC, 0x03, 0x00, 0x01, 0x00};
    return build_adv(mfg, sizeof(mfg), out);
}

// E9-05: single 5-bit palette color.
//   timing byte 0x8F = always-on (bit7=1 per EMCOT docs).
//   mask = bits[7:5] of color byte; color5 = bits[4:0].
//   vibe = upper nibble of last byte; 0x0B lower = enable colors+vibe.
static void build_e905(const MagicbandCfg* c, uint8_t out[9]) {
    out[0] = 0xE1;
    out[1] = 0x00;
    out[2] = 0xE9;
    out[3] = 0x05;
    out[4] = 0x00;
    out[5] = c->timing;
    out[6] = 0x0E;
    out[7] = (uint8_t)((c->mask << 5) | (c->color5 & 0x1F));
    out[8] = c->vibe ? (uint8_t)((c->vibe << 4) | 0x0B) : 0x00;
}

// E9-06: dual palette (outer ring + inner/center).
static void build_e906(const MagicbandCfg* c, uint8_t out[10]) {
    // bits[7:5] of each color byte: use 0x40 pattern from known-good examples
    out[0] = 0xE2;
    out[1] = 0x00;
    out[2] = 0xE9;
    out[3] = 0x06;
    out[4] = 0x00;
    out[5] = 0x22;
    out[6] = 0x0F;
    out[7] = (uint8_t)(0x40 | (c->color5_inner & 0x1F));
    out[8] = (uint8_t)(0x40 | (c->color5_outer & 0x1F));
    out[9] = c->vibe ? (uint8_t)((c->vibe << 4) | 0x0B) : 0xB0;
}

// E9-08: single 6-bit RGB color.
//   Each channel: bit7=flash, bits[6:1]=6-bit value, bit0=unknown.
static void build_e908(const MagicbandCfg* c, uint8_t out[12]) {
    out[0] = 0xE1;
    out[1] = 0x00;
    out[2] = 0xE9;
    out[3] = 0x08;
    out[4] = 0x00;
    out[5] = 0x0E;
    out[6] = 0xD2;
    out[7] = 0x55;
    out[8] = (uint8_t)((c->r6 & 0x3F) << 1);
    out[9] = (uint8_t)((c->g6 & 0x3F) << 1);
    out[10] = (uint8_t)((c->b6 & 0x3F) << 1);
    out[11] = c->vibe ? (uint8_t)((c->vibe << 4) | 0x0B) : 0xB0;
}

// E9-09: 5 LEDs with individual 5-bit colors.
//   Each color byte: bits[7:5]=101b (0xA0), bits[4:0]=color5.
static void build_e909(const MagicbandCfg* c, uint8_t out[13]) {
    out[0] = 0xE1;
    out[1] = 0x00;
    out[2] = 0xE9;
    out[3] = 0x09;
    out[4] = 0x00;
    out[5] = 0x0E;
    out[6] = 0x0F;
    for(int i = 0; i < 5; i++)
        out[7 + i] = (uint8_t)(0xA0 | (c->led5[i] & 0x1F));
    out[12] = c->vibe ? (uint8_t)((c->vibe << 4) | 0x0B) : 0xB0;
}

// ─── Example preset payloads ─────────────────────────────────────────────────

typedef struct {
    const char* name;
    const uint8_t* data;
    size_t len;
} Entry;

// CC codes
static const uint8_t CC_0[] = {0xE1, 0x00, 0xCC, 0x03, 0x00, 0x00, 0x00};
static const uint8_t CC_1[] = {0xE1, 0x00, 0xCC, 0x03, 0x00, 0x01, 0x00};
static const uint8_t CC_2[] = {0xE1, 0x00, 0xCC, 0x03, 0x13, 0x20, 0x00};
static const Entry CAT_CC[] = {
    {"Ping (cc030000)", CC_0, sizeof(CC_0)},
    {"Fast Mode Ping", CC_1, sizeof(CC_1)},
    {"RRC (cc03132000)", CC_2, sizeof(CC_2)},
};

// E9-07 unknown
static const uint8_t E907_0[] = {0xE2, 0x00, 0xE9, 0x07, 0x00, 0x22, 0x0F, 0x43, 0x43, 0x41, 0xB0};
static const uint8_t E907_1[] = {0xE2, 0x00, 0xE9, 0x07, 0x00, 0x22, 0x0F, 0x44, 0x44, 0x41, 0xB0};
static const Entry CAT_E907[] = {
    {"Unknown #1", E907_0, sizeof(E907_0)},
    {"Unknown #2", E907_1, sizeof(E907_1)},
};

// E9-0B circle
static const uint8_t E90B_0[] =
    {0xE1, 0x00, 0xE9, 0x0B, 0x0B, 0x0F, 0x0F, 0x5C, 0x5D, 0x48, 0xA5, 0xD1, 0x45, 0x32, 0x05};
static const Entry CAT_E90B[] = {{"Circle", E90B_0, sizeof(E90B_0)}};

// E9-0C animations
static const uint8_t E90C_0[] =
    {0xE1, 0x00, 0xE9, 0x0C, 0x00, 0x0F, 0x0F, 0x5D, 0x46, 0x5B, 0xF0, 0x05, 0x32, 0x37, 0x48, 0x95};
static const uint8_t E90C_1[] =
    {0xE1, 0x00, 0xE9, 0x0C, 0x00, 0xEF, 0x0F, 0x4F, 0x4F, 0x5B, 0xF0, 0xFB, 0x14, 0x37, 0x48, 0x95};
static const uint8_t E90C_2[] =
    {0xE1, 0x00, 0xE9, 0x0C, 0x00, 0x0F, 0x0F, 0xB1, 0xB9, 0xB5, 0xB1, 0xA2, 0x30, 0x7B, 0x7D, 0xB0};
static const uint8_t E90C_3[] =
    {0xE1, 0x00, 0xE9, 0x0C, 0x00, 0x0F, 0x0F, 0x5D, 0x46, 0x5B, 0xF0, 0x05, 0x32, 0x37, 0x48, 0xB0};
static const Entry CAT_E90C[] = {
    {"Blink White", E90C_0, sizeof(E90C_0)},
    {"Orange Blink", E90C_1, sizeof(E90C_1)},
    {"Color Cycle", E90C_2, sizeof(E90C_2)},
    {"Rainbow", E90C_3, sizeof(E90C_3)},
};

// E9-0E
static const uint8_t E90E_0[] = {
    0xE1,
    0x00,
    0xE9,
    0x0E,
    0x00,
    0x01,
    0x0F,
    0xBD,
    0xA0,
    0xA0,
    0xBD,
    0xA0,
    0x59,
    0x07,
    0x00,
    0x48,
    0xAE,
    0xB5};
static const uint8_t E90E_1[] = {
    0xE1,
    0x00,
    0xE9,
    0x0E,
    0x00,
    0x02,
    0x0F,
    0xBC,
    0xA0,
    0xBC,
    0xA0,
    0xBC,
    0x59,
    0x17,
    0xFB,
    0x48,
    0xAE,
    0xBB};
static const uint8_t E90E_2[] = {
    0xE1,
    0x00,
    0xE9,
    0x0E,
    0x00,
    0x11,
    0x0F,
    0xBC,
    0xA7,
    0xB9,
    0xA7,
    0xB9,
    0x59,
    0x19,
    0x02,
    0x48,
    0xAE,
    0xB0};
static const uint8_t E90E_3[] = {
    0xE1,
    0x00,
    0xE9,
    0x0E,
    0x00,
    0x15,
    0x0F,
    0xBB,
    0xBB,
    0xBB,
    0xBB,
    0x59,
    0x19,
    0x02,
    0x48,
    0xAE,
    0xB0};
static const uint8_t E90E_4[] = {
    0xE1,
    0x00,
    0xE9,
    0x0E,
    0x00,
    0x83,
    0x0F,
    0xB5,
    0xB9,
    0xB2,
    0xAD,
    0xB6,
    0x59,
    0x19,
    0x0B,
    0x48,
    0xAE,
    0xB0};
static const Entry CAT_E90E[] = {
    {"E9-0E #1", E90E_0, sizeof(E90E_0)},
    {"E9-0E #2", E90E_1, sizeof(E90E_1)},
    {"E9-0E #3", E90E_2, sizeof(E90E_2)},
    {"E9-0E #4", E90E_3, sizeof(E90E_3)},
    {"E9-0E #5", E90E_4, sizeof(E90E_4)},
};

// E9-0F
static const uint8_t E90F_0[] = {
    0xE1,
    0x00,
    0xE9,
    0x0F,
    0x00,
    0x11,
    0x0F,
    0x4F,
    0x42,
    0x58,
    0x07,
    0x48,
    0x8D,
    0xD2,
    0x46,
    0x2A,
    0x07,
    0x17,
    0xB8};
static const uint8_t E90F_1[] = {
    0xE1,
    0x00,
    0xE9,
    0x0F,
    0x00,
    0x2A,
    0x0F,
    0x46,
    0x43,
    0x58,
    0x12,
    0x48,
    0x8D,
    0xD2,
    0x46,
    0x02,
    0x12,
    0x00,
    0xB0};
static const Entry CAT_E90F[] = {
    {"E9-0F #1", E90F_0, sizeof(E90F_0)},
    {"E9-0F #2", E90F_1, sizeof(E90F_1)}};

// E9-10
static const uint8_t E910_0[] = {0xE1, 0x00, 0xE9, 0x10, 0x00, 0x13, 0x48, 0x97, 0xD0, 0x0E,
                                 0xA0, 0xD1, 0x46, 0x06, 0x0F, 0x30, 0xD0, 0x4E, 0x07, 0xB0};
static const Entry CAT_E910[] = {{"Alternating", E910_0, sizeof(E910_0)}};

// E9-11 cross fade
static const uint8_t E911_0[] = {0xE1, 0x00, 0xE9, 0x11, 0x00, 0x6F, 0x0F, 0x56, 0x48, 0x58, 0xF4,
                                 0x48, 0x82, 0xD1, 0x46, 0x02, 0x08, 0xD0, 0x65, 0x00, 0xB0};
static const uint8_t E911_1[] = {0xE2, 0x00, 0xE9, 0x11, 0x00, 0x4F, 0x0F, 0x44, 0x4F, 0x58, 0xF4,
                                 0x48, 0x82, 0xD1, 0x46, 0x06, 0x07, 0xD0, 0x65, 0x43, 0xB0};
static const uint8_t E911_2[] = {0xE1, 0x00, 0xE9, 0x11, 0x00, 0x0F, 0x0F, 0x48, 0x59, 0x58, 0xF4,
                                 0x48, 0x82, 0xD1, 0x46, 0x02, 0x0D, 0xD0, 0x65, 0x05, 0xB0};
static const uint8_t E911_3[] = {0xE2, 0x00, 0xE9, 0x11, 0x00, 0x4F, 0x0F, 0x4F, 0x55, 0x58, 0xF4,
                                 0x48, 0x82, 0xD1, 0x46, 0x02, 0x2A, 0xD0, 0x65, 0x01, 0xB0};
static const uint8_t E911_4[] = {0xE1, 0x00, 0xE9, 0x11, 0x00, 0x01, 0x0F, 0x5A, 0x47, 0x5B, 0xF0,
                                 0x31, 0x34, 0x37, 0x48, 0x94, 0xD1, 0x3D, 0x05, 0x07, 0xB0};
static const uint8_t E911_5[] = {0xE1, 0x00, 0xE9, 0x11, 0x00, 0x07, 0x0F, 0x55, 0x5D, 0x58, 0xF4,
                                 0x48, 0x82, 0xD1, 0x46, 0x05, 0x08, 0xD0, 0x65, 0x00, 0xB0};
static const uint8_t E911_6[] = {0xE1, 0x00, 0xE9, 0x11, 0x00, 0x44, 0x0F, 0x51, 0x42, 0x58, 0xF4,
                                 0x48, 0x82, 0xD1, 0x46, 0x05, 0x0F, 0xD0, 0x65, 0x00, 0xB0};
static const Entry CAT_E911[] = {
    {"Cross Fade #1", E911_0, sizeof(E911_0)},
    {"Cross Fade #2", E911_1, sizeof(E911_1)},
    {"Cross Fade #3", E911_2, sizeof(E911_2)},
    {"Cross Fade #4", E911_3, sizeof(E911_3)},
    {"Cross Fade #5", E911_4, sizeof(E911_4)},
    {"Cross Fade #6", E911_5, sizeof(E911_5)},
    {"Cross Fade #7", E911_6, sizeof(E911_6)},
};

// E9-12 circle+vibe
static const uint8_t E912_0[] = {0xE1, 0x00, 0xE9, 0x12, 0x00, 0x01, 0x0F, 0xBC, 0xBD, 0xBD, 0xBD,
                                 0xBD, 0x30, 0xD0, 0x37, 0xF4, 0xD2, 0x46, 0x00, 0x00, 0xFC, 0xBB};
static const uint8_t E912_1[] = {0xE1, 0x00, 0xE9, 0x12, 0x00, 0x01, 0x29, 0x04, 0x02, 0x02, 0x11,
                                 0x11, 0x48, 0x96, 0xD0, 0x0E, 0xFF, 0xD1, 0x46, 0x07, 0x07, 0xB0};
static const uint8_t E912_2[] = {0xE2, 0x00, 0xE9, 0x12, 0x00, 0x03, 0x0F, 0xA2, 0xA2, 0xA4, 0xA4,
                                 0xA2, 0x30, 0xD0, 0x37, 0xF4, 0xD2, 0x46, 0x00, 0x64, 0xFC, 0xB0};
static const Entry CAT_E912[] = {
    {"Circle+Vibe #1", E912_0, sizeof(E912_0)},
    {"Circle+Vibe #2", E912_1, sizeof(E912_1)},
    {"Circle+Vibe #3", E912_2, sizeof(E912_2)},
};

// E9-13
static const uint8_t E913_0[] = {0xE1, 0x00, 0xE9, 0x13, 0x00, 0x02, 0xD0, 0x37,
                                 0xF0, 0xD2, 0x3D, 0x05, 0x05, 0x00, 0x0E, 0xFA,
                                 0x89, 0x83, 0x51, 0x0E, 0xE7, 0xA0, 0xB0};
static const uint8_t E913_1[] = {0xE2, 0x00, 0xE9, 0x13, 0x00, 0x65, 0x0F, 0xBD,
                                 0xB5, 0xBC, 0xB5, 0xBC, 0x7A, 0xEC, 0x5C, 0x0A,
                                 0x29, 0x15, 0x29, 0x15, 0x48, 0xAB, 0xB0};
static const Entry CAT_E913[] = {
    {"Anim 13 #1", E913_0, sizeof(E913_0)},
    {"Anim 13 #2", E913_1, sizeof(E913_1)}};

// E9-14
static const uint8_t E914_0[] = {0xE1, 0x00, 0xE9, 0x14, 0x00, 0x0C, 0xD0, 0x37,
                                 0xF0, 0xD2, 0x3D, 0x05, 0x0C, 0x0C, 0x0E, 0xEC,
                                 0x89, 0x83, 0x51, 0x0E, 0xEE, 0x0C, 0x3D, 0xB0};
static const uint8_t E914_1[] = {0xE2, 0x00, 0xE9, 0x14, 0x00, 0x2C, 0xD0, 0x37,
                                 0xF0, 0xD2, 0x3D, 0x02, 0x12, 0x00, 0x0E, 0xEA,
                                 0x89, 0x83, 0x51, 0x0E, 0xE3, 0x0C, 0x1E, 0xB0};
static const uint8_t E914_2[] = {0xE2, 0x00, 0xE9, 0x14, 0x00, 0x42, 0x0F, 0x55,
                                 0x5B, 0x58, 0xF4, 0x48, 0x82, 0xD0, 0x65, 0x1B,
                                 0xD1, 0x46, 0x2A, 0x02, 0x30, 0x7B, 0x5D, 0xB0};
static const Entry CAT_E914[] = {
    {"Anim 14 #1", E914_0, sizeof(E914_0)},
    {"Anim 14 #2", E914_1, sizeof(E914_1)},
    {"Anim 14 #3", E914_2, sizeof(E914_2)},
};

// ─── Protocol implementation ─────────────────────────────────────────────────

static const char* get_name(const Payload* payload) {
    const MagicbandCfg* c = &payload->cfg.magicband;
    switch(c->category) {
    case MB_Cat_E905_Single:
        return mb_color_name(c->color5);
    case MB_Cat_E908_RGB:
        return "RGB Custom";
    case MB_Cat_Custom:
        return "Custom Hex";
    default:
        return "MagicBand+";
    }
}

static void make_packet(uint8_t* _size, uint8_t** _packet, Payload* payload) {
    MagicbandCfg* c = &payload->cfg.magicband;
    uint8_t adv[31];
    size_t adv_len = 0;

#define DISPATCH(ARR)                                            \
    do {                                                         \
        uint8_t i = c->index % (sizeof(ARR) / sizeof((ARR)[0])); \
        adv_len = build_adv((ARR)[i].data, (ARR)[i].len, adv);   \
    } while(0)

    switch(c->category) {
    case MB_Cat_E905_Single: {
        uint8_t b[9];
        build_e905(c, b);
        adv_len = build_adv(b, 9, adv);
        break;
    }
    case MB_Cat_E906_Dual: {
        uint8_t b[10];
        build_e906(c, b);
        adv_len = build_adv(b, 10, adv);
        break;
    }
    case MB_Cat_E908_RGB: {
        uint8_t b[12];
        build_e908(c, b);
        adv_len = build_adv(b, 12, adv);
        break;
    }
    case MB_Cat_E909_5LED: {
        uint8_t b[13];
        build_e909(c, b);
        adv_len = build_adv(b, 13, adv);
        break;
    }
    case MB_Cat_Custom:
        if(c->custom_len > 0) adv_len = build_adv(c->custom, c->custom_len, adv);
        break;
    default:
    case MB_Cat_CC:
        DISPATCH(CAT_CC);
        break;
    case MB_Cat_E907_Unknown:
        DISPATCH(CAT_E907);
        break;
    case MB_Cat_E90B_Circle:
        DISPATCH(CAT_E90B);
        break;
    case MB_Cat_E90C_Anim:
        DISPATCH(CAT_E90C);
        break;
    case MB_Cat_E90E:
        DISPATCH(CAT_E90E);
        break;
    case MB_Cat_E90F:
        DISPATCH(CAT_E90F);
        break;
    case MB_Cat_E910_Alt:
        DISPATCH(CAT_E910);
        break;
    case MB_Cat_E911_Crossfade:
        DISPATCH(CAT_E911);
        break;
    case MB_Cat_E912_CircVibe:
        DISPATCH(CAT_E912);
        break;
    case MB_Cat_E913_Anim:
        DISPATCH(CAT_E913);
        break;
    case MB_Cat_E914_Anim:
        DISPATCH(CAT_E914);
        break;
    }
#undef DISPATCH

    *_size = (uint8_t)adv_len;
    *_packet = malloc(adv_len);
    memcpy(*_packet, adv, adv_len);
}

// ─── Config UI (hold OK on any attack) ───────────────────────────────────────

static uint8_t vibe_idx(uint8_t v) {
    for(uint8_t i = 0; i < VIBE_COUNT; i++)
        if(VIBES[i].val == v) return i;
    return 0;
}
static uint8_t timing_idx(uint8_t v) {
    for(uint8_t i = 0; i < TIMING_COUNT; i++)
        if(TIMING_VALS[i] == v) return i;
    return 0;
}

// Per-item value-change callbacks
static void on_color5(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    c->color5 = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, COLORS[c->color5 & 0x1F]);
}
static void on_vibe(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    c->vibe = VIBES[i].val;
    variable_item_set_current_value_text(item, VIBES[i].name);
}
static void on_mask(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    c->mask = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, MASKS[c->mask]);
}
static void on_timing(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    c->timing = TIMING_VALS[i];
    variable_item_set_current_value_text(item, TIMING_NAMES[i]);
}
static void on_outer(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    c->color5_outer = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, COLORS[c->color5_outer & 0x1F]);
}
static void on_inner(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    c->color5_inner = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, COLORS[c->color5_inner & 0x1F]);
}
static void on_r6(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    c->r6 = variable_item_get_current_value_index(item);
    static char buf[4];
    snprintf(buf, sizeof(buf), "%u", c->r6);
    variable_item_set_current_value_text(item, buf);
}
static void on_g6(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    c->g6 = variable_item_get_current_value_index(item);
    static char buf[4];
    snprintf(buf, sizeof(buf), "%u", c->g6);
    variable_item_set_current_value_text(item, buf);
}
static void on_b6(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    c->b6 = variable_item_get_current_value_index(item);
    static char buf[4];
    snprintf(buf, sizeof(buf), "%u", c->b6);
    variable_item_set_current_value_text(item, buf);
}

// E9-09: per-LED context
typedef struct {
    MagicbandCfg* c;
    uint8_t idx;
} LEDCtx;
static LEDCtx led_ctx[5];

static void on_variant(VariableItem* item) {
    MagicbandCfg* c = variable_item_get_context(item);
    c->index = variable_item_get_current_value_index(item);
    char buf[4];
    snprintf(buf, sizeof(buf), "%u", c->index + 1);
    variable_item_set_current_value_text(item, buf);
}

typedef struct {
    MagicbandCategory cat;
    uint8_t count;
    const char* label;
} VariantInfo;

static const VariantInfo VARIANTS[] = {
    {MB_Cat_CC, 3, "Code"},
    {MB_Cat_E90C_Anim, 4, "Animation"},
    {MB_Cat_E90E, 5, "Pattern"},
    {MB_Cat_E90F, 2, "Pattern"},
    {MB_Cat_E911_Crossfade, 7, "Fade"},
    {MB_Cat_E912_CircVibe, 3, "Variant"},
    {MB_Cat_E913_Anim, 2, "Animation"},
    {MB_Cat_E914_Anim, 3, "Animation"},
    {MB_Cat_E907_Unknown, 2, "Variant"},
};
#define VARIANTS_COUNT ((uint8_t)(sizeof(VARIANTS) / sizeof(VARIANTS[0])))

static void on_led_color(VariableItem* item) {
    LEDCtx* lc = variable_item_get_context(item);
    lc->c->led5[lc->idx] = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, COLORS[lc->c->led5[lc->idx] & 0x1F]);
}

// Helper to add a VIL item
static void add_item(
    VariableItemList* list,
    const char* label,
    uint8_t count,
    VariableItemChangeCallback cb,
    void* ctx,
    uint8_t cur_idx,
    const char* cur_text) {
    VariableItem* it = variable_item_list_add(list, label, count, cb, ctx);
    variable_item_set_current_value_index(it, cur_idx);
    variable_item_set_current_value_text(it, cur_text);
}

static void extra_config(Ctx* ctx) {
    MagicbandCfg* c = &ctx->attack->payload.cfg.magicband;
    VariableItemList* l = ctx->variable_item_list;
    char buf[8];

    switch(c->category) {
    case MB_Cat_E905_Single:
        add_item(l, "Color", 32, on_color5, c, c->color5 & 0x1F, COLORS[c->color5 & 0x1F]);
        add_item(l, "Vibrate", VIBE_COUNT, on_vibe, c, vibe_idx(c->vibe), mb_vibe_name(c->vibe));
        add_item(
            l,
            "Mask",
            MASK_COUNT,
            on_mask,
            c,
            c->mask < MASK_COUNT ? c->mask : 0,
            MASKS[c->mask < MASK_COUNT ? c->mask : 0]);
        add_item(
            l,
            "Duration",
            TIMING_COUNT,
            on_timing,
            c,
            timing_idx(c->timing),
            TIMING_NAMES[timing_idx(c->timing)]);
        break;

    case MB_Cat_E906_Dual:
        add_item(
            l, "Outer", 32, on_outer, c, c->color5_outer & 0x1F, COLORS[c->color5_outer & 0x1F]);
        add_item(
            l, "Inner", 32, on_inner, c, c->color5_inner & 0x1F, COLORS[c->color5_inner & 0x1F]);
        add_item(l, "Vibrate", VIBE_COUNT, on_vibe, c, vibe_idx(c->vibe), mb_vibe_name(c->vibe));
        break;

    case MB_Cat_E908_RGB:
        snprintf(buf, sizeof(buf), "%u", c->r6);
        add_item(l, "Red  (0-63)", 64, on_r6, c, c->r6, buf);
        snprintf(buf, sizeof(buf), "%u", c->g6);
        add_item(l, "Green (0-63)", 64, on_g6, c, c->g6, buf);
        snprintf(buf, sizeof(buf), "%u", c->b6);
        add_item(l, "Blue  (0-63)", 64, on_b6, c, c->b6, buf);
        add_item(l, "Vibrate", VIBE_COUNT, on_vibe, c, vibe_idx(c->vibe), mb_vibe_name(c->vibe));
        break;

    case MB_Cat_E909_5LED:
        for(int i = 0; i < 5; i++) {
            led_ctx[i].c = c;
            led_ctx[i].idx = i;
            add_item(
                l,
                LED_LABELS[i],
                32,
                on_led_color,
                &led_ctx[i],
                c->led5[i] & 0x1F,
                COLORS[c->led5[i] & 0x1F]);
        }
        add_item(l, "Vibrate", VIBE_COUNT, on_vibe, c, vibe_idx(c->vibe), mb_vibe_name(c->vibe));
        break;

    default: {
        for(uint8_t i = 0; i < VARIANTS_COUNT; i++) {
            if(VARIANTS[i].cat == c->category && VARIANTS[i].count > 1) {
                char buf[4];
                snprintf(buf, sizeof(buf), "%u", (c->index % VARIANTS[i].count) + 1);
                add_item(
                    l,
                    VARIANTS[i].label,
                    VARIANTS[i].count,
                    on_variant,
                    c,
                    c->index % VARIANTS[i].count,
                    buf);
                break;
            }
        }
        break;
    }
    }
}

static uint8_t config_count(const Payload* payload) {
    switch(payload->cfg.magicband.category) {
    case MB_Cat_E905_Single:
        return 4;
    case MB_Cat_E906_Dual:
        return 3;
    case MB_Cat_E908_RGB:
        return 4;
    case MB_Cat_E909_5LED:
        return 6;
    default:
        return 0;
    }
}

const Protocol protocol_magicband = {
    .icon = &I_ble,
    .get_name = get_name,
    .make_packet = make_packet,
    .extra_config = extra_config,
    .config_count = config_count,
};
