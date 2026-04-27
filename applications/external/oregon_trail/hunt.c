#include "hunt.h"
#include "sound.h"
#include "sprites.h"
#include <furi.h>
#include <gui/gui.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── Species parameter tables ──────────────────────────────────
// Each species gets fixed base_y, y_amp, x_spd, meat range.
// y_phase is randomised per spawn so animals don't sync up.
typedef struct {
    float base_y;
    float y_amp;
    float y_spd;    // radians/ms — how fast it bobs
    float x_spd;    // px/ms
    int   meat_lo;
    int   meat_hi;
} SpeciesParams;

static const SpeciesParams PARAMS[5] = {
    [SPECIES_RABBIT]  = { .base_y=29, .y_amp=5,  .y_spd=0.00140f, .x_spd=0.034f, .meat_lo=2,   .meat_hi=6   },
    [SPECIES_DEER]    = { .base_y=22, .y_amp=7,  .y_spd=0.00080f, .x_spd=0.020f, .meat_lo=40,  .meat_hi=80  },
    [SPECIES_BUFFALO] = { .base_y=23, .y_amp=6,  .y_spd=0.00050f, .x_spd=0.011f, .meat_lo=100, .meat_hi=200 },
    [SPECIES_ELK]     = { .base_y=22, .y_amp=7,  .y_spd=0.00080f, .x_spd=0.018f, .meat_lo=50,  .meat_hi=100 },
    [SPECIES_WOLF]    = { .base_y=26, .y_amp=8,  .y_spd=0.00120f, .x_spd=0.028f, .meat_lo=20,  .meat_hi=40  },
};

// Minimum ms between spawn attempts (prevents immediate refill)
#define SPAWN_COOLDOWN_MS 2000

// ── Pseudo-random helpers ─────────────────────────────────────
// Simple LCG seeded from furi_get_tick() — good enough for a game.
static uint32_t rng_state = 0;

static void rng_seed(void) {
    rng_state = (uint32_t)furi_get_tick();
}

