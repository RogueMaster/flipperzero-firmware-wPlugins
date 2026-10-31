#pragma once
#include "Platform.h"
#include <math.h>
#include <stdint.h>
#include "GameState.h"
#include "NPCShip.h"
#include "Ship3D.h"
#include "Faction.h"
#include "Quest.h"
#include "Rank.h"
#include "Audio.h"
#include "Particles.h"

// Laser combat.
//
// Player presses W → `tryPlayerFire()` casts a forward cone from
// the cockpit and damages the nearest NPC inside it. Damage and fire
// rate both scale with `game.laserTier` (Pulse / Beam / Military) —
// no overheat anymore; the per-tier cooldown is the throttle.
//
// Pirates with R19's `attacking` flag fire back on a per-ship cooldown,
// damaging the player's shield then spilling onto the hull. A small
// visual "flash" timer per shot drives the laser-bolt render pass that
// lives in SystemFlight.
//
// NPC HP (shields + hull) is stored on NPCShip::Ship directly so kill
// detection works whether or not the player initiated it.

namespace Combat {

// --- Tuning ---------------------------------------------------------------
constexpr float MaxLaserRange = 3500.0f; // sysu — past this you miss
constexpr float LaserConeRad = 0.10f; // ~5.7° half-angle for aim
// Fire rate is the cap now that overheat is gone — pricier weapons fire
// faster as well as harder.
constexpr float PlayerFireCDByTier[3] = {0.30f, 0.20f, 0.13f};
constexpr float NPCFireCD = 1.25f; // seconds between pirate shots
constexpr float FlashLifetime = 0.18f; // s — bullet travel time

// Per-tier player damage to NPC. Pulse / Beam / Military.
// Tuned up — Cardputer aim is fiddly, so each successful hit needs to
// pay off more or kills feel hopeless.
constexpr float DamageByTier[3] = {0.20f, 0.34f, 0.55f};
constexpr float NPCDamage = 0.07f; // applied to player shield/hull

constexpr int BountyTenthsCR = 250; // 25.0 CR per pirate kill

// --- Flash state for the laser-bolt visual --------------------------------
struct Flash {
    float t; // seconds remaining
    int shipIdx; // for NPC flashes: which ship fired; for player: target index (or -1)
    uint16_t color;
};

inline Flash playerFlash = {0.0f, -1, 0};
inline Flash npcFlash[NPCShip::MaxNPCs];

// Player-hit visual feedback timer (seconds). Set when the player takes
// damage; renderer flashes a red viewport border + a small shake while
// > 0 so hits read on a small screen.
inline float playerHitFlash = 0.0f;
constexpr float PlayerHitFlashTime = 0.25f;

// Where recent hits came from, for the arc the scanner lights on its
// rim. World-space source position (the shooter / missile / rammer at
// impact), so the arc swings round as the player turns. A few slots so
// a crossfire shows both sides.
struct HitMark {
    float wx, wy, wz, t;
};
constexpr int MaxHitMarks = 3;
constexpr float HitMarkTime = 1.2f; // s
inline HitMark hitMarks[MaxHitMarks];

inline void markHit(float wx, float wy, float wz) {
    // Reuse the slot of a still-showing hit from (nearly) the same spot,
    // else the one closest to expiry.
    int best = 0;
    for(int i = 0; i < MaxHitMarks; i++) {
        const HitMark& h = hitMarks[i];
        float dx = h.wx - wx, dy = h.wy - wy, dz = h.wz - wz;
        if(h.t > 0.0f && dx * dx + dy * dy + dz * dz < 600.0f * 600.0f) {
            best = i;
            break;
        }
        if(h.t < hitMarks[best].t) best = i;
    }
    HitMark& h = hitMarks[best];
    h.wx = wx;
    h.wy = wy;
    h.wz = wz;
    h.t = HitMarkTime;
}

inline void resetFlashes() {
    playerFlash.t = 0.0f;
    playerHitFlash = 0.0f;
    for(int i = 0; i < MaxHitMarks; i++)
        hitMarks[i].t = 0.0f;
    for(int i = 0; i < NPCShip::MaxNPCs; i++)
        npcFlash[i].t = 0.0f;
}

// Decay all flash timers; call once per frame.
// Last kill, for the cockpit message.
inline float killToast = 0.0f;
inline NPCShip::Role killRole = NPCShip::Role::Pirate;

inline void tick(float dt) {
    if(playerFlash.t > 0.0f) {
        playerFlash.t -= dt;
        if(playerFlash.t < 0.0f) playerFlash.t = 0.0f;
    }
    if(killToast > 0.0f) killToast -= dt;
    if(playerHitFlash > 0.0f) {
        playerHitFlash -= dt;
        if(playerHitFlash < 0.0f) playerHitFlash = 0.0f;
    }
    for(int i = 0; i < MaxHitMarks; i++) {
        if(hitMarks[i].t > 0.0f) hitMarks[i].t -= dt;
    }
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        if(npcFlash[i].t > 0.0f) {
            npcFlash[i].t -= dt;
            if(npcFlash[i].t < 0.0f) npcFlash[i].t = 0.0f;
        }
    }
}

