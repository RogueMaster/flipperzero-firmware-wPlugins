#pragma once
#include <gui/gui.h>
#include <furi.h>
#include "game_state.h"

// ── Animal species ────────────────────────────────────────────
typedef enum {
    SPECIES_RABBIT,
    SPECIES_DEER,
    SPECIES_BUFFALO,
    SPECIES_ELK,    // renamed deer in mountain/forest zones, same sprite
    SPECIES_WOLF,   // replaces buffalo in non-plains zones — exits left = maul
} Species;

typedef struct {
    Species  species;
    float    x;
    float    y;
    float    base_y;
    float    y_amp;
    float    y_phase;   // radians, randomised at spawn
    float    y_spd;     // radians/ms
    float    x_spd;     // px/ms
    int      meat_lo;
    int      meat_hi;
    bool     active;
} Animal;

// ── Hunt state ────────────────────────────────────────────────
#define MAX_ANIMALS      3
#define BULLET_ORIGIN_X  19
#define BULLET_Y         30   // barrel height — horizontal fire line
#define BULLET_SPD       0.22f // px/ms

typedef struct {
    Animal   animals[MAX_ANIMALS];
    float    bullet_x;
    bool     bullet_active;
    int      meat_this_hunt;
    bool     off_plains;
    bool     gored;
    bool     gored_by_wolf;
    char     msg[20];
    uint32_t msg_timer_ms;
    uint32_t spawn_timer_ms;
    uint32_t wolf_companion_timer; // ms until companion wolf spawns, 0=inactive
    bool     wolf_was_on_screen;   // true if wolf appeared — Back penalty applies
    int      pending_sound;
} HuntState;

void   hunt_init           (HuntState* h);
void   hunt_set_region     (HuntState* h, bool off_plains);
void   hunt_update         (HuntState* h, uint32_t dt_ms, GameState* gs);
void   hunt_fire           (HuntState* h, GameState* gs);
void   hunt_back_penalty   (HuntState* h, GameState* gs); // wolf bite on early exit
void   hunt_draw           (Canvas* c, const HuntState* h, const GameState* gs);
void   hunt_draw_gored_card(Canvas* c, bool by_wolf);