static uint32_t rng_next(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

// Returns integer in [lo, hi] inclusive
static int rng_range(int lo, int hi) {
    return lo + (int)(rng_next() % (uint32_t)(hi - lo + 1));
}

// Returns float in [0.0, 1.0)
static float rng_float(void) {
    return (float)(rng_next() & 0xFFFF) / 65536.0f;
}

// ── Spawn a single animal slot ────────────────────────────────
static Species spawn_animal(Animal* a, bool off_plains) {
    Species pool[3];
    if(!off_plains) {
        pool[0] = SPECIES_RABBIT;
        pool[1] = SPECIES_DEER;
        pool[2] = SPECIES_BUFFALO;
    } else {
        pool[0] = SPECIES_RABBIT;
        pool[1] = SPECIES_ELK;
        pool[2] = SPECIES_WOLF;
    }
    Species sp = pool[rng_range(0, 2)];
    const SpeciesParams* p = &PARAMS[sp];

    a->species  = sp;
    a->x        = 128.0f + 8.0f;
    a->base_y   = p->base_y;
    a->y_amp    = p->y_amp;
    a->y_phase  = rng_float() * 6.283f;
    a->y_spd    = p->y_spd;
    a->x_spd    = p->x_spd;
    a->meat_lo  = p->meat_lo;
    a->meat_hi  = p->meat_hi;
    a->y        = a->base_y;
    a->active   = true;

    return sp;  // return species for companion check
}

// Spawn a companion wolf — faster, slightly different y offset
static void spawn_companion_wolf(Animal* a) {
    const SpeciesParams* p = &PARAMS[SPECIES_WOLF];
    a->species  = SPECIES_WOLF;
    a->x        = 128.0f + 16.0f;  // slightly behind lead wolf entry
    a->base_y   = p->base_y + 4.0f; // different lane
    a->y_amp    = p->y_amp;
    a->y_phase  = rng_float() * 6.283f;
    a->y_spd    = p->y_spd * 1.2f;
    a->x_spd    = p->x_spd * 1.3f;  // 30% faster — the aggressive one
    a->meat_lo  = p->meat_lo;
    a->meat_hi  = p->meat_hi;
    a->y        = a->base_y;
    a->active   = true;
}

// ── Hit bounding boxes ────────────────────────────────────────
// Returns true if bullet_x (at y=BULLET_Y) overlaps this animal.
// Note: bounding box is relative to a->x, a->y.
static bool hit_check(const Animal* a, int bx) {
    int x1, x2, y1, y2;
    switch(a->species) {
        case SPECIES_RABBIT:
            x1=(int)a->x;    x2=x1+4;
            y1=(int)a->y;    y2=y1+4;
            break;
        case SPECIES_DEER:
            x1=(int)a->x-1;  x2=(int)a->x+12;
            y1=(int)a->y;    y2=(int)a->y+12;
            break;
        case SPECIES_BUFFALO:
        default:
            x1=(int)a->x-4;  x2=(int)a->x+12;
            y1=(int)a->y;    y2=(int)a->y+11;
            break;
    }
    return (bx >= x1) && (bx <= x2) && (BULLET_Y >= y1) && (BULLET_Y <= y2);
}

// ── Public API ────────────────────────────────────────────────

void hunt_init(HuntState* h) {
    rng_seed();
    bool off_plains = h->off_plains;
    memset(h, 0, sizeof(HuntState));
    h->off_plains = off_plains;
    h->pending_sound = -1;
    for(int i = 0; i < MAX_ANIMALS; i++) {
        (void)spawn_animal(&h->animals[i], h->off_plains);
        h->animals[i].x = 50.0f + i * 32.0f;
    }
}

void hunt_set_region(HuntState* h, bool off_plains) {
    h->off_plains = off_plains;
}

void hunt_update(HuntState* h, uint32_t dt_ms, GameState* gs) {
    if(h->gored) return; // paused on goring card

    uint32_t elapsed_ms = furi_get_tick();

    // ── Move animals ─────────────────────────────────────────
    for(int i = 0; i < MAX_ANIMALS; i++) {
        Animal* a = &h->animals[i];
        if(!a->active) continue;

        a->x -= a->x_spd * dt_ms;
        a->y  = a->base_y + a->y_amp * sinf(elapsed_ms * a->y_spd + a->y_phase);

        // Exited left edge
        if(a->x < -20.0f) {
                if(a->species == SPECIES_BUFFALO || a->species == SPECIES_WOLF) {
                    h->gored        = true;
                    h->gored_by_wolf = (a->species == SPECIES_WOLF);
                    h->pending_sound = SND_HUNT_GORED;
                    for(int p = 0; p < gs->num_players; p++) {
                        if(gs->players[p].hp > 0.0f) {
                            gs->players[p].hp -= 2.0f;
                            if(gs->players[p].hp < 0.0f) gs->players[p].hp = 0.0f;
                            break;
                        }
                    }
                    a->active = false;
                } else {
                    a->active = false;
                }
            }
    }

    // ── Spawn new animals ────────────────────────────────────
    int active_count = 0;
    for(int i = 0; i < MAX_ANIMALS; i++)
        if(h->animals[i].active) active_count++;

    if(active_count < MAX_ANIMALS) {
        h->spawn_timer_ms += dt_ms;
        if(h->spawn_timer_ms >= SPAWN_COOLDOWN_MS) {
            for(int i = 0; i < MAX_ANIMALS; i++) {
                if(!h->animals[i].active) {
                    Species sp = spawn_animal(&h->animals[i], h->off_plains);
                    h->spawn_timer_ms = 0;
                    // 20% chance wolf triggers a delayed companion
                    if(sp == SPECIES_WOLF && h->wolf_companion_timer == 0
                       && rng_range(1, 5) == 1) {
                        h->wolf_companion_timer = rng_range(3000, 5000); // 3-5 sec delay
                    }
                    break;
                }
            }
        }
    } else {
        h->spawn_timer_ms = 0;
    }

    // ── Wolf companion timer ──────────────────────────────────
    if(h->wolf_companion_timer > 0) {
        h->wolf_companion_timer = (h->wolf_companion_timer > dt_ms)
                                   ? h->wolf_companion_timer - dt_ms : 0;
        if(h->wolf_companion_timer == 0) {
            // Find a free slot and spawn the faster companion
            for(int i = 0; i < MAX_ANIMALS; i++) {
                if(!h->animals[i].active) {
                    spawn_companion_wolf(&h->animals[i]);
                    break;
                }
            }
        }
    }

    // ── Track if wolf was ever on screen (for back penalty) ──
    for(int i = 0; i < MAX_ANIMALS; i++) {
        if(h->animals[i].active && h->animals[i].species == SPECIES_WOLF
           && h->animals[i].x < 128.0f) {
            h->wolf_was_on_screen = true;
        }
    }

    // ── Move bullet ─────────────────────────────────────────
    if(h->bullet_active) {
        h->bullet_x += BULLET_SPD * dt_ms;

        // Check hits — break immediately on first impact (no punch-through)
        for(int i = 0; i < MAX_ANIMALS; i++) {
            Animal* a = &h->animals[i];
            if(!a->active) continue;
            if(hit_check(a, (int)h->bullet_x)) {
                const char* species_names[] = {"Rabbit", "Deer", "Buffalo", "Elk", "Wolf"};
                int meat = a->meat_lo + rng_range(0, a->meat_hi - a->meat_lo);
                gs->trail.food_lbs += meat;
                h->meat_this_hunt  += meat;
                snprintf(h->msg, sizeof(h->msg), "%s! +%d lb",
                         species_names[a->species], meat);
                h->msg_timer_ms = 2200;
                h->bullet_active = false;
                a->active = false;
                h->pending_sound = SND_HUNT_HIT;
                break;  // bullet stops here
            }
        }

        // Bullet left screen
        if(h->bullet_active && h->bullet_x >= 128.0f) {
            h->bullet_active = false;
            strncpy(h->msg, "MISS", sizeof(h->msg));
            h->msg_timer_ms = 900;
            h->pending_sound = SND_HUNT_MISS;
        }
    }

    // ── Decay message timer ──────────────────────────────────
    if(h->msg_timer_ms > 0) {
        h->msg_timer_ms = (h->msg_timer_ms > dt_ms) ? h->msg_timer_ms - dt_ms : 0;
    }
}

void hunt_back_penalty(HuntState* h, GameState* gs) {
    if(!h->wolf_was_on_screen) return;
    // Wolf gave chase — first living player takes 1 HP
    for(int i = 0; i < gs->num_players; i++) {
        if(gs->players[i].hp > 0.0f) {
            gs->players[i].hp -= 1.0f;
            if(gs->players[i].hp < 0.0f) gs->players[i].hp = 0.0f;
            break;
        }
    }
    // Show as gored card so player sees the consequence
    h->gored        = true;
    h->gored_by_wolf = true;
}

void hunt_fire(HuntState* h, GameState* gs) {
    if(h->gored || h->bullet_active || gs->trail.ammo <= 0) return;
    gs->trail.ammo--;
    h->bullet_active = true;
    h->bullet_x = (float)BULLET_ORIGIN_X;
}

// ── Draw ─────────────────────────────────────────────────────

void hunt_draw(Canvas* c, const HuntState* h, const GameState* gs) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    if(h->gored) {
        hunt_draw_gored_card(c, h->gored_by_wolf);
        return;
    }

    // Horizon hills (sine curve)
    for(int x = 0; x < 128; x++) {
        int hy = 25 + (int)(4.0f * sinf(x * 0.08f));
        canvas_draw_dot(c, x, hy);
    }

    // Ground band
    canvas_draw_box(c, 0, 38, 128, 2);
    for(int x = 4; x < 128; x += 5)
        canvas_draw_box(c, x, 37, 1, 2);

    // Stickman: head top y=27, feet y=38, barrel y=30, tip x=19
    sprite_shooter(c, 2, 27);

    // Animals (back to front — buffalo first so others overdraw)
    for(int i = 0; i < MAX_ANIMALS; i++) {
        const Animal* a = &h->animals[i];
        if(!a->active || a->x < -20.0f || a->x > 140.0f) continue;
        int ix = (int)a->x, iy = (int)a->y;
        switch(a->species) {
            case SPECIES_RABBIT:  sprite_rabbit (c, ix, iy); break;
            case SPECIES_DEER:    sprite_deer   (c, ix, iy); break;
            case SPECIES_BUFFALO: sprite_buffalo(c, ix, iy); break;
            case SPECIES_ELK:     sprite_deer   (c, ix, iy); break; // same sprite
            case SPECIES_WOLF:    sprite_wolf   (c, ix, iy); break;
            default: break;
        }
    }

    // Bullet dot (4×2px for visibility)
    if(h->bullet_active && h->bullet_x > 0.0f && h->bullet_x < 128.0f) {
        canvas_draw_box(c, (int)h->bullet_x, BULLET_Y, 4, 2);
    }

    // ── Top-left: hit message only ────────────────────────────
    canvas_set_font(c, FontSecondary);
    if(h->msg_timer_ms > 0) {
        canvas_draw_str(c, 2, 8, h->msg);
    }

    // ── HUD bar: Meat left, Ammo right ───────────────────────
    canvas_draw_line(c, 0, 41, 127, 41);

    char meat_str[16];
    snprintf(meat_str, sizeof(meat_str), "Meat:%d", h->meat_this_hunt);
    canvas_draw_str_aligned(c, 2, 44, AlignLeft, AlignTop, meat_str);

    char ammo_str[16];
    snprintf(ammo_str, sizeof(ammo_str), "Ammo:%d", gs->trail.ammo);
    canvas_draw_str_aligned(c, 127, 44, AlignRight, AlignTop, ammo_str);

    // ── Bottom bar: controls ──────────────────────────────────
    canvas_draw_line(c, 0, 54, 127, 54);
    canvas_draw_str_aligned(c, 127, 56, AlignRight, AlignTop, "OK:Fire  Back:Exit Hunt");
}

void hunt_draw_gored_card(Canvas* c, bool by_wolf) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, 0, 0, 128, 64);
    canvas_draw_box(c, 1, 1, 126, 10);
    canvas_set_color(c, ColorWhite);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop,
                            by_wolf ? "!! MAULED !!" : "!! GORED !!");
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);
    if(by_wolf) {
        canvas_draw_str_aligned(c, 64, 13, AlignCenter, AlignTop, "You were mauled");
        canvas_draw_str_aligned(c, 64, 21, AlignCenter, AlignTop, "by a wolf. Don't");
        canvas_draw_str_aligned(c, 64, 29, AlignCenter, AlignTop, "be a chew toy!");
    } else {
        canvas_draw_str_aligned(c, 64, 13, AlignCenter, AlignTop, "You were gored by a");
        canvas_draw_str_aligned(c, 64, 21, AlignCenter, AlignTop, "buffalo. We should");
        canvas_draw_str_aligned(c, 64, 29, AlignCenter, AlignTop, "put up warning signs!");
    }
    canvas_draw_str_aligned(c, 64, 43, AlignCenter, AlignTop, "HP -2");
    canvas_draw_str_aligned(c, 64, 53, AlignCenter, AlignTop, "OK: Continue");
}