// Common bookkeeping for a player-caused NPC kill. Used by laser
// hits and ramming collisions so a ram counts the same as a clean
// shot — bounty, kill counter, faction shift, quest progress, rank
// promotion check, and the death particle burst.
inline void registerPlayerKill(GameState& g, NPCShip::Ship& sh) {
    killToast = 2.0f;
    killRole = sh.role;
    Particles::spawnBurst(sh.wx, sh.wy, sh.wz, 0xFD20, 40);
    Audio::explosion();
    g.kills++;
    if(sh.role == NPCShip::Role::Pirate) {
        g.credits += BountyTenthsCR;
        Quest::onPirateKill(g);
    }
    Faction::applyKill(g, sh.role, (Faction::Id)sh.homeFaction);
    Rank::checkPromotion(g);
    sh.active = false;
    NPCShip::numActive--;
    if(NPCShip::numActive < 0) NPCShip::numActive = 0;
}

// --- Player → NPC fire ----------------------------------------------------
inline uint16_t playerColorForTier(uint8_t tier) {
    if(tier == 0) return 0xFD20; // orange (pulse)
    if(tier == 1) return 0xF800; // red (beam)
    return 0x07FF; // cyan (military)
}

// Cooldown is held outside `tryPlayerFire` so callers can keep the
// "Space held to fire" pattern without managing it themselves.
inline float playerCooldown = 0.0f;

inline void tickPlayerCooldown(float dt) {
    playerCooldown -= dt;
    if(playerCooldown < 0.0f) playerCooldown = 0.0f;
}

