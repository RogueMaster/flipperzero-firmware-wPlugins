#pragma once
#include "Platform.h"
#include <math.h>
#include <stdint.h>
#include "GameState.h"
#include "NPCShip.h"
#include "Faction.h"
#include "Rank.h"
#include "Audio.h"
#include "Combat.h"

// Refit R21: missiles + ECM.
//
// A small pool of homing projectiles shared by the player and pirates.
// Player missiles steer toward a chosen NPC (the lock target set in
// SystemFlight); pirate missiles steer toward the player. Hits dump a
// big damage chunk into shields → hull. ECM (purchased via R18's EQUIP)
// detonates every missile within a generous radius on a cooldown.

namespace Missile {

// Pool size — keeps RAM bounded. 4 player missiles cap + a handful of
// in-flight pirate missiles fits comfortably.
constexpr int MaxMissiles = 6;

constexpr float Speed = 1100.0f; // sysu/s — outruns CruiseSpeed (700)
constexpr float TurnRate = 2.0f; // 1/s steering blend
constexpr float Lifetime = 9.0f; // s before self-destruct
constexpr float HitRadius = 220.0f; // sysu — proximity fuse
constexpr float NPCDamage = 0.60f; // applied to NPC shields/hull
constexpr float PlayerDamage = 0.45f; // applied to player shields

constexpr float ECMRadius = 4500.0f; // sysu — blast wipes missiles
constexpr float ECMCooldown = 6.0f; // s between ECM uses

struct Projectile {
    bool active;
    bool fromPlayer; // true: player → NPC; false: NPC → player
    int targetIdx; // NPC slot for player missiles (-1 if lost)
    int sourceIdx; // NPC slot that fired (npc → player only)
    float wx, wy, wz;
    float vx, vy, vz;
    float life;
    uint16_t color;
};

inline Projectile pool[MaxMissiles];

inline void resetAll() {
    for(int i = 0; i < MaxMissiles; i++)
        pool[i].active = false;
}

inline int freeSlot() {
    for(int i = 0; i < MaxMissiles; i++)
        if(!pool[i].active) return i;
    return -1;
}

inline int countActive() {
    int n = 0;
    for(int i = 0; i < MaxMissiles; i++)
        if(pool[i].active) n++;
    return n;
}

// True if any active NPC missile is currently homing on the player —
// the HUD lights a red "INCOMING" badge off this.
inline bool incomingToPlayer() {
    for(int i = 0; i < MaxMissiles; i++) {
        if(pool[i].active && !pool[i].fromPlayer) return true;
    }
    return false;
}

// Distance to the closest NPC missile homing on the player, or -1 if
// none. Drives the incoming-missile beeper's tempo.
inline float nearestIncomingDist(float px, float py, float pz) {
    float best = -1.0f;
    for(int i = 0; i < MaxMissiles; i++) {
        const Projectile& m = pool[i];
        if(!m.active || m.fromPlayer) continue;
        float dx = m.wx - px, dy = m.wy - py, dz = m.wz - pz;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if(best < 0.0f || d < best) best = d;
    }
    return best;
}

// Player launches a missile at `targetNPC`. Spawns the projectile a few
// sysu in front of the cockpit and arms it with the player's forward
// velocity vector at full Speed. Returns false if pool is full / no lock.
inline bool
    spawnPlayer(float px, float py, float pz, float fx, float fy, float fz, int targetNPC) {
    if(targetNPC < 0) return false;
    int slot = freeSlot();
    if(slot < 0) return false;
    Projectile& m = pool[slot];
    m.active = true;
    m.fromPlayer = true;
    m.targetIdx = targetNPC;
    m.sourceIdx = -1;
    m.wx = px + fx * 80.0f;
    m.wy = py + fy * 80.0f;
    m.wz = pz + fz * 80.0f;
    m.vx = fx * Speed;
    m.vy = fy * Speed;
    m.vz = fz * Speed;
    m.life = Lifetime;
    m.color = 0xFFE0; // yellow
    Audio::missileLaunch(); // R28
    return true;
}

// NPC fires at the player. Spawned in front of the firing ship along its
// current yaw. Returns false if pool full.
inline bool spawnNPC(int shipIdx) {
    if(shipIdx < 0 || shipIdx >= NPCShip::MaxNPCs) return false;
    if(!NPCShip::ships[shipIdx].active) return false;
    int slot = freeSlot();
    if(slot < 0) return false;
    const auto& sh = NPCShip::ships[shipIdx];
    float cy = cosf(sh.yaw), sy = sinf(sh.yaw);
    Projectile& m = pool[slot];
    m.active = true;
    m.fromPlayer = false;
    m.targetIdx = -1;
    m.sourceIdx = shipIdx;
    m.wx = sh.wx + sy * 80.0f;
    m.wy = sh.wy;
    m.wz = sh.wz + cy * 80.0f;
    m.vx = sy * Speed;
    m.vy = 0.0f;
    m.vz = cy * Speed;
    m.life = Lifetime;
    m.color = 0xF800; // red
    return true;
}

// Pirates attempt to launch a homing missile while `attacking`. Launches
// are rate-limited per ship via `Ship::missileTimer` and gated by a
// per-roll probability so volleys feel like a credible threat, not spam.
constexpr float PiratePoll = 4.0f; // s between fire attempts per ship
constexpr float PirateFireChance = 0.35f; // probability on each poll

inline void tickPirateLaunches(GameState& g, float dt) {
    (void)g;
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        auto& sh = NPCShip::ships[i];
        if(!sh.active) continue;
        if(sh.role != NPCShip::Role::Pirate || !sh.attacking) {
            sh.missileTimer = PiratePoll;
            continue;
        }
        // Critically wounded ships can't launch missiles either — same
        // rule as the laser-fire gate in Combat::updateNPCs.
        if(sh.hull < 0.25f) {
            sh.missileTimer = PiratePoll;
            continue;
        }
        sh.missileTimer -= dt;
        if(sh.missileTimer > 0.0f) continue;
        sh.missileTimer = PiratePoll;
        // Cheap pseudo-random: mix slot index with millis. Don't need
        // determinism here — combat is already noisy.
        uint32_t r = ((uint32_t)millis() * 2654435761u) ^ ((uint32_t)i * 0x9E3779B9u);
        float roll = (float)(r & 0xFFFFu) / 65536.0f;
        if(roll < PirateFireChance) spawnNPC(i);
    }
}

