#pragma once
#include <stdint.h>
#include <gui/canvas.h>
#include <furi.h>

#define MAX_PARTICLES 14
#define PARTICLE_MS   50u   // ~20 fps
#define PARTICLE_DIR_SECS 5u  // seconds per direction phase

typedef enum {
    ParticleDirRight = 0,  // spawn left  edge, drift right
    ParticleDirLeft  = 1,  // spawn right edge, drift left
    ParticleDirDown  = 2,  // spawn top   edge, drift down
    ParticleDirUp    = 3,  // spawn bottom edge, drift up
} ParticleDir;

typedef struct {
    uint8_t x, y;
    int8_t  vx;     // signed: + = right, - = left
    int8_t  vy;     // signed: + = down,  - = up
    uint8_t t;      // frame counter (for drift every 3 frames)
    uint8_t shape;  // 0 = plus (+), 1 = cross (x)
} Particle;

typedef struct {
    Particle     items[MAX_PARTICLES];
    uint32_t     next_at;
    uint32_t     current_phase;
} ParticleSystem;

void particle_system_init(ParticleSystem* ps);
void particle_system_update(ParticleSystem* ps);
void particle_system_draw(const ParticleSystem* ps, Canvas* canvas);
