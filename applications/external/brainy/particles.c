#include "particles.h"
#include <stdlib.h>

// Spawn a particle at the leading edge for the given direction.
// Safe margins keep the 3x3 box (±1px) fully on-screen.
static void particle_spawn(Particle* p, ParticleDir dir) {
    p->t     = 0;
    p->shape = (uint8_t)(rand() % 2);
    switch(dir) {
    case ParticleDirRight:
        p->x  = 1;
        p->y  = (uint8_t)(1 + rand() % 62);
        p->vx = (int8_t)(1 + rand() % 3);
        p->vy = (int8_t)((rand() % 3) - 1);
        break;
    case ParticleDirLeft:
        p->x  = 126;
        p->y  = (uint8_t)(1 + rand() % 62);
        p->vx = (int8_t)(-(1 + rand() % 3));
        p->vy = (int8_t)((rand() % 3) - 1);
        break;
    case ParticleDirDown:
        p->x  = (uint8_t)(1 + rand() % 126);
        p->y  = 1;
        p->vx = (int8_t)((rand() % 3) - 1);
        p->vy = (int8_t)(1 + rand() % 3);
        break;
    case ParticleDirUp:
        p->x  = (uint8_t)(1 + rand() % 126);
        p->y  = 62;
        p->vx = (int8_t)((rand() % 3) - 1);
        p->vy = (int8_t)(-(1 + rand() % 3));
        break;
    }
}

void particle_system_init(ParticleSystem* ps) {
    ps->current_phase = 0;
    for(int i = 0; i < MAX_PARTICLES; i++) {
        particle_spawn(&ps->items[i], ParticleDirRight);
        ps->items[i].x = (uint8_t)(1 + rand() % 126);  // stagger across screen
    }
    ps->next_at = furi_get_tick();
}

void particle_system_update(ParticleSystem* ps) {
    uint32_t now = furi_get_tick();
    if(now < ps->next_at) return;
    ps->next_at = now + PARTICLE_MS;

    uint32_t new_phase = (now / (PARTICLE_DIR_SECS * 1000u)) % 4u;
    ParticleDir dir = (ParticleDir)new_phase;

    // Direction changed: respawn all particles spread across the screen so
    // the transition looks like an incoming wave rather than a sudden flash.
    if(new_phase != ps->current_phase) {
        ps->current_phase = new_phase;
        for(int i = 0; i < MAX_PARTICLES; i++) {
            particle_spawn(&ps->items[i], dir);
            if(dir == ParticleDirRight || dir == ParticleDirLeft) {
                ps->items[i].x = (uint8_t)(1 + rand() % 126);
            } else {
                ps->items[i].y = (uint8_t)(1 + rand() % 62);
            }
        }
    }

    for(int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &ps->items[i];
        p->t++;

        int16_t nx = (int16_t)p->x;
        int16_t ny = (int16_t)p->y;

        if(dir == ParticleDirRight || dir == ParticleDirLeft) {
            // Primary motion: horizontal. Vertical drift every 3 frames.
            nx += (int16_t)p->vx;
            if(p->t % 3 == 0) ny += (int16_t)p->vy;
            if(ny < 1) ny = 1;
            if(ny > 62) ny = 62;
        } else {
            // Primary motion: vertical. Horizontal drift every 3 frames.
            ny += (int16_t)p->vy;
            if(p->t % 3 == 0) nx += (int16_t)p->vx;
            if(nx < 1) nx = 1;
            if(nx > 126) nx = 126;
        }

        if(nx < 0 || nx > 127 || ny < 0 || ny > 63) {
            particle_spawn(p, dir);
        } else {
            p->x = (uint8_t)nx;
            p->y = (uint8_t)ny;
        }
    }
}

void particle_system_draw(const ParticleSystem* ps, Canvas* canvas) {
    for(int i = 0; i < MAX_PARTICLES; i++) {
        const Particle* p = &ps->items[i];
        uint8_t x = p->x, y = p->y;
        if(p->shape == 0) {
            // Plus:  010
            //        111
            //        010
            canvas_draw_dot(canvas, x,     y - 1);
            canvas_draw_dot(canvas, x - 1, y);
            canvas_draw_dot(canvas, x,     y);
            canvas_draw_dot(canvas, x + 1, y);
            canvas_draw_dot(canvas, x,     y + 1);
        } else {
            // Cross: 101
            //        010
            //        101
            canvas_draw_dot(canvas, x - 1, y - 1);
            canvas_draw_dot(canvas, x + 1, y - 1);
            canvas_draw_dot(canvas, x,     y);
            canvas_draw_dot(canvas, x - 1, y + 1);
            canvas_draw_dot(canvas, x + 1, y + 1);
        }
    }
}