// Returns true if a shot was fired this call. fx/fy/fz is the player's
// forward unit vector; px/py/pz is the player's world position.
inline bool
    tryPlayerFire(GameState& g, float px, float py, float pz, float fx, float fy, float fz) {
    if(playerCooldown > 0.0f) return false;
    // Critically wounded — gunnery is offline below 25% hull. Matches
    // the same rule applied to NPCs in updateNPCs.
    if(g.hull < 0.25f) return false;
    uint8_t t = g.laserTier < 3 ? g.laserTier : 2;
    playerCooldown = PlayerFireCDByTier[t];

    // Hit test: project each ship onto the forward ray and accept if its
    // perpendicular distance is within the hull's hit radius. This makes
    // the hitbox match the visible silhouette — a Freighter / Barge is a
    // much fatter target than an Interceptor, and the old cone-only check
    // missed shots that visibly clipped the hull.
    int bestI = -1;
    float bestD = 1e9f;
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        const auto& sh = NPCShip::ships[i];
        if(!sh.active) continue;
        float dx = sh.wx - px, dy = sh.wy - py, dz = sh.wz - pz;
        float fwd = dx * fx + dy * fy + dz * fz; // along ray
        if(fwd < 1.0f || fwd > MaxLaserRange) continue;
        float dist2 = dx * dx + dy * dy + dz * dz;
        float perp2 = dist2 - fwd * fwd;
        if(perp2 < 0.0f) perp2 = 0.0f;
        float hitR = Ship3D::scaleFor(sh.modelId); // hull half-extent
        if(perp2 > hitR * hitR) continue;
        if(fwd < bestD) {
            bestD = fwd;
            bestI = i;
        }
    }

    playerFlash.t = FlashLifetime;
    playerFlash.shipIdx = bestI;
    playerFlash.color = playerColorForTier(g.laserTier);
    // R28: laser zap — pitch tracks the equipped tier.
    Audio::laserZap(g.laserTier);

    if(bestI < 0) return true; // fired but missed

    float dmg = DamageByTier[g.laserTier];
    auto& sh = NPCShip::ships[bestI];
    // Hit confirmation — on its own channel so it layers over the zap
    // instead of cutting it off.
    Audio::hitTarget();
    // Player shot this ship — it fights back from now on, regardless of
    // role. Cargo traders included.
    sh.provoked = true;
    // Hit sparks at the ship's world position. Color seeded from the
    // laser tier so each weapon reads as its own impact flash.
    Particles::spawnBurst(sh.wx, sh.wy, sh.wz, playerColorForTier(g.laserTier), 10);
    if(sh.shields > 0.0f) {
        sh.shields -= dmg;
        if(sh.shields < 0.0f) {
            // Excess damage spills into hull.
            sh.hull += sh.shields;
            sh.shields = 0.0f;
        }
    } else {
        sh.hull -= dmg;
    }

    if(sh.hull <= 0.0f) {
        registerPlayerKill(g, sh);
    }
    return true;
}

// --- NPC → Player fire ----------------------------------------------------
// Shield absorbs first; anything past it spills onto the hull. Used by
// both laser and missile impacts; (sx, sy, sz) is where the hit came
// from, for the scanner's hit-direction arc.
inline void damagePlayer(GameState& g, float amount, float sx, float sy, float sz) {
    bool shielded = g.shield > 0.0f;
    if(shielded) {
        g.shield -= amount;
        if(g.shield < 0.0f) {
            // Excess spills past the shield onto the hull.
            g.hull += g.shield;
            g.shield = 0.0f;
        }
    } else {
        g.hull -= amount;
    }
    if(g.hull < 0.0f) g.hull = 0.0f;
    // Fizz while the shield soaks it, metal crunch once it's the hull —
    // plus a power-down sweep on the hit that collapses the shield.
    if(shielded)
        Audio::shieldHit();
    else
        Audio::hullHit();
    if(shielded && g.shield <= 0.0f) Audio::shieldDown();
    playerHitFlash = PlayerHitFlashTime;
    markHit(sx, sy, sz);
}

// Direct hull damage — collisions bypass shields entirely so a ram is
// always punishing.
inline void damagePlayerHull(GameState& g, float amount, float sx, float sy, float sz) {
    g.hull -= amount;
    if(g.hull < 0.0f) g.hull = 0.0f;
    Audio::hullHit();
    playerHitFlash = PlayerHitFlashTime;
    markHit(sx, sy, sz);
}

inline void updateNPCs(GameState& g, float dt) {
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        auto& sh = NPCShip::ships[i];
        if(!sh.active) continue;

        // Pirates fire when in attack cone. Provoked ships (any role the
        // player has shot) fire the same way. Otherwise the ship is
        // peaceful and just keeps its cooldown topped up. Critically
        // wounded ships (hull < 25%) can no longer fire — their gunnery
        // systems are wrecked along with the rest of the hull.
        bool canFire = sh.attacking && (sh.role == NPCShip::Role::Pirate || sh.provoked) &&
                       sh.hull >= 0.25f;
        if(!canFire) {
            sh.fireTimer = NPCFireCD;
            continue;
        }
        sh.fireTimer -= dt;
        if(sh.fireTimer > 0.0f) continue;
        sh.fireTimer = NPCFireCD;

        npcFlash[i].t = FlashLifetime;
        npcFlash[i].shipIdx = i;
        npcFlash[i].color = 0xF800; // red
        damagePlayer(g, NPCDamage, sh.wx, sh.wy, sh.wz);
    }
}

} // namespace Combat
