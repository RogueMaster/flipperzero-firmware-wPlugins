#pragma once
#include "Platform.h"
#include <math.h>
#include <stdint.h>

// Tiny world-space particle pool used for hit sparks when the player
// damages an NPC ship. Particles live in world coordinates; the
// SystemFlight renderer projects them through the shared camera so they
// inherit perspective with the rest of the scene.

namespace Particles {

constexpr int MaxParticles = 64;
constexpr float SpeedMin = 90.0f; // sysu/s
constexpr float SpeedMax = 240.0f; // sysu/s
constexpr float LifeMin = 0.25f;
constexpr float LifeMax = 0.55f;

struct Particle {
    bool alive;
    float wx, wy, wz;
    float vx, vy, vz;
    float life; // remaining seconds
    float life0; // initial life (for fade)
    uint16_t color;
};

inline Particle pool[MaxParticles];
inline uint32_t rngState = 0xA1B2C3D4u;

inline uint32_t rnd() {
    rngState = rngState * 1664525u + 1013904223u;
    return rngState;
}

inline float rndUnit() {
    // 0..1
    return (float)(rnd() & 0xFFFFu) / 65535.0f;
}

inline float rndSigned() {
    // -1..1
    return rndUnit() * 2.0f - 1.0f;
}

inline void reset() {
    for(int i = 0; i < MaxParticles; i++)
        pool[i].alive = false;
}

// Spawn a burst of `count` particles at (wx, wy, wz) with random
// velocities in a sphere. `color` seeds the spark color; the renderer
// fades it toward yellow as life shortens.
inline void spawnBurst(float wx, float wy, float wz, uint16_t color, int count) {
    if(count > MaxParticles) count = MaxParticles;
    int placed = 0;
    for(int i = 0; i < MaxParticles && placed < count; i++) {
        Particle& p = pool[i];
        if(p.alive) continue;
        float vx = rndSigned();
        float vy = rndSigned();
        float vz = rndSigned();
        float n = sqrtf(vx * vx + vy * vy + vz * vz);
        if(n < 0.001f) {
            vx = 1.0f;
            vy = vz = 0.0f;
            n = 1.0f;
        }
        float speed = SpeedMin + rndUnit() * (SpeedMax - SpeedMin);
        float k = speed / n;
        p.alive = true;
        p.wx = wx;
        p.wy = wy;
        p.wz = wz;
        p.vx = vx * k;
        p.vy = vy * k;
        p.vz = vz * k;
        p.life0 = LifeMin + rndUnit() * (LifeMax - LifeMin);
        p.life = p.life0;
        p.color = color;
        placed++;
    }
}

inline void tick(float dt) {
    for(int i = 0; i < MaxParticles; i++) {
        Particle& p = pool[i];
        if(!p.alive) continue;
        p.life -= dt;
        if(p.life <= 0.0f) {
            p.alive = false;
            continue;
        }
        p.wx += p.vx * dt;
        p.wy += p.vy * dt;
        p.wz += p.vz * dt;
    }
}

} // namespace Particles
