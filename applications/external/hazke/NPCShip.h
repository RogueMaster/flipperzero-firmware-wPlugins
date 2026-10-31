#pragma once
#include "Platform.h"
#include <math.h>
#include <stdint.h>
#include "Galaxy.h"
#include "SolarSystem.h"
#include "Audio.h"

// Refit R16: NPC traders in flight.
//
// 1..4 ships per system, deterministically seeded from the system index.
// Each ship picks a non-star POI as its destination and cruises toward it
// at CruiseSpeed; on arrival it picks a new POI (≠ current dest) and the
// cycle continues — station ↔ planet ↔ gate. Ships fly roughly flat in y
// with a small vertical lerp toward their target.
//
// Rendering lives in SystemFlight (so it can reuse the same projection),
// but NPCShip owns state and AI here.

namespace NPCShip {

constexpr int MaxNPCs = 4;
// Both cruise and pursue speeds sit below the player's MaxSpeed (600
// sysu/s in SystemFlight) so a full-throttle player can always either
// run away from a fight or close in on a wounded ship for boarding.
constexpr float CruiseSpeed = 420.0f; // sysu/s
constexpr float ArriveRadius = 900.0f; // sysu — pick a new dest within
constexpr float HailRange = 1000.0f; // sysu — player can hail within
constexpr float TurnRate = 1.2f; // rad/s yaw lerp

// R19 pirate/patrol tuning.
constexpr float DetectRange = 4000.0f; // sysu — pirates aggro inside
constexpr float FireRange = 2400.0f; // sysu — must reach the orbit ring
constexpr float FireConeRad = 0.35f; // ~20° half-angle for fire cone
constexpr float PursueSpeed = 520.0f; // sysu/s — still faster than cruise
constexpr float PursueTurn = 1.8f; // rad/s — and turn harder
// Stand-off ring: hostiles ease into a band around the player and orbit
// it laterally instead of parking nose-to-nose for a still-frame duel.
constexpr float StandoffRange = 2200.0f; // outer edge of the ring
constexpr float StandoffInner = 1500.0f; // closer than this and we back off
constexpr float OrbitSpeed = 300.0f; // sysu/s lateral drift while orbiting

using Galaxy::lcg;

enum class Role : uint8_t {
    Trader = 0,
    Pirate,
    Patrol
};

struct Ship {
    bool active;
    Role role;
    uint8_t kind; // 0..3 (cosmetic variant — same wedge model for now)
    uint16_t color;
    int destPOI; // index into the system's POI table (for wander AI)
    float wx, wy, wz; // world position (sysu)
    float yaw; // world yaw (radians), Y-axis rotation
    float pitch; // world pitch (radians), nose up positive — lets
        // ships dive/climb instead of staying flat in XZ
    uint32_t hailSeed; // stable seed for this NPC's trade quotes
    bool attacking; // R19: pirate has player in cone + FireRange
    float shields; // R20: 0..1 — depleted before hull takes damage
    float hull; // R20: 0..1 — ship dies when this hits 0
    float fireTimer; // R20: seconds until next pirate laser shot
    float missileTimer; // R21: seconds until next pirate missile poll
    uint8_t homeFaction; // R22: Faction::Id this ship belongs to
    uint8_t modelId; // R25: Ship3D::ModelId used to render this hull
    bool provoked; // Player has shot this ship — it now fights back
        // even if it's a Trader or peaceful Patrol.
    bool alerted; // Hostile-alert SFX already played for the
        // current engagement; cleared with hysteresis
        // when the ship breaks off.
};

inline Ship ships[MaxNPCs];
inline int numActive = 0;
inline int loadedSys = -1;

// Pick a non-star POI index, excluding `exclude` if possible. Cheap: a
// few random picks then a linear scan fallback.
inline int pickNonStarPOI(const SolarSystem::Layout& L, uint32_t& s, int exclude) {
    for(int tries = 0; tries < 12; tries++) {
        int c = (int)(lcg(s) % (uint32_t)L.numPOIs);
        if(L.poi[c].type == SolarSystem::POIType::Star) continue;
        if(c == exclude) continue;
        return c;
    }
    for(int i = 0; i < L.numPOIs; i++) {
        if(L.poi[i].type != SolarSystem::POIType::Star && i != exclude) return i;
    }
    return -1;
}

// R22: Government → faction numeric id (mirrors Faction::Id). Inlined
// here so NPCShip doesn't need to include Faction.h (which would create
// a cycle, since Faction.h depends on NPCShip).
//   0 Imperium  1 Federation  2 Cartel  3 FreeTraders
inline uint8_t factionForGovernment(Galaxy::Government g) {
    switch(g) {
    case Galaxy::Government::Anarchy:
        return 2; // Cartel
    case Galaxy::Government::Feudal:
        return 0; // Imperium
    case Galaxy::Government::MultiGov:
        return 3; // FreeTraders
    case Galaxy::Government::Dictatorship:
        return 0; // Imperium
    case Galaxy::Government::Communist:
        return 3; // FreeTraders
    case Galaxy::Government::Confederacy:
        return 1; // Federation
    case Galaxy::Government::Democracy:
        return 1; // Federation
    case Galaxy::Government::CorporateState:
        return 1; // Federation
    }
    return 3;
}

// Role distribution skews on the current system's government. Anarchy
// breeds pirates and few patrols; stable governments are the inverse.
// Returns picks summing to 100; pirateOdds + patrolOdds + traderOdds.
inline void rollRoleOdds(uint8_t gov, int& pirateOdds, int& patrolOdds) {
    int g = (int)gov; // 0..7 (Anarchy..Corporate)
    pirateOdds = 50 - g * 6;
    if(pirateOdds < 5) pirateOdds = 5;
    patrolOdds = 10 + g * 4;
    if(patrolOdds > 40) patrolOdds = 40;
    // Trader is the remainder; ensure it isn't negative.
    if(pirateOdds + patrolOdds > 90) {
        int slack = pirateOdds + patrolOdds - 90;
        pirateOdds -= slack / 2;
        patrolOdds -= slack - slack / 2;
    }
}

// Re-seed the ship roster for the given system. Called from
// SystemFlight::enter / enterNearPOI when the layout switches.
inline void spawnFor(int sysIdx, const SolarSystem::Layout& L) {
    loadedSys = sysIdx;
    uint32_t s = Galaxy::systemSubSeed(sysIdx, 0x4E5043u); // "NPC"
    int want = 1 + (int)(lcg(s) & 3u); // 1..4
    numActive = want;
    // Neutral palette for traders (greens / yellows / sky / tan); pirates
    // and patrols use dedicated colors so the player can read intent off
    // ship silhouettes and radar blips at a glance.
    static const uint16_t traderPalette[4] = {
        0x05E0, // green
        0xFFC0, // pale yellow
        0x867F, // sky blue
        0xCE59, // tan
    };
    constexpr uint16_t kPirateColor = 0xF800; // red
    constexpr uint16_t kPatrolColor = 0x07FF; // cyan

    int pirateOdds = 0, patrolOdds = 0;
    rollRoleOdds((uint8_t)Galaxy::systems[sysIdx].government, pirateOdds, patrolOdds);

    // Starter system is always friendly: no pirates anywhere in space, and
    // any rolled patrols stay peaceful regardless of standing. This gives
    // a new commander a safe runway to learn flight, trade, and landing.
    const bool friendlySystem = (sysIdx == 0);
    if(friendlySystem) pirateOdds = 0;

    for(int i = 0; i < MaxNPCs; i++) {
        Ship& sh = ships[i];
        sh.active = (i < want);
        if(!sh.active) continue;

        int roll = (int)(lcg(s) % 100u);
        if(roll < pirateOdds)
            sh.role = Role::Pirate;
        else if(roll < pirateOdds + patrolOdds)
            sh.role = Role::Patrol;
        else
            sh.role = Role::Trader;

        sh.kind = (uint8_t)(lcg(s) & 3u);
        switch(sh.role) {
        case Role::Pirate:
            sh.color = kPirateColor;
            break;
        case Role::Patrol:
            sh.color = kPatrolColor;
            break;
        case Role::Trader:
            sh.color = traderPalette[sh.kind];
            break;
        }
        sh.attacking = false;
        sh.provoked = false;
        sh.alerted = false;
        sh.shields = 1.0f;
        sh.hull = 1.0f;
        sh.fireTimer = 0.5f + (float)i * 0.3f; // stagger pirate first shots
        sh.missileTimer = 2.0f + (float)i * 0.5f; // stagger missile launch polls
        // R22: pirates always Cartel; everyone else inherits the system.
        sh.homeFaction = (sh.role == Role::Pirate) ?
                             (uint8_t)2 // Cartel
                             :
                             factionForGovernment(Galaxy::systems[sysIdx].government);

        // R25: pick a wireframe silhouette. IDs mirror Ship3D::ModelId:
        //   0 Wedge, 1 Freighter, 2 Interceptor, 3 Gunship, 4 Barge, 5 Alien.
        // Pirates fly Interceptors and Gunships. Patrols favor Gunships;
        // their lighter craft fall back to the Wedge so they don't look
        // identical to the trader baseline. Traders rotate the heavier
        // civilian hulls. Anarchy systems roll a low-probability Alien
        // pirate so the boss silhouette occasionally appears.
        {
            uint8_t mid;
            switch(sh.role) {
            case Role::Pirate:
                mid = (sh.kind & 1u) ? 3 /*Gunship*/ : 2 /*Interceptor*/;
                if(Galaxy::systems[sysIdx].government == Galaxy::Government::Anarchy) {
                    if((lcg(s) % 5u) == 0u) mid = 5; // Alien (rare)
                }
                break;
            case Role::Patrol:
                mid = (sh.kind & 1u) ? 3 /*Gunship*/ : 0 /*Wedge*/;
                break;
            case Role::Trader:
            default:
                switch(sh.kind & 3u) {
                case 0:
                    mid = 0 /*Wedge*/;
                    break;
                case 1:
                    mid = 1 /*Freighter*/;
                    break;
                case 2:
                    mid = 4 /*Barge*/;
                    break;
                default:
                    mid = 0 /*Wedge*/;
                    break;
                }
                break;
            }
            sh.modelId = mid;
        }

        int home = pickNonStarPOI(L, s, -1);
        int dest = pickNonStarPOI(L, s, home);
        sh.destPOI = (dest >= 0) ? dest : home;
        const auto& src = L.poi[home];
        float ang = (float)(lcg(s) & 0xFFFFu) * (6.2831853f / 65536.0f);
        // Offset starts past the body's surface so a ship can't spawn
        // inside (or skimming) a large planet.
        float offR = (float)src.radius + 400.0f + (float)(lcg(s) & 0xFFFFu) / 65536.0f * 600.0f;
        sh.wx = (float)src.x + cosf(ang) * offR;
        sh.wy = (float)src.y;
        sh.wz = (float)src.z + sinf(ang) * offR;
        if(sh.destPOI >= 0) {
            const auto& dst = L.poi[sh.destPOI];
            float dx = (float)dst.x - sh.wx;
            float dy = (float)dst.y - sh.wy;
            float dz = (float)dst.z - sh.wz;
            float horiz = sqrtf(dx * dx + dz * dz);
            sh.yaw = atan2f(dx, dz);
            sh.pitch = atan2f(dy, horiz);
        } else {
            sh.yaw = 0.0f;
            sh.pitch = 0.0f;
        }
        sh.hailSeed = lcg(s) ^ ((uint32_t)i * 0xC0FFEE05u);
    }
}

inline void clearAll() {
    numActive = 0;
    loadedSys = -1;
    for(int i = 0; i < MaxNPCs; i++)
        ships[i].active = false;
}

// Ensure at least `want` ships in this system have Role::Pirate, used
// by the patrol-quest hook so peaceful systems still produce threats
// after the player launches with an open contract. Promotes existing
// non-pirate ships first; falls back to activating empty slots near a
// random non-star POI.
inline void ensurePirates(int want, int sysIdx, const SolarSystem::Layout& L) {
    if(want <= 0) return;
    int piratesFound = 0;
    for(int i = 0; i < MaxNPCs; i++) {
        if(ships[i].active && ships[i].role == Role::Pirate) piratesFound++;
    }
    int needed = want - piratesFound;
    if(needed <= 0) return;

    auto stampPirate = [](Ship& sh, uint8_t kind) {
        sh.role = Role::Pirate;
        sh.kind = kind;
        sh.color = 0xF800; // red
        sh.modelId = (kind & 1u) ? 3 /*Gunship*/ : 2 /*Interceptor*/;
        sh.shields = 1.0f;
        sh.hull = 1.0f;
        sh.fireTimer = 0.7f;
        sh.missileTimer = 2.5f;
        sh.homeFaction = 2; // Cartel
        sh.provoked = false;
        sh.attacking = false;
        sh.alerted = false;
    };

    // First pass: promote existing non-pirate ships.
    for(int i = 0; i < MaxNPCs && needed > 0; i++) {
        Ship& sh = ships[i];
        if(!sh.active || sh.role == Role::Pirate) continue;
        stampPirate(sh, sh.kind);
        needed--;
    }

    if(needed <= 0) return;

    // Second pass: activate empty slots and place them well off the
    // player's launch standoff so they don't collide with the cold-
    // launching ship.
    uint32_t s = Galaxy::systemSubSeed(sysIdx, 0xB1A7Eu) ^ (uint32_t)millis();
    for(int i = 0; i < MaxNPCs && needed > 0; i++) {
        Ship& sh = ships[i];
        if(sh.active) continue;
        sh.active = true;
        stampPirate(sh, (uint8_t)(lcg(s) & 3u));
        sh.hailSeed = lcg(s) ^ ((uint32_t)i * 0xC0FFEE05u);

        int spawn = pickNonStarPOI(L, s, -1);
        if(spawn >= 0) {
            const auto& src = L.poi[spawn];
            float ang = (float)(lcg(s) & 0xFFFFu) * (6.2831853f / 65536.0f);
            float offR =
                (float)src.radius + 3000.0f + (float)(lcg(s) & 0xFFFFu) / 65536.0f * 1500.0f;
            sh.wx = (float)src.x + cosf(ang) * offR;
            sh.wy = (float)src.y;
            sh.wz = (float)src.z + sinf(ang) * offR;
        } else {
            sh.wx = 4000.0f;
            sh.wy = 0.0f;
            sh.wz = 4000.0f;
        }
        sh.destPOI = pickNonStarPOI(L, s, -1);
        sh.yaw = 0.0f;
        sh.pitch = 0.0f;
        numActive++;
        needed--;
    }
}

// Helper: lerp `yaw` toward `targetYaw` by at most `turnRate * dt`.
inline void lerpYaw(float& yaw, float targetYaw, float turnRate, float dt) {
    float dyaw = targetYaw - yaw;
    while(dyaw > 3.14159265f)
        dyaw -= 6.2831853f;
    while(dyaw < -3.14159265f)
        dyaw += 6.2831853f;
    float maxStep = turnRate * dt;
    if(dyaw > maxStep) dyaw = maxStep;
    if(dyaw < -maxStep) dyaw = -maxStep;
    yaw += dyaw;
}

// Pitch slew (no wrap-around — pitch is naturally bounded between ±π/2).
inline void lerpPitch(float& pitch, float targetPitch, float turnRate, float dt) {
    float d = targetPitch - pitch;
    float maxStep = turnRate * dt;
    if(d > maxStep) d = maxStep;
    if(d < -maxStep) d = -maxStep;
    pitch += d;
    // Soft clamp so the nose can't tip past straight-up / straight-down.
    const float lim = 1.30f;
    if(pitch > lim) pitch = lim;
    if(pitch < -lim) pitch = -lim;
}

// Wander AI shared by Trader / Patrol (and by Pirate when the player is
// outside DetectRange). Returns false if the ship arrived this frame
// (dest re-picked already; caller should skip motion).
inline bool wander(const SolarSystem::Layout& L, Ship& sh, uint32_t& s, float dt) {
    if(sh.destPOI < 0 || sh.destPOI >= L.numPOIs) {
        sh.destPOI = pickNonStarPOI(L, s, -1);
        if(sh.destPOI < 0) return true;
    }
    const auto& dst = L.poi[sh.destPOI];
    float dx = (float)dst.x - sh.wx;
    float dy = (float)dst.y - sh.wy;
    float dz = (float)dst.z - sh.wz;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    // Arrival is measured from the body's *surface*, not its center —
    // planets are up to ~1600 sysu in radius, so a center-based check
    // would march ships straight into the globe before re-targeting.
    if(dist < (float)dst.radius + ArriveRadius) {
        sh.destPOI = pickNonStarPOI(L, s, sh.destPOI);
        return false;
    }
    float horiz = sqrtf(dx * dx + dz * dz);
    lerpYaw(sh.yaw, atan2f(dx, dz), TurnRate, dt);
    lerpPitch(sh.pitch, atan2f(dy, horiz), TurnRate, dt);
    // 3D forward vector — ship climbs / dives toward the destination's
    // altitude instead of staying glued to the XZ plane.
    float cp = cosf(sh.pitch), sp = sinf(sh.pitch);
    float cy = cosf(sh.yaw), sy = sinf(sh.yaw);
    float fx = sy * cp;
    float fy = sp;
    float fz = cy * cp;
    // Damaged ships limp along at half cruise too (mirrors the pursue path).
    float spd = (sh.hull < 0.5f) ? (CruiseSpeed * 0.5f) : CruiseSpeed;
    sh.wx += fx * spd * dt;
    sh.wy += fy * spd * dt;
    sh.wz += fz * spd * dt;
    return true;
}

// R22: forward declaration — Patrol hostility depends on player standing
// with the patrol's home faction. The bool is computed in SystemFlight
// (which knows the standing) and passed in via `patrolHostile[i]`.

// Per-frame AI. Trader wanders between POIs. Pirate always hunts the
// player when inside DetectRange. Patrol normally wanders, but when
// `patrolHostile[i]` is true (player has soured relations with the
// patrol's faction below HostileThreshold) it behaves exactly like a
// Pirate.
inline void update(
    const SolarSystem::Layout& L,
    float dt,
    float playerX,
    float playerY,
    float playerZ,
    const bool patrolHostile[MaxNPCs]) {
    if(loadedSys < 0) return;
    uint32_t s = ((uint32_t)loadedSys * 0xCAFEBABEu) + (uint32_t)millis();
    for(int i = 0; i < MaxNPCs; i++) {
        Ship& sh = ships[i];
        if(!sh.active) continue;

        // Slow shield regen mirrors the player's — ~37 s full recharge.
        // Once shields collapse to 0 they stay down for the rest of this
        // ship's life (matches the player rule — depleted shields don't
        // self-repair, the ship has to die or land to reset).
        // Hull stays burnt-in until the ship dies.
        if(sh.shields > 0.0f && sh.shields < 1.0f) {
            sh.shields += 0.0267f * dt;
            if(sh.shields > 1.0f) sh.shields = 1.0f;
        }

        // Any ship the player has shot turns hostile regardless of role —
        // shooting a cargo trader earns you a fight.
        bool huntsPlayer = (sh.role == Role::Pirate) ||
                           (sh.role == Role::Patrol && patrolHostile[i]) || sh.provoked;
        // Hull damage cripples the engines: below 50% hull a ship can only
        // muster half thrust. Combined with NPCs being slower than the
        // player at full throttle, this is what lets a winged ship be run
        // down for boarding (or run away from when the player's the wounded one).
        float speedMul = (sh.hull < 0.5f) ? 0.5f : 1.0f;
        if(huntsPlayer) {
            float dx = playerX - sh.wx;
            float dy = playerY - sh.wy;
            float dz = playerZ - sh.wz;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if(dist < DetectRange) {
                // First tick of this engagement — warn the player audibly that
                // someone just turned hostile and is coming in.
                if(!sh.alerted) {
                    sh.alerted = true;
                    Audio::hostileAlert();
                }
                float horiz = sqrtf(dx * dx + dz * dz);
                float targetYaw = atan2f(dx, dz);
                float targetPitch = atan2f(dy, horiz);
                lerpYaw(sh.yaw, targetYaw, PursueTurn, dt);
                lerpPitch(sh.pitch, targetPitch, PursueTurn, dt);
                float cp = cosf(sh.pitch), sp = sinf(sh.pitch);
                float cy = cosf(sh.yaw), sy = sinf(sh.yaw);
                float fx = sy * cp;
                float fy = sp;
                float fz = cy * cp;
                // Stand off ring:
                //   dist > StandoffRange     → close in at full pursue speed
                //   StandoffInner..Standoff  → orbit the player laterally while
                //                              gently trimming radius, so the
                //                              duel keeps moving instead of
                //                              freezing nose-to-nose
                //   dist < StandoffInner     → actively reverse so we don't end up
                //                              inside the player's cockpit
                float spd;
                float lateralSpd = 0.0f;
                if(dist > StandoffRange) {
                    spd = PursueSpeed;
                } else if(dist > StandoffInner) {
                    // Gentle radial trim toward the middle of the ring.
                    float desiredR = 0.5f * (StandoffRange + StandoffInner);
                    spd = (dist - desiredR) * 0.4f;
                    if(spd > OrbitSpeed) spd = OrbitSpeed;
                    if(spd < -OrbitSpeed) spd = -OrbitSpeed;
                    // Slot index parity picks orbit handedness so multiple
                    // hostiles don't strafe in lockstep.
                    lateralSpd = (i & 1) ? OrbitSpeed : -OrbitSpeed;
                } else {
                    float k = (StandoffInner - dist) / StandoffInner; // 0..1 the closer we are
                    if(k > 1.0f) k = 1.0f;
                    spd = -PursueSpeed * (0.4f + 0.6f * k); // back off hard when very close
                }
                // Right vector in the XZ plane (90° CW from forward yaw).
                float rxw = cosf(sh.yaw);
                float rzw = -sinf(sh.yaw);
                spd *= speedMul;
                lateralSpd *= speedMul;
                sh.wx += (fx * spd + rxw * lateralSpd) * dt;
                sh.wy += fy * spd * dt;
                sh.wz += (fz * spd + rzw * lateralSpd) * dt;
                float coneErr = targetYaw - sh.yaw;
                while(coneErr > 3.14159265f)
                    coneErr -= 6.2831853f;
                while(coneErr < -3.14159265f)
                    coneErr += 6.2831853f;
                float pitchErr = targetPitch - sh.pitch;
                sh.attacking = (fabsf(coneErr) < FireConeRad) && (fabsf(pitchErr) < FireConeRad) &&
                               (dist < FireRange);
                continue;
            }
            sh.attacking = false;
            // Re-arm the alert only once the ship has clearly broken off, so
            // a hostile drifting across the detect boundary doesn't whoop on
            // every crossing.
            if(dist > DetectRange * 1.5f) sh.alerted = false;
        } else {
            sh.attacking = false;
        }

        wander(L, sh, s, dt);
    }
}

// R19/R22: true if any ship (pirate, or hostile patrol) currently has the
// player in cone + FireRange. Used by the HUD to flash a HOSTILE warning.
inline bool anyPirateAttacking() {
    for(int i = 0; i < MaxNPCs; i++) {
        const auto& sh = ships[i];
        if(!sh.active || !sh.attacking) continue;
        if(sh.role == Role::Pirate) return true;
        if(sh.role == Role::Patrol) return true; // patrol only sets it when hostile
    }
    return false;
}

// True if the player can currently open a hail/loot dialog with this
// ship. Traders are always hailable (they have goods for sale).
// Anything else has to be wounded first — hull below the critical
// firing threshold means the gunnery is dead and the engines are at
// half thrust, which is when a player can actually run it down and
// board it, Parkan-style.
inline bool isHailable(const Ship& sh) {
    if(!sh.active) return false;
    if(sh.role == Role::Trader) return true;
    return sh.hull < 0.25f && sh.hull > 0.0f;
}

// Returns the index of the nearest hailable NPC within HailRange of
// the player, or -1 if none. The SystemFlight loop uses this to gate
// the H prompt and the actual hail action.
inline int hailIdx(float px, float py, float pz) {
    int best = -1;
    float bestD2 = HailRange * HailRange;
    for(int i = 0; i < MaxNPCs; i++) {
        if(!isHailable(ships[i])) continue;
        float dx = ships[i].wx - px;
        float dy = ships[i].wy - py;
        float dz = ships[i].wz - pz;
        float d2 = dx * dx + dy * dy + dz * dz;
        if(d2 < bestD2) {
            bestD2 = d2;
            best = i;
        }
    }
    return best;
}

} // namespace NPCShip
