#pragma once
#include "Platform.h"
#include <stdint.h>
#include <string.h>

// Day 1: procedural galaxy.
//
// One galaxy of 16 systems (refit R10 — down from 64), generated
// deterministically from a single 32-bit seed (LCG, Numerical Recipes
// constants). All names, coords, and economic/political attributes are
// derived from successive LCG draws, so the same seed always reproduces
// the same galaxy on every boot — important for save/load later on.
//
// Each system also has an outbound gate adjacency list (up to MaxGates
// destinations, picked greedily by distance during generate()). Hyperspace
// will follow this graph instead of a pure-distance jump rule — see R15.

namespace Galaxy {

constexpr int NumSystems = 16;
constexpr int MaxGates = 4;
constexpr uint32_t Seed = 0x00ACE1A0u;

enum class Economy : uint8_t {
    RichIndustrial = 0,
    AverageIndustrial,
    PoorIndustrial,
    MainlyIndustrial,
    MainlyAgricultural,
    RichAgricultural,
    AverageAgricultural,
    PoorAgricultural,
};

enum class Government : uint8_t {
    Anarchy = 0,
    Feudal,
    MultiGov,
    Dictatorship,
    Communist,
    Confederacy,
    Democracy,
    CorporateState,
};

struct System {
    char name[10]; // up to 8 chars + null
    uint8_t x, y; // map coordinates 0..255
    Economy economy;
    Government government;
    uint8_t techLevel; // 0..15
    uint16_t population; // billions × 10  (so 47 -> 4.7 B)
    uint16_t productivity; // arbitrary MCr-ish 0..10000
};

// ---------- LCG ----------
inline uint32_t lcg(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

// ---------- Name generator ----------
// Syllable tables sized as powers of two so we can index with a mask.
static const char* const NameStart[32] = {
    "VE", "AR", "TA", "OR", "EN", "RI", "BE", "QU", "ZI", "DO", "GA", "MA", "NA", "PO", "SU", "TI",
    "WE", "XO", "YA", "ZE", "AL", "EL", "IL", "OL", "UL", "BR", "CR", "DR", "FR", "GR", "PR", "TR",
};
static const char* const NameMid[16] = {
    "LA",
    "RA",
    "NA",
    "TA",
    "RO",
    "LO",
    "TI",
    "RI",
    "GA",
    "NO",
    "DO",
    "BA",
    "CO",
    "VE",
    "SA",
    "MO",
};
static const char* const NameEnd[16] = {
    "TH",
    "ON",
    "AN",
    "IS",
    "ES",
    "OR",
    "AR",
    "ER",
    "US",
    "IX",
    "OX",
    "IL",
    "OL",
    "EX",
    "AX",
    "EM",
};

inline void appendChars(char* out, size_t& i, size_t cap, const char* src) {
    for(size_t k = 0; src[k] && i + 1 < cap; k++)
        out[i++] = src[k];
}

constexpr size_t NameCap = 10;

inline void makeName(uint32_t& s, char out[NameCap]) {
    const char* a = NameStart[lcg(s) & 31];
    const char* b = NameMid[lcg(s) & 15];
    const char* c = NameEnd[lcg(s) & 15];
    bool dropMid = (lcg(s) & 3u) == 0; // 1-in-4 names are shorter

    size_t i = 0;
    appendChars(out, i, NameCap, a);
    if(!dropMid) appendChars(out, i, NameCap, b);
    appendChars(out, i, NameCap, c);
    out[i] = '\0';
}

// ---------- Storage ----------
inline System systems[NumSystems];
inline bool generated = false;

// Outbound gate destinations per system, plus a count (≤ MaxGates). Built
// at the end of generate(); a value of 0xFF means "unused slot".
inline uint8_t gates[NumSystems][MaxGates];
inline uint8_t gateCount[NumSystems];

// Bumped by Hyperspace on each successful jump. Used by Market noise so
// the same system quotes different prices each time you come back.
inline uint32_t marketEpoch = 0;

// Forward decls (defined below).
inline float distanceLY(int a, int b);
inline uint32_t systemSubSeed(int idx, uint32_t salt);
inline void buildGateGraph();

inline void generate() {
    uint32_t s = Seed;
    for(int i = 0; i < NumSystems; i++) {
        System& sys = systems[i];
        makeName(s, sys.name);

        sys.x = (uint8_t)(lcg(s) & 0xFFu);
        sys.y = (uint8_t)(lcg(s) & 0xFFu);

        sys.economy = (Economy)(lcg(s) & 7u);
        sys.government = (Government)(lcg(s) & 7u);

        // Tech leans high under stable governments and rich economies.
        int tech = 1 + (int)(lcg(s) % 10);
        tech += (int)sys.government / 2;
        tech -= (int)sys.economy / 2;
        if(tech < 0) tech = 0;
        if(tech > 15) tech = 15;
        sys.techLevel = (uint8_t)tech;

        sys.population = (uint16_t)(10 + (lcg(s) % 90)); // 1.0 .. 9.9 B
        sys.productivity = (uint16_t)((lcg(s) % 9000) + 1000); // 1000 .. 9999
    }
    buildGateGraph();
    generated = true;
}

// ---------- Gate graph ----------
// Each system picks its 2..4 nearest neighbors as bidirectional gate
// links. We do an "or-with-reverse" pass so the graph is symmetric (if A
// points to B, B also lists A). Per-system stream count is 2..4 so the
// galaxy isn't a fully-connected blob.
inline void buildGateGraph() {
    for(int i = 0; i < NumSystems; i++)
        gateCount[i] = 0;

    // Pass 1: each system picks K nearest neighbors.
    for(int i = 0; i < NumSystems; i++) {
        uint32_t s = systemSubSeed(i, 0x6A7Eu);
        int k = 2 + (int)(lcg(s) & 1u); // 2 or 3 outbound
        if(k > MaxGates) k = MaxGates;

        // Selection sort over distances, stopping at k.
        uint8_t picked[MaxGates] = {0xFF, 0xFF, 0xFF, 0xFF};
        float pickedD[MaxGates] = {1e9f, 1e9f, 1e9f, 1e9f};
        for(int j = 0; j < NumSystems; j++) {
            if(j == i) continue;
            float d = distanceLY(i, j);
            // Insert into sorted picked[] if smaller than the worst kept.
            int worst = 0;
            for(int t = 1; t < k; t++)
                if(pickedD[t] > pickedD[worst]) worst = t;
            if(d < pickedD[worst]) {
                pickedD[worst] = d;
                picked[worst] = (uint8_t)j;
            }
        }
        for(int t = 0; t < k; t++) {
            if(picked[t] == 0xFF) continue;
            gates[i][gateCount[i]++] = picked[t];
        }
    }

    // Pass 2: symmetrize. If A→B exists but B→A doesn't, add it.
    for(int i = 0; i < NumSystems; i++) {
        for(int t = 0; t < gateCount[i]; t++) {
            uint8_t j = gates[i][t];
            bool back = false;
            for(int u = 0; u < gateCount[j]; u++) {
                if(gates[j][u] == i) {
                    back = true;
                    break;
                }
            }
            if(!back && gateCount[j] < MaxGates) {
                gates[j][gateCount[j]++] = (uint8_t)i;
            }
        }
    }
}

// True if there's a direct gate link between systems a and b.
inline bool isGateNeighbor(int a, int b) {
    if(a == b) return false;
    for(int t = 0; t < gateCount[a]; t++)
        if(gates[a][t] == b) return true;
    return false;
}

// ---------- Helpers ----------
inline const char* economyName(Economy e) {
    static const char* names[] = {
        "Rich Ind",
        "Avg Ind",
        "Poor Ind",
        "Mainly Ind",
        "Mainly Agr",
        "Rich Agr",
        "Avg Agr",
        "Poor Agr",
    };
    return names[(int)e];
}

inline const char* govName(Government g) {
    static const char* names[] = {
        "Anarchy",
        "Feudal",
        "Multi-Gov",
        "Dictator",
        "Communist",
        "Confed",
        "Democracy",
        "Corporate",
    };
    return names[(int)g];
}

// ---------- Derived per-system data ----------
// These are pure functions of the system index, so we don't need to
// reserve RAM for them: they re-derive from the same LCG sequence at
// any time and never drift.

// Per-system seed (re-mixed from the global seed + index so different
// derived fields can use independent streams without colliding).
inline uint32_t systemSubSeed(int idx, uint32_t salt) {
    return (Seed ^ (salt + 0x9E3779B9u)) + (uint32_t)idx * 2654435761u;
}

// Average radius in kilometers (4000..8999).
inline uint16_t systemRadius(int idx) {
    uint32_t s = systemSubSeed(idx, 0x52A1u);
    return (uint16_t)(4000 + (lcg(s) % 5000));
}

// One-line procedural flavor text built from small word tables. Word
// choices are seeded so the same system always gets the same description.
static const char* const FlavorSubj[4] = {
    "This planet",
    "The world",
    "%s", // %s is replaced with system name
    "This system",
};
static const char* const FlavorVerb[4] = {
    "is known for its",
    "is famous for its",
    "is cursed by its",
    "is plagued by its",
};
static const char* const FlavorAdj[16] = {
    "exotic",
    "ancient",
    "deadly",
    "rare",
    "edible",
    "shy",
    "weird",
    "fragrant",
    "violent",
    "silent",
    "luminous",
    "humble",
    "loyal",
    "fearless",
    "glowing",
    "tiny",
};
static const char* const FlavorNoun[16] = {
    "tree grubs",
    "rock pythons",
    "dust storms",
    "neon forests",
    "lava beaches",
    "crystal flutes",
    "cyber-poets",
    "tin miners",
    "moon harps",
    "sand whales",
    "spice merchants",
    "cave dancers",
    "robot priests",
    "star whisky",
    "honey orchards",
    "ocean lanterns",
};

inline void buildFlavor(int idx, char* out, size_t cap) {
    if(cap == 0) return;
    uint32_t s = systemSubSeed(idx, 0xF1A4u);
    int subjIdx = lcg(s) & 3;
    const char* verb = FlavorVerb[lcg(s) & 3];
    const char* adj = FlavorAdj[lcg(s) & 15];
    const char* noun = FlavorNoun[lcg(s) & 15];

    if(subjIdx == 2) {
        snprintf(out, cap, "%s %s %s %s.", systems[idx].name, verb, adj, noun);
    } else {
        snprintf(out, cap, "%s %s %s %s.", FlavorSubj[subjIdx], verb, adj, noun);
    }
}

// ---------- Distance ----------
// Returned in LY. Tuned so that with 64 systems on a 256x256 map a
// typical neighbor is 2..5 LY away — comfortably within a full 7-LY
// hyperspace tank. Elite's vertical axis is squashed 2:1 (so the map
// looks wider than tall) and we keep that convention.
inline float distanceLY(int a, int b) {
    int dx = (int)systems[a].x - (int)systems[b].x;
    int dy = (int)systems[a].y - (int)systems[b].y;
    float fx = (float)dx;
    float fy = (float)dy * 0.5f;
    return sqrtf(fx * fx + fy * fy) * 0.1f;
}

} // namespace Galaxy
