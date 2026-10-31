#pragma once
#include "Platform.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "GameState.h"
#include "Galaxy.h"
#include "Market.h"
#include "Faction.h"
#include "Audio.h"
#include "SolarSystem.h"
#include "NPCShip.h"

// Quest system — local-system-only, single-active-quest.
//
// Rules:
//   * Only ONE quest can be active at a time.
//   * A quest is TAKEN at a specific planet (sysIdx + planetPOI). All
//     objectives are resolved INSIDE the same solar system — no
//     hyperspace travel is required.
//   * The player must COMPLETE the objective and RETURN to that same
//     planet to turn it in for the credit reward + standing nudge.
//   * The player can DISCARD an active quest at any time (small
//     standing penalty, no payout).
//   * Each planet exposes a deterministic board of `BoardSize` offers.
//     Boards on planets in far-from-origin systems are harder and pay
//     better (see `systemDifficulty`).
//
// To add a new quest type:
//   1. Add an enum to `Quest::Type`.
//   2. Add a `buildBoard` case populating the slot fields.
//   3. Add a `formatObjective` case for the board / pause UI.
//   4. Add an event hook (onPirateKill / onHyperspaceArrive / onDock)
//      that flips `status` to ReadyToTurnIn when the objective is met.

namespace Quest {

enum class Type : uint8_t {
    None = 0,
    Patrol, // kill N pirates in this system, return home
    Delivery, // deliver N t of commodity to a planet POI, return home
    VisitPlanet, // pick up something at a planet POI, return home
    Courier, // one-way drop at a planet POI — paid on arrival
    Scavenge, // bring N t of an off-market commodity to the home planet
    Count,
};

constexpr int BoardSize = 4;

enum class Status : uint8_t {
    Idle = 0,
    InProgress,
    ReadyToTurnIn,
};

struct Slot {
    Type type;
    uint8_t fromSys; // home system
    uint8_t fromPOI; // home planet POI index
    uint8_t toPOI; // target planet POI (same system); 0xFF if N/A
    uint8_t commodity; // 0xFF if N/A
    uint8_t qty;
    uint8_t faction; // Faction::Id of sponsor
    int8_t factionDelta; // standing reward; penalty on discard = -half
    int16_t rewardTenthsCR;
    uint16_t progress; // patrol kill counter
    uint8_t difficulty; // 0=EASY .. 3=ELITE
};

inline Slot active = {};
inline Status status = Status::Idle;

// Set when a Patrol quest is accepted, cleared on the FIRST launch after
// acceptance. The launch handler reads + clears this to force a one-time
// pirate spawn — subsequent launches do not respawn killed pirates, so
// the player has a fixed quota to hunt.
inline bool pirateSpawnPending = false;

// Completion popup — turnIn() arms it; LandingScreen renders it modally
// and consumes it when the player presses ENTER (or ESC) to dismiss.
inline char completionMsg[48] = "";
inline bool completionPending = false;

inline void clearCompletion() {
    completionMsg[0] = '\0';
    completionPending = false;
}

inline void dismissCompletion() {
    clearCompletion();
}

inline void resetAll() {
    active = Slot{};
    active.type = Type::None;
    status = Status::Idle;
    pirateSpawnPending = false;
    clearCompletion();
}

inline bool isActive() {
    return status != Status::Idle && active.type != Type::None;
}

// Distance-from-origin difficulty (0..3). Drives qty + reward scaling so
// quests offered on far worlds are noticeably bigger.
inline uint8_t systemDifficulty(int sysIdx) {
    if(sysIdx == 0) return 0;
    float d = Galaxy::distanceLY(0, sysIdx);
    if(d < 6.0f) return 0;
    if(d < 12.0f) return 1;
    if(d < 18.0f) return 2;
    return 3;
}

inline const char* difficultyLabel(uint8_t d) {
    switch(d) {
    case 0:
        return "EASY";
    case 1:
        return "MED";
    case 2:
        return "HARD";
    default:
        return "ELITE";
    }
}

inline const char* typeShort(Type t) {
    switch(t) {
    case Type::Patrol:
        return "PATROL";
    case Type::Delivery:
        return "DELIVER";
    case Type::VisitPlanet:
        return "VISIT";
    case Type::Courier:
        return "COURIER";
    case Type::Scavenge:
        return "GATHER";
    default:
        return "-";
    }
}

// One-way quests pay out on arrival at `toPOI` and never require the
// player to come back to the home planet. Round-trip quests flip to
// ReadyToTurnIn when the objective is met and only turn in at home.
inline bool isOneWay(Type t) {
    return t == Type::Courier;
}

// ----- Board generation -----
//
// Picks a planet POI in the system distinct from `excludePOI`. Returns
// 0xFF if no other planet exists (single-planet systems exist).
inline uint8_t pickPlanetPOI(const SolarSystem::Layout& L, uint32_t& seed, int excludePOI) {
    // Build a tiny array of candidate planet indices, then pick from it.
    uint8_t cand[SolarSystem::MaxPlanets];
    int n = 0;
    for(int i = 0; i < L.numPOIs; i++) {
        if(L.poi[i].type != SolarSystem::POIType::Planet) continue;
        if(i == excludePOI) continue;
        if(n < SolarSystem::MaxPlanets) cand[n++] = (uint8_t)i;
    }
    if(n == 0) return 0xFF;
    return cand[Galaxy::lcg(seed) % (uint32_t)n];
}

// Pick a commodity matching `inStock` (true → qty>0, false → qty==0).
// Falls back across the constraints if nothing matches — we'd rather
// hand out a slightly off-flavor quest than skip it entirely.
inline uint8_t pickCommodity(int sysIdx, uint32_t& seed, bool inStock) {
    // First pass: respect both inStock + legal.
    for(int tries = 0; tries < 16; tries++) {
        int c = (int)(Galaxy::lcg(seed) % (uint32_t)Market::N);
        if(Market::items[c].illegal) continue;
        uint8_t q = Market::qtyAt(sysIdx, c);
        if(inStock ? (q > 0) : (q == 0)) return (uint8_t)c;
    }
    // Second pass: drop the legality requirement.
    for(int tries = 0; tries < 16; tries++) {
        int c = (int)(Galaxy::lcg(seed) % (uint32_t)Market::N);
        uint8_t q = Market::qtyAt(sysIdx, c);
        if(inStock ? (q > 0) : (q == 0)) return (uint8_t)c;
    }
    // Final fallback: anything legal.
    return (uint8_t)Market::Food;
}

// Pick a commodity for a fetch-style quest (Delivery / Scavenge). The
// goal is a real shopping trip: the good is NOT sold at the home planet
// but IS stocked (≥ `need`) at some other planet in the same system.
// Falls back gracefully — MarketScreen additionally enforces the rule at
// runtime once the contract is active, so a fallback pick still plays
// out correctly.
inline uint8_t pickFetchCommodity(
    int sysIdx,
    const SolarSystem::Layout& L,
    int homePOI,
    uint32_t& seed,
    uint8_t need) {
    auto stockedElsewhere = [&](int c) -> bool {
        for(int i = 0; i < L.numPOIs; i++) {
            if(L.poi[i].type != SolarSystem::POIType::Planet) continue;
            if(i == homePOI) continue;
            if(Market::qtyAtPlanet(sysIdx, i, c) >= need) return true;
        }
        return false;
    };
    // Pass 1: legal + missing at home + sourceable at another planet.
    for(int tries = 0; tries < 24; tries++) {
        int c = (int)(Galaxy::lcg(seed) % (uint32_t)Market::N);
        if(Market::items[c].illegal) continue;
        if(Market::qtyAtPlanet(sysIdx, homePOI, c) > 0) continue;
        if(!stockedElsewhere(c)) continue;
        return (uint8_t)c;
    }
    // Pass 2: legal + sourceable at another planet.
    for(int tries = 0; tries < 24; tries++) {
        int c = (int)(Galaxy::lcg(seed) % (uint32_t)Market::N);
        if(Market::items[c].illegal) continue;
        if(!stockedElsewhere(c)) continue;
        return (uint8_t)c;
    }
    // Pass 3: anything legal in the system at all.
    return pickCommodity(sysIdx, seed, /*inStock=*/true);
}

inline void buildBoard(int sysIdx, int planetPOI, Slot board[BoardSize]) {
    uint8_t diff = systemDifficulty(sysIdx);
    uint32_t seed = Galaxy::systemSubSeed(sysIdx, 0x5151u) ^ ((uint32_t)planetPOI * 0x9E3779B9u) ^
                    (Galaxy::marketEpoch * 0xA24BAED4u);
    uint8_t faction = (uint8_t)Faction::forSystem(sysIdx);

    SolarSystem::Layout L;
    SolarSystem::layoutFor(sysIdx, L);
    // Pre-count alternate planet POIs so we can skip planet-target quests
    // in single-planet systems (no destination is possible).
    int otherPlanets = 0;
    for(int i = 0; i < L.numPOIs; i++) {
        if(L.poi[i].type == SolarSystem::POIType::Planet && i != planetPOI) {
            otherPlanets++;
        }
    }

    for(int i = 0; i < BoardSize; i++) {
        Slot& s = board[i];
        s.fromSys = (uint8_t)sysIdx;
        s.fromPOI = (uint8_t)planetPOI;
        s.faction = faction;
        s.factionDelta = (int8_t)(2 + diff); // 2..5
        s.toPOI = 0xFF;
        s.commodity = 0xFF;
        s.progress = 0;
        s.difficulty = diff;

        // Pick a type. In single-planet systems we can't pick anything
        // with a planet-POI target, so collapse those to Patrol / Scavenge.
        int roll = (int)(Galaxy::lcg(seed) % 5u);
        Type t = (Type)(1 + roll);
        if(otherPlanets == 0 &&
           (t == Type::Delivery || t == Type::VisitPlanet || t == Type::Courier)) {
            t = (roll & 1) ? Type::Patrol : Type::Scavenge;
        }
        s.type = t;

        int base = 0; // payout in tenths CR
        switch(t) {
        case Type::Patrol: {
            // Patrol pirates spawn once on quest accept (never refill), so
            // the quota has to fit in the per-system ship table. Cap at
            // MaxNPCs so the contract is always winnable.
            int want = 2 + (int)diff;
            if(want > NPCShip::MaxNPCs) want = NPCShip::MaxNPCs;
            s.qty = (uint8_t)want;
            base = 600 + diff * 700 + (int)(Galaxy::lcg(seed) % 400u);
            break;
        }
        case Type::Delivery:
            s.toPOI = pickPlanetPOI(L, seed, planetPOI);
            s.qty = (uint8_t)(2 + diff); // 2..5 t
            s.commodity = pickFetchCommodity(sysIdx, L, planetPOI, seed, s.qty);
            base = 500 + diff * 900 + (int)(Galaxy::lcg(seed) % 500u);
            break;
        case Type::VisitPlanet:
            s.toPOI = pickPlanetPOI(L, seed, planetPOI);
            s.qty = 1;
            base = 500 + diff * 700 + (int)(Galaxy::lcg(seed) % 300u);
            break;
        case Type::Courier:
            s.toPOI = pickPlanetPOI(L, seed, planetPOI);
            s.qty = 1;
            // One-way contract — pays slightly less than VisitPlanet's
            // round-trip but is finished the moment the player lands at
            // toPOI, no return trip required.
            base = 350 + diff * 550 + (int)(Galaxy::lcg(seed) % 250u);
            break;
        case Type::Scavenge:
            s.qty = (uint8_t)(2 + diff);
            s.commodity = pickFetchCommodity(sysIdx, L, planetPOI, seed, s.qty);
            base = 700 + diff * 900 + (int)(Galaxy::lcg(seed) % 500u);
            break;
        default:
            break;
        }
        if(base > 32000) base = 32000;
        s.rewardTenthsCR = (int16_t)base;
    }
}

// ----- Accept / discard / turn-in -----

inline bool accept(GameState& g, const Slot& s) {
    (void)g;
    if(isActive()) return false;
    if(s.type == Type::None) return false;
    // Sanity: planet-target quests in a single-planet system would be
    // unwinnable. buildBoard already filters these out, but double-guard
    // here in case a stale board sneaks through.
    if((s.type == Type::Delivery || s.type == Type::VisitPlanet || s.type == Type::Courier) &&
       s.toPOI == 0xFF) {
        return false;
    }
    active = s;
    active.progress = 0;
    status = Status::InProgress;
    // Patrol contracts arm the one-shot pirate spawn — the launch handler
    // reads + clears `pirateSpawnPending` so killed pirates don't refill
    // on subsequent landings.
    pirateSpawnPending = (s.type == Type::Patrol);
    clearCompletion();
    Audio::missionAccept();
    return true;
}

inline void discard(GameState& g) {
    if(!isActive()) return;
    int8_t penalty = -(int8_t)((active.factionDelta + 1) / 2);
    Faction::nudge(g, (Faction::Id)active.faction, penalty);
    Audio::deny();
    active.type = Type::None;
    status = Status::Idle;
    pirateSpawnPending = false;
    clearCompletion();
}

inline void turnIn(GameState& g) {
    if(status != Status::ReadyToTurnIn) return;
    int16_t reward = active.rewardTenthsCR;
    uint8_t fac = active.faction;
    int8_t nudge = active.factionDelta;
    g.credits += reward;
    Faction::nudge(g, (Faction::Id)fac, nudge);
    Audio::missionComplete();

    // Completion popup body — modal, dismissed only via ENTER/ESC.
    snprintf(
        completionMsg,
        sizeof(completionMsg),
        "+%d.%d CR  %s %+d REP",
        reward / 10,
        reward % 10,
        Faction::shortName((Faction::Id)fac),
        (int)nudge);
    completionPending = true;

    active.type = Type::None;
    status = Status::Idle;
    pirateSpawnPending = false;
}

// True only on the FIRST launch after a Patrol quest is accepted. The
// launch handler force-spawns `qty` pirates and then calls
// `consumePirateSpawn()` so killed pirates don't refill afterward.
inline bool needsPirateSpawn() {
    return pirateSpawnPending && isActive() && active.type == Type::Patrol;
}

inline uint8_t pirateSpawnQty() {
    return needsPirateSpawn() ? active.qty : 0;
}

inline void consumePirateSpawn() {
    pirateSpawnPending = false;
}

// ----- Event hooks -----

inline void onPirateKill(GameState& g) {
    (void)g;
    if(!isActive()) return;
    if(status != Status::InProgress) return;
    if(active.type != Type::Patrol) return;
    if(active.progress < 65000) active.progress++;
    if(active.progress >= (uint16_t)active.qty) {
        status = Status::ReadyToTurnIn;
    }
}

// Kept as a stub so future cross-system quest types could re-introduce
// arrival-based triggers. Local-only quests don't use it today.
inline void onHyperspaceArrive(GameState& g, int sysIdx) {
    (void)g;
    (void)sysIdx;
}

// Landing handler — call on planetfall with the system + planet POI
// index. Resolves delivery drops at the target POI, VisitPlanet flips,
// Scavenge home-arrival, and the actual home turn-in payout.
inline void onDock(GameState& g, int sysIdx, int planetPOI) {
    if(!isActive()) return;
    // Local-only: a quest accepted in system X can only resolve in X.
    if((int)active.fromSys != sysIdx) return;

    // Delivery: dropping the cargo at the target POI.
    if(status == Status::InProgress && active.type == Type::Delivery &&
       (int)active.toPOI == planetPOI) {
        int c = (int)active.commodity;
        if(c >= 0 && c < (int)Market::N && g.cargo[c] >= active.qty) {
            g.cargo[c] = (uint8_t)(g.cargo[c] - active.qty);
            status = Status::ReadyToTurnIn;
        }
    }

    // VisitPlanet: touching down at the target POI flips to ReadyToTurnIn
    // — the player still owes a trip home to collect.
    if(status == Status::InProgress && active.type == Type::VisitPlanet &&
       (int)active.toPOI == planetPOI) {
        status = Status::ReadyToTurnIn;
    }

    // Courier: one-way drop. Landing at the target IS the turn-in — pay
    // the player out on the spot and clear the quest, no return trip.
    if(status == Status::InProgress && active.type == Type::Courier &&
       (int)active.toPOI == planetPOI) {
        status = Status::ReadyToTurnIn;
        turnIn(g);
        return;
    }

    // Home planet — must match BOTH system and POI.
    bool atHome = ((int)active.fromPOI == planetPOI);
    if(!atHome) return;

    // Scavenge resolves on home arrival if the cargo is on board.
    if(status == Status::InProgress && active.type == Type::Scavenge) {
        int c = (int)active.commodity;
        if(c >= 0 && c < (int)Market::N && g.cargo[c] >= active.qty) {
            g.cargo[c] = (uint8_t)(g.cargo[c] - active.qty);
            status = Status::ReadyToTurnIn;
        }
    }

    if(status == Status::ReadyToTurnIn) {
        turnIn(g);
    }
}

// ----- Display helpers -----
//
// POI-relative name for a target planet in the local system. Returns the
// system name as a degenerate fallback if the POI index is bad.
inline void planetName(uint8_t sysIdx, uint8_t poi, char* out, size_t cap) {
    SolarSystem::Layout L;
    SolarSystem::layoutFor((int)sysIdx, L);
    if(poi < L.numPOIs && L.poi[poi].type == SolarSystem::POIType::Planet) {
        SolarSystem::displayName((int)sysIdx, L.poi[poi], out, cap);
    } else {
        snprintf(out, cap, "%s", Galaxy::systems[sysIdx].name);
    }
}

// Deterministic "flavor index" derived from the slot fields. Lets each
// board entry pick one of a handful of narrative wordings without
// storing an extra byte per slot.
inline uint8_t slotFlavor(const Slot& s) {
    uint32_t h = (uint32_t)s.type * 131u + (uint32_t)s.qty * 17u + (uint32_t)s.toPOI * 7u +
                 (uint32_t)s.commodity * 3u + (uint32_t)s.fromPOI;
    return (uint8_t)(h & 3u);
}

// Board / accepted-card text. Reads like a contract description rather
// than a bare action verb — "TAXI PASSENGER TO X" instead of "VISIT X".
inline void formatObjective(const Slot& s, char* out, size_t cap) {
    uint8_t f = slotFlavor(s);
    switch(s.type) {
    case Type::Patrol: {
        static const char* const tpl[4] = {
            "BOUNTY: %u PIRATES",
            "HUNT %u RAIDERS",
            "CLEAR %u PIRATES",
            "TAKE DOWN %u OUTLAWS",
        };
        snprintf(out, cap, tpl[f], (unsigned)s.qty);
        break;
    }
    case Type::Delivery: {
        char dst[16];
        planetName(s.fromSys, s.toPOI, dst, sizeof(dst));
        static const char* const tpl[4] = {
            "DELIVER %ut %s TO %s",
            "SHIP %ut %s TO %s",
            "DROP %ut %s AT %s",
            "SUPPLY %s WITH %ut %s", // swapped arg order — handled below
        };
        if(f == 3) {
            snprintf(out, cap, tpl[3], dst, (unsigned)s.qty, Market::items[s.commodity].name);
        } else {
            snprintf(out, cap, tpl[f], (unsigned)s.qty, Market::items[s.commodity].name, dst);
        }
        break;
    }
    case Type::VisitPlanet: {
        char dst[16];
        planetName(s.fromSys, s.toPOI, dst, sizeof(dst));
        // Round-trip — narrative implies bringing something *back*.
        static const char* const tpl[4] = {
            "PICK UP CONTACT AT %s",
            "RECOVER ARTIFACT AT %s",
            "INSPECT %s SITE",
            "SCOUT %s INSTALLATION",
        };
        snprintf(out, cap, tpl[f], dst);
        break;
    }
    case Type::Courier: {
        char dst[16];
        planetName(s.fromSys, s.toPOI, dst, sizeof(dst));
        // One-way — narrative is a drop-off; payment lands on arrival.
        static const char* const tpl[4] = {
            "COURIER PACKET TO %s",
            "FERRY DIPLOMAT TO %s",
            "TAXI PASSENGER TO %s",
            "DELIVER DATA-CHIP TO %s",
        };
        snprintf(out, cap, tpl[f], dst);
        break;
    }
    case Type::Scavenge: {
        static const char* const tpl[4] = {
            "PROCURE %ut %s",
            "SOURCE %ut %s",
            "GATHER %ut %s",
            "BLACK-MARKET %ut %s",
        };
        snprintf(out, cap, tpl[f], (unsigned)s.qty, Market::items[s.commodity].name);
        break;
    }
    default:
        snprintf(out, cap, "-");
        break;
    }
}

// Single-line active-step string for the pause overlay. Always describes
// the ONE thing the player should do right now — "GO TO TRIX II",
// "KILL 2/3 PIRATES", "RETURN TO TRIX" — never both the contract and
// the step at once.
inline void formatStatus(char* out, size_t cap) {
    if(!isActive()) {
        snprintf(out, cap, "NO ACTIVE QUEST");
        return;
    }
    if(status == Status::ReadyToTurnIn) {
        char home[16];
        planetName(active.fromSys, active.fromPOI, home, sizeof(home));
        snprintf(out, cap, "RETURN TO %s", home);
        return;
    }
    switch(active.type) {
    case Type::Patrol:
        snprintf(out, cap, "KILL %u/%u PIRATES", (unsigned)active.progress, (unsigned)active.qty);
        break;
    case Type::Delivery: {
        char dst[16];
        planetName(active.fromSys, active.toPOI, dst, sizeof(dst));
        snprintf(
            out,
            cap,
            "DELIVER %ut %s TO %s",
            (unsigned)active.qty,
            Market::items[active.commodity].name,
            dst);
        break;
    }
    case Type::VisitPlanet: {
        char dst[16];
        planetName(active.fromSys, active.toPOI, dst, sizeof(dst));
        snprintf(out, cap, "GO TO %s", dst);
        break;
    }
    case Type::Courier: {
        char dst[16];
        planetName(active.fromSys, active.toPOI, dst, sizeof(dst));
        snprintf(out, cap, "DROP AT %s", dst);
        break;
    }
    case Type::Scavenge:
        snprintf(
            out, cap, "GET %ut %s", (unsigned)active.qty, Market::items[active.commodity].name);
        break;
    default:
        snprintf(out, cap, "-");
        break;
    }
}

} // namespace Quest