// Per-frame: home toward target, integrate, check proximity hit.
inline void update(GameState& g, float dt, float playerX, float playerY, float playerZ) {
    for(int i = 0; i < MaxMissiles; i++) {
        Projectile& m = pool[i];
        if(!m.active) continue;
        m.life -= dt;
        if(m.life <= 0.0f) {
            m.active = false;
            continue;
        }

        // Pick the chase target this frame.
        float tx = 0, ty = 0, tz = 0;
        bool haveTarget = false;
        if(m.fromPlayer) {
            if(m.targetIdx >= 0 && m.targetIdx < NPCShip::MaxNPCs &&
               NPCShip::ships[m.targetIdx].active) {
                const auto& sh = NPCShip::ships[m.targetIdx];
                tx = sh.wx;
                ty = sh.wy;
                tz = sh.wz;
                haveTarget = true;
            }
        } else {
            tx = playerX;
            ty = playerY;
            tz = playerZ;
            haveTarget = true;
        }

        if(haveTarget) {
            float dx = tx - m.wx, dy = ty - m.wy, dz = tz - m.wz;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if(dist > 1.0f) {
                float dvx = dx / dist * Speed;
                float dvy = dy / dist * Speed;
                float dvz = dz / dist * Speed;
                float k = TurnRate * dt;
                if(k > 1.0f) k = 1.0f;
                m.vx += (dvx - m.vx) * k;
                m.vy += (dvy - m.vy) * k;
                m.vz += (dvz - m.vz) * k;
                float vmag = sqrtf(m.vx * m.vx + m.vy * m.vy + m.vz * m.vz);
                if(vmag > 1.0f) {
                    m.vx = m.vx / vmag * Speed;
                    m.vy = m.vy / vmag * Speed;
                    m.vz = m.vz / vmag * Speed;
                }
            }
        }

        m.wx += m.vx * dt;
        m.wy += m.vy * dt;
        m.wz += m.vz * dt;

        // Proximity hit check.
        if(m.fromPlayer) {
            if(m.targetIdx >= 0 && m.targetIdx < NPCShip::MaxNPCs) {
                auto& sh = NPCShip::ships[m.targetIdx];
                if(sh.active) {
                    float dx = sh.wx - m.wx, dy = sh.wy - m.wy, dz = sh.wz - m.wz;
                    if(dx * dx + dy * dy + dz * dz < HitRadius * HitRadius) {
                        if(sh.shields > 0.0f) {
                            sh.shields -= NPCDamage;
                            if(sh.shields < 0.0f) {
                                sh.hull += sh.shields;
                                sh.shields = 0.0f;
                            }
                        } else {
                            sh.hull -= NPCDamage;
                        }
                        if(sh.hull <= 0.0f) {
                            // Same bookkeeping as a laser kill: bounty, kill count,
                            // standing, quest progress, rank, explosion.
                            Combat::registerPlayerKill(g, sh);
                        } else {
                            Audio::hitTarget();
                            Particles::spawnBurst(sh.wx, sh.wy, sh.wz, 0xFFE0, 16);
                        }
                        m.active = false;
                        continue;
                    }
                }
            }
        } else {
            float dx = playerX - m.wx, dy = playerY - m.wy, dz = playerZ - m.wz;
            if(dx * dx + dy * dy + dz * dz < HitRadius * HitRadius) {
                Combat::damagePlayer(g, PlayerDamage, m.wx, m.wy, m.wz);
                m.active = false;
                continue;
            }
        }
    }
}

// ECM trigger: detonate every active missile inside ECMRadius of player.
// Returns the number wiped (mostly for HUD toast / debug).
inline int triggerECM(float px, float py, float pz) {
    int count = 0;
    for(int i = 0; i < MaxMissiles; i++) {
        Projectile& m = pool[i];
        if(!m.active) continue;
        float dx = m.wx - px, dy = m.wy - py, dz = m.wz - pz;
        if(dx * dx + dy * dy + dz * dz < ECMRadius * ECMRadius) {
            m.active = false;
            count++;
        }
    }
    Audio::ecmBurst(); // R28: fires whenever the burst goes off, even no-hit
    return count;
}

} // namespace Missile
