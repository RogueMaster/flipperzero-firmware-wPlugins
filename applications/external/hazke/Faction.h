#pragma once
#include "Platform.h"
#include <stdint.h>
#include "Galaxy.h"
#include "GameState.h"
#include "NPCShip.h"

// Refit R22: factions and reputation.
//
// Four factions. Every system belongs to one — derived from its
// `Galaxy::Government` so the same seed always maps to the same
// allegiance. Each NPC inherits its home faction (Pirate always Cartel).
//
// Reputation lives on `GameState::standing[4]` (-100..+100) and is
// nudged by kills (big) and by trades (small). Patrols turn hostile
// when their faction's standing drops past `HostileThreshold`, and
// market prices flex up to ±20% with standing. Persistence is still
// post-R30 — for now everything zeroes on `GameState::reset()`.

namespace Faction {

enum Id : uint8_t {
    Imperium = 0,
    Federation,
    Cartel,
    FreeTraders,
    Count
};

inline const char* shortName(Id f) {
    switch(f) {
    case Imperium:
        return "IMP";
    case Federation:
        return "FED";
    case Cartel:
        return "CTL";
    case FreeTraders:
        return "FRT";
    default:
        return "?";
    }
}

inline const char* longName(Id f) {
    switch(f) {
    case Imperium:
        return "IMPERIUM";
    case Federation:
        return "FEDERATION";
    case Cartel:
        return "CARTEL";
    case FreeTraders:
        return "FREE TRADERS";
    default:
        return "?";
    }
}

// Government → faction. Bias toward the lawful / corporate axis:
//   Anarchy / Feudal / Dictatorship          → CTL / IMP / IMP
//   MultiGov / Communist                     → FRT / FRT
//   Confed / Democracy / Corporate           → FED / FED / FED
inline Id forGovernment(Galaxy::Government g) {
    switch(g) {
    case Galaxy::Government::Anarchy:
        return Cartel;
    case Galaxy::Government::Feudal:
        return Imperium;
    case Galaxy::Government::MultiGov:
        return FreeTraders;
    case Galaxy::Government::Dictatorship:
        return Imperium;
    case Galaxy::Government::Communist:
        return FreeTraders;
    case Galaxy::Government::Confederacy:
        return Federation;
    case Galaxy::Government::Democracy:
        return Federation;
    case Galaxy::Government::CorporateState:
        return Federation;
    }
    return FreeTraders;
}

inline Id forSystem(int sysIdx) {
    return forGovernment(Galaxy::systems[sysIdx].government);
}

// Assign each NPC a home faction at spawn. Pirate is always Cartel; the
// rest inherit the system they were spawned into.
inline Id forNPC(NPCShip::Role role, int sysIdx) {
    if(role == NPCShip::Role::Pirate) return Cartel;
    return forSystem(sysIdx);
}

// --- Standing helpers -----------------------------------------------------

inline void clampStanding(GameState& g) {
    for(int i = 0; i < Count; i++) {
        if(g.standing[i] > 100) g.standing[i] = 100;
        if(g.standing[i] < -100) g.standing[i] = -100;
    }
}

inline int8_t standingOf(const GameState& g, Id f) {
    return g.standing[(int)f];
}

inline void nudge(GameState& g, Id f, int delta) {
    int v = (int)g.standing[(int)f] + delta;
    if(v > 100) v = 100;
    if(v < -100) v = -100;
    g.standing[(int)f] = (int8_t)v;
}

// --- Kill payloads --------------------------------------------------------
//
// Pirate kill: every "lawful" faction approves — and Cartel sours hard.
// Patrol kill: the patrol's own faction takes the biggest hit; the other
//   lawful factions disapprove slightly. Cartel doesn't care (or quietly
//   approves — minor +).
// Trader kill: the home faction sours; Free Traders sour everywhere.
inline void applyKill(GameState& g, NPCShip::Role role, Id homeFaction) {
    switch(role) {
    case NPCShip::Role::Pirate:
        nudge(g, Imperium, +3);
        nudge(g, Federation, +3);
        nudge(g, FreeTraders, +3);
        nudge(g, Cartel, -8);
        break;
    case NPCShip::Role::Patrol:
        nudge(g, homeFaction, -12);
        // Aggression against lawful order shifts the whole lawful axis.
        for(int i = 0; i < Count; i++) {
            Id f = (Id)i;
            if(f == homeFaction || f == Cartel) continue;
            nudge(g, f, -2);
        }
        nudge(g, Cartel, +2); // outlaws cheer
        break;
    case NPCShip::Role::Trader:
        nudge(g, homeFaction, -8);
        if(homeFaction != FreeTraders) nudge(g, FreeTraders, -3);
        nudge(g, Cartel, +1);
        break;
    }
}

// Trade event: light nudge toward the system's faction, mirroring civil
// engagement. Buy/sell each call this once.
inline void applyTrade(GameState& g, int sysIdx) {
    nudge(g, forSystem(sysIdx), +1);
}

// --- Patrol hostility -----------------------------------------------------

constexpr int HostileThreshold = -30; // standing ≤ this → patrol attacks

inline bool patrolWouldAttack(const GameState& g, Id homeFaction) {
    return (int)g.standing[(int)homeFaction] <= HostileThreshold;
}

// --- Market price modifier ------------------------------------------------
//
// Returns a multiplier (0.0001 fixed-point) on the system's base price.
// Standing +100 → 0.80 (20% friend discount). Standing -100 → 1.20.
// Standing 0   → 1.00. Linear between.
//
// Returned as integer permille (×1000) to keep market arithmetic in ints.
inline int priceScalePermille(const GameState& g, int sysIdx) {
    Id f = forSystem(sysIdx);
    int st = (int)g.standing[(int)f]; // -100..+100
    // 1000 - 2*st  ⇒  +100→800, -100→1200.
    int scale = 1000 - 2 * st;
    if(scale < 700) scale = 700;
    if(scale > 1300) scale = 1300;
    return scale;
}

} // namespace Faction
