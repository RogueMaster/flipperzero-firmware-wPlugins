#pragma once
#include "Platform.h"
#include <stdint.h>
#include <string.h>
#include "Galaxy.h"

// Refit R10: per-system point-of-interest table.
//
// A solar system is a free-flight volume containing a star, one to three
// planets (each possibly ringed, with 0..3 moonlets and an optional belt),
// one Coriolis station, and one outbound jump gate. POIs are laid out in a
// large bounded cube measured in "system units" (sysu) — pick units later
// so 1 sysu ≈ 1 km feels right; the renderer (R11) just scales to screen.
//
// Per-system layout is deterministic: re-derived from
// Galaxy::systemSubSeed(idx, salt) on demand, so we never store all 16
// layouts in RAM at once — only the active system.

namespace SolarSystem {

using Galaxy::lcg; // shared LCG; defined in Galaxy.h.

constexpr int MaxPOIs = 12; // 1 star + 3 planets + 1 station + 1 gate + slop
constexpr int MaxPlanets = 3;
// Radius of the system's zone around the star, sysu. Space itself is
// open — this is where the system "ends": the map is drawn to it, and
// past it the cockpit treats the player as being in deep space. Leaves
// a good margin past the outermost body (~22.3K) and the gate (~20.9K).
constexpr float ZoneRadius = 30000.0f;

enum class POIType : uint8_t {
    Star = 0,
    Planet,
    Station,
    JumpGate,
};

// POI flag bits.
constexpr uint8_t PoiFlagRing = 0x01;
constexpr uint8_t PoiFlagBelt = 0x02;
constexpr uint8_t PoiFlagMoonMask = 0x0C; // bits 2-3 hold moonlet count 0..3
constexpr uint8_t PoiFlagMoonShift = 2;

struct POI {
    POIType type;
    uint8_t subIdx; // for Planet: which planet (0..MaxPlanets-1); else 0
    int16_t x, y, z; // system-space position, sysu
    uint16_t radius; // body radius in sysu (or visual size for station/gate)
    uint8_t flags; // see PoiFlag* (only meaningful for Planet)
    uint8_t _pad;
};

struct Layout {
    uint8_t numPOIs;
    POI poi[MaxPOIs];
};

// ---------- Helpers ----------

// Map a uniform LCG draw to [-half, +half].
inline int16_t signedRange(uint32_t r, int16_t half) {
    return (int16_t)((int32_t)(r % (2u * (uint32_t)half + 1u)) - (int32_t)half);
}

// Place a planet in an orbit at the given radius. Angle is chosen from the
// LCG; the vertical offset scales with orbit radius so outer planets can
// sit well above/below the ecliptic — keeps the system genuinely 3D
// instead of pancaked onto a flat XZ disc.
inline void placeOnOrbit(uint32_t& s, int16_t orbitR, int16_t& x, int16_t& y, int16_t& z) {
    uint32_t a = lcg(s);
    // 0..2π in 65536 steps; small float math is OK here, only runs once per
    // system entry.
    float ang = (float)(a & 0xFFFFu) * (6.2831853f / 65536.0f);
    x = (int16_t)(cosf(ang) * (float)orbitR);
    z = (int16_t)(sinf(ang) * (float)orbitR);
    // y is up to ±35% of the orbit radius — enough that planets noticeably
    // stack above/below each other when viewed from the side.
    int32_t yRange = (int32_t)orbitR * 35 / 100;
    if(yRange < 1) yRange = 1;
    int32_t yJitter = (int32_t)(lcg(s) % (uint32_t)(2 * yRange + 1)) - yRange;
    y = (int16_t)yJitter;
}

// ---------- Layout generation ----------

// Fill `out` with the POI table for system `idx`. Pure function of idx +
// the global galaxy seed (via Galaxy::systemSubSeed), so the same system
// always lays out the same way.
inline void layoutFor(int idx, Layout& out) {
    out.numPOIs = 0;

    // --- Star at origin. Body radius is kept small enough that the heat
    // band (star.r + SystemFlight::SunHeatBuffer = 4000 sysu) clears the
    // innermost planet orbit at 8000 sysu — so a planet visit is never
    // automatically a sun-burn.
    {
        POI& p = out.poi[out.numPOIs++];
        p.type = POIType::Star;
        p.subIdx = 0;
        p.x = p.y = p.z = 0;
        uint32_t sr = Galaxy::systemSubSeed(idx, 0x52A1u);
        p.radius = (uint16_t)(1600 + (lcg(sr) % 1400)); // 1600..2999 sysu
        p.flags = 0;
        p._pad = 0;
    }

    // --- Planets. Count is 1..3, drawn from a planet-specific seed stream.
    uint32_t s = Galaxy::systemSubSeed(idx, 0x504Cu);
    int numPlanets = 1 + (int)(lcg(s) % MaxPlanets); // 1..3

    // Pre-chosen orbit radii so planets don't collide. Innermost sits
    // outside the star's heat band; outer two stay well inside ZoneRadius.
    const int16_t orbits[MaxPlanets] = {8000, 13500, 19500};

    int firstPlanetPOI = -1; // index into out.poi[] of planet 0 (used to
        // anchor the outbound jump gate on the
        // opposite side of the star).
    for(int i = 0; i < numPlanets; i++) {
        POI& p = out.poi[out.numPOIs++];
        if(i == 0) firstPlanetPOI = out.numPOIs - 1;
        p.type = POIType::Planet;
        p.subIdx = (uint8_t)i;
        placeOnOrbit(s, orbits[i], p.x, p.y, p.z);
        // Planet radius 600..1400 sysu, with the outer planets running a bit
        // larger on average — gas-giant flavor.
        uint32_t r = lcg(s);
        p.radius = (uint16_t)(600 + (r % 801) + (uint32_t)i * 100u);

        // Flag rolls. Each is independent so we can read intent off the bits.
        uint8_t flags = 0;
        if((lcg(s) & 7u) < 2u) flags |= PoiFlagRing; // ~25% ringed
        if((lcg(s) & 7u) < 3u) flags |= PoiFlagBelt; // ~37% have a belt
        uint8_t moons = (uint8_t)(lcg(s) & 3u); // 0..3 moonlets
        flags |= (uint8_t)(moons << PoiFlagMoonShift) & PoiFlagMoonMask;
        p.flags = flags;
        p._pad = 0;
    }

    // (Orbit stations removed — planet landings handle all docked services.)

    // --- Jump gate: parked far out, near the system bounding edge. Place
    // it on the opposite side of the star from planet 0 so it isn't on top
    // of a populated body.
    {
        int16_t gx = 0, gy = 0, gz = 0;
        if(firstPlanetPOI >= 0) {
            const POI& parent = out.poi[firstPlanetPOI];
            float px = (float)parent.x, pz = (float)parent.z;
            float len = sqrtf(px * px + pz * pz);
            if(len > 0.5f) {
                gx = (int16_t)(-px / len * 20000.0f);
                gz = (int16_t)(-pz / len * 20000.0f);
            } else {
                gx = 20000;
            }
            // Push the gate well off the ecliptic so it isn't visually stacked
            // with the planets when scanning the system from the side.
            int32_t gyRange = 6000;
            gy = (int16_t)((int32_t)(lcg(s) % (uint32_t)(2 * gyRange + 1)) - gyRange);
        } else {
            gx = 20000;
        }
        POI& p = out.poi[out.numPOIs++];
        p.type = POIType::JumpGate;
        p.subIdx = 0;
        p.x = gx;
        p.y = gy;
        p.z = gz;
        p.radius = 300; // visual gate-ring size in sysu
        p.flags = 0;
        p._pad = 0;
    }
}

// ---------- Display ----------

// Build a short human-readable name for a POI. `out` should be at least
// 16 bytes. Uses the system name (Galaxy::systems[idx].name) plus a Roman
// numeral for planets, or a typed suffix for the station / gate.
inline void displayName(int sysIdx, const POI& p, char* out, size_t cap) {
    if(cap == 0) return;
    const char* sys = Galaxy::systems[sysIdx].name;
    switch(p.type) {
    case POIType::Star:
        snprintf(out, cap, "%s", sys);
        break;
    case POIType::Planet: {
        static const char* const roman[MaxPlanets] = {"I", "II", "III"};
        const char* r = (p.subIdx < MaxPlanets) ? roman[p.subIdx] : "?";
        snprintf(out, cap, "%s %s", sys, r);
        break;
    }
    case POIType::Station:
        snprintf(out, cap, "%s ORBITAL", sys);
        break;
    case POIType::JumpGate:
        snprintf(out, cap, "%s GATE", sys);
        break;
    }
}

// Convenience: moonlet count packed into a POI's flag bits.
inline uint8_t moonletCount(const POI& p) {
    return (uint8_t)((p.flags & PoiFlagMoonMask) >> PoiFlagMoonShift);
}

} // namespace SolarSystem
