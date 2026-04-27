#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "game_state.h"
#include <gui/gui.h>

// ── River definitions ─────────────────────────────────────────
#define NUM_RIVERS 3

typedef struct {
    const char* name;
    uint32_t    miles;      // trigger mile marker
    float       base_depth; // ft at neutral conditions
    int         ferry_cost; // $ for ferry option
} RiverDef;

static const RiverDef RIVERS[NUM_RIVERS] = {
    { "Big Blue River",  100,  2.5f, 10 },
    { "South Platte R.", 250,  3.8f, 15 },
    { "Snake River",    1300,  4.2f, 20 },
};

// ── Current strength ──────────────────────────────────────────
typedef enum { CURRENT_SLOW, CURRENT_MODERATE, CURRENT_FAST } Current;

// ── Outcome ───────────────────────────────────────────────────
typedef enum {
    FORD_SAFE,          // crossed without incident
    FORD_ROUGH,         // lost food, wagon got wet
    FORD_DISASTROUS,    // lost food + 1HP to one player
    FORD_CATASTROPHIC,  // lost food + 0.5HP to both players
} FordOutcome;

// ── Active river state ────────────────────────────────────────
typedef struct {
    bool        active;
    int         river_idx;   // index into RIVERS[]
    float       depth;       // computed depth this crossing
    Current     current;
    int         cursor;      // 0=ford 1=ferry 2=wait
    bool        outcome_shown;
    FordOutcome outcome;
    int         food_lost;
    int         affected_player; // -1 = both, index = one player
} RiverState;

// ── Public API ────────────────────────────────────────────────
void river_init    (RiverState* r, int river_idx, const Trail* t);
void river_commit  (RiverState* r, GameState* gs); // apply chosen option
void river_draw    (Canvas* c, const RiverState* r, const GameState* gs);
void river_draw_outcome(Canvas* c, const RiverState* r, const GameState* gs);
