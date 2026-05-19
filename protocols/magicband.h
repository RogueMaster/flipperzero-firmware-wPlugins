#pragma once
#include "_base.h"

// MagicBand+ BLE protocol - Disney SIG ID 0x0183
// Reference: https://emcot.world/Disney_MagicBand%2B_Bluetooth_Codes

typedef enum {
    MB_Cat_CC = 0,        // cc03 ping / park broadcast codes
    MB_Cat_E905_Single,   // Single color from 5-bit palette  (CONFIGURABLE)
    MB_Cat_E906_Dual,     // Dual color (outer + inner)       (CONFIGURABLE)
    MB_Cat_E907_Unknown,  // Unknown / experimental
    MB_Cat_E908_RGB,      // Single 6-bit RGB color           (CONFIGURABLE)
    MB_Cat_E909_5LED,     // 5 LEDs, per-LED color             (CONFIGURABLE)
    MB_Cat_E90B_Circle,   // Circle animation
    MB_Cat_E90C_Anim,     // Rich animations (blink, rainbow, etc.)
    MB_Cat_E90E,
    MB_Cat_E90F,
    MB_Cat_E910_Alt,      // Alternating colors
    MB_Cat_E911_Crossfade,// Cross fade (center vs outer ring)
    MB_Cat_E912_CircVibe, // Circle + vibration
    MB_Cat_E913_Anim,
    MB_Cat_E914_Anim,
    MB_Cat_Custom,        // Raw hex payload (entered by user) (CONFIGURABLE)
    MB_Cat_COUNT
} MagicbandCategory;

// Vibration: upper nibble of last byte
//  0x0=off  0x1=-  0x2=--  0x3=---  0x4=--*  0x5=----*-
//  0x6=complex  0x7=#(2s)  0x8=rapid  0xA=*(0.5s)  0xB=%(1s)

typedef struct {
    uint8_t  category;      // MagicbandCategory
    uint8_t  index;         // preset index within category

    // --- E9-05 single color ---
    uint8_t  color5;        // 0..31  five-bit color palette
    uint8_t  vibe;          // vibration nibble value (0=off, 0xB=1s pulse, …)
    uint8_t  mask;          // 0=all, 1=top-right, 2=bot-right, 3=bot-left, 4=top-left
    uint8_t  timing;        // 0x8F=always-on, 0x09=~20s, 0x06=~10s, 0x04=~5s

    // --- E9-06 dual color ---
    uint8_t  color5_outer;
    uint8_t  color5_inner;

    // --- E9-08 6-bit RGB ---
    uint8_t  r6;            // 0..63
    uint8_t  g6;
    uint8_t  b6;

    // --- E9-09 5-LED ---
    uint8_t  led5[5];       // per-LED color5: [center, top-r, top-l, bot-r, bot-l]

    // --- Custom raw payload ---
    uint8_t  custom[20];    // manufacturer data after Disney 0x0183 header
    uint8_t  custom_len;    // valid bytes in custom[]
} MagicbandCfg;

// Palette helpers
const char*  mb_color_name(uint8_t color5);
const char*  mb_vibe_name(uint8_t vibe_val);

// Build the CC fast-mode ping (cc03000100) full ADV packet.
// Fills out[]; returns packet length.  out must be >= 16 bytes.
size_t mb_build_fast_ping(uint8_t* out);

extern const Protocol protocol_magicband;
