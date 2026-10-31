#pragma once
#include "Platform.h"
#include <stdint.h>
#include "Galaxy.h"

// Day 4: commodity market.
//
// Seventeen Elite-style trade goods. Prices and available quantities are
// deterministic functions of (system index, commodity), so each system
// always quotes the same numbers until the player jumps and a new
// per-jump salt re-rolls them (wired in Day 5).
//
// Prices are stored in tenths of a credit (so 45 means 4.5 CR).

namespace Market {

enum Commodity : uint8_t {
    Food = 0,
    Textiles,
    Radioactv,
    Slaves,
    LiqWines,
    Luxuries,
    Narcotics,
    Computers,
    Machinery,
    Alloys,
    Firearms,
    Furs,
    Minerals,
    Gold,
    Platinum,
    GemStones,
    Aliens,
    N
};

struct Item {
    const char* name;
    const char* unit; // "t", "kg", "g"
    uint16_t basePrice; // in 0.1 CR
    int8_t agriBias; // -ve: cheaper in agri systems (food/textiles)
        // +ve: cheaper in industrial (machinery/computers)
    uint8_t baseQty; // average tons available
    bool illegal; // shrinks supply in stable governments
};

static const Item items[N] = {
    // name         unit  base  agriBias  qty  illegal
    {"FOOD", "t", 45, -8, 22, false},
    {"TEXTILES", "t", 75, -6, 18, false},
    {"RADIOACTV", "t", 320, +1, 8, false},
    {"SLAVES", "t", 160, -4, 6, true},
    {"LIQ/WINES", "t", 155, -3, 12, false},
    {"LUXURIES", "t", 220, +5, 9, false},
    {"NARCOTICS", "t", 410, -2, 6, true},
    {"COMPUTERS", "t", 370, +7, 10, false},
    {"MACHINERY", "t", 280, +6, 12, false},
    {"ALLOYS", "t", 195, +3, 14, false},
    {"FIREARMS", "t", 330, +4, 7, true},
    {"FURS", "t", 195, -3, 8, false},
    {"MINERALS", "t", 85, +2, 16, false},
    {"GOLD", "kg", 400, 0, 4, false},
    {"PLATINUM", "kg", 650, +1, 3, false},
    {"GEMSTONES", "g", 180, -1, 5, false},
    {"ALIENS", "t", 530, 0, 2, false},
};

// 0..3 = industrial, 4..7 = agricultural (matches Galaxy::Economy ordering).
inline bool isAgricultural(Galaxy::Economy e) {
    return (uint8_t)e >= 4;
}

// 0..3 inclusive = more stable / law-abiding (Confed/Democracy/Corporate)
inline bool isStableGov(Galaxy::Government g) {
    return (uint8_t)g >= 5;
}

inline int agriIndustrialStrength(Galaxy::Economy e) {
    // Returns -3..+3. Negative = strongly industrial, positive = strongly agri.
    static const int8_t map[8] = {
        -3,
        -2,
        -1,
        -1, // industrial group (rich..mainly)
        +1,
        +3,
        +2,
        +1, // agricultural group (mainly..poor)
    };
    return map[(uint8_t)e];
}

// R22: price scaled by the player's standing with the system's faction.
// `scalePermille` is 1000 for neutral; <1000 = friend discount, >1000 =
// hostile markup. Callers compute it via Faction::priceScalePermille().
inline uint16_t priceWithScale(int sysIdx, int c, int scalePermille);

inline uint16_t priceAt(int sysIdx, int c) {
    const auto& sys = Galaxy::systems[sysIdx];
    const Item& it = items[c];

    // agriBias > 0  -> commodity is cheap on industrial worlds, dear on agri
    // agriBias < 0  -> commodity is cheap on agri worlds, dear on industrial
    int bias = it.agriBias * agriIndustrialStrength(sys.economy);
    int p = (int)it.basePrice + bias * 4;

    // Per-system noise. The marketEpoch shifts every hyperspace jump so
    // returning to the same system later quotes slightly different prices.
    uint32_t s = Galaxy::systemSubSeed(
        sysIdx, 0x4D5A0000u + (uint32_t)c + Galaxy::marketEpoch * 0x9E3779B9u);
    int noise = (int)(Galaxy::lcg(s) % 21) - 10; // -10 .. +10
    p += noise;

    if(p < 1) p = 1;
    if(p > 9999) p = 9999;
    return (uint16_t)p;
}

inline uint16_t priceWithScale(int sysIdx, int c, int scalePermille) {
    int base = (int)priceAt(sysIdx, c);
    int p = (base * scalePermille + 500) / 1000;
    if(p < 1) p = 1;
    if(p > 9999) p = 9999;
    return (uint16_t)p;
}

inline uint8_t qtyAt(int sysIdx, int c) {
    const auto& sys = Galaxy::systems[sysIdx];
    const Item& it = items[c];

    int q = (int)it.baseQty;
    // Agri stuff is plentiful on agri worlds, scarce on industrial; mirror.
    q -= it.agriBias * agriIndustrialStrength(sys.economy);

    uint32_t s =
        Galaxy::systemSubSeed(sysIdx, 0xC0FFEEu + (uint32_t)c + Galaxy::marketEpoch * 0xDEADBEEFu);
    int n = (int)(Galaxy::lcg(s) % 9) - 4; // -4 .. +4
    q += n;

    if(it.illegal && isStableGov(sys.government)) {
        q = q / 2 - 2; // hard to find under stable government
    }
    if(q < 0) q = 0;
    if(q > 99) q = 99;
    return (uint8_t)q;
}

// R31: per-planet stock. Each planet carries a deterministic subset of
// the system market — roughly one commodity in four simply isn't sold at
// a given world, and the rest scale 50..150% of the system baseline.
// This is what makes "buy it at the other planet" quests meaningful.
inline uint8_t qtyAtPlanet(int sysIdx, int planetPOI, int c) {
    if(planetPOI < 0) return qtyAt(sysIdx, c);
    int q = (int)qtyAt(sysIdx, c);
    uint32_t s = Galaxy::systemSubSeed(
        sysIdx,
        0x504C41u + (uint32_t)c * 53u + (uint32_t)planetPOI * 0x9E3779B9u +
            Galaxy::marketEpoch * 0x85EBCA6Bu);
    uint32_t r = Galaxy::lcg(s);
    if((r & 3u) == 0u) return 0; // this world doesn't carry it
    int scale = 50 + (int)((r >> 8) % 101u); // 50..150%
    q = q * scale / 100;
    if(q < 0) q = 0;
    if(q > 99) q = 99;
    return (uint8_t)q;
}

inline const Item& itemAt(int c) {
    return items[c];
}

} // namespace Market
