#include "river.h"
#include "sprites.h"
#include <furi.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── LCG RNG (local copy — same seed pattern as hunt/day) ─────
static uint32_t rng_state;
static uint32_t rng_next(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}
static int rng_range(int lo, int hi) {
    return lo + (int)(rng_next() % (uint32_t)(hi - lo + 1));
}

// ── Seasonal depth modifier ───────────────────────────────────
// Snowmelt peaks May/June, rivers run low Aug/Sep
static float seasonal_depth_mod(int month) {
    static const float MOD[13] = {
        0.0f,   // unused
        0.8f,   // Jan — frozen/low
        0.6f,   // Feb
        0.8f,   // Mar — starting to rise
        1.2f,   // Apr — rising
        1.5f,   // May — peak snowmelt ← optimal departure month
        1.5f,   // Jun — still high
        0.8f,   // Jul — dropping
        0.2f,   // Aug — low
        0.0f,   // Sep — low
       -0.2f,   // Oct — very low
       -0.1f,   // Nov
        0.4f,   // Dec
    };
    if(month < 1 || month > 12) return 0.0f;
    return MOD[month];
}

// ── Compute current from depth ────────────────────────────────
static Current depth_to_current(float depth) {
    if(depth < 3.0f) return CURRENT_SLOW;
    if(depth < 4.5f) return CURRENT_MODERATE;
    return CURRENT_FAST;
}

// ── Risk level 0-3 for skull display ─────────────────────────
static int risk_level(float depth, Current current) {
    int r = 0;
    if(depth >= 3.0f) r++;
    if(depth >= 4.5f) r++;
    if(current >= CURRENT_MODERATE) r++;
    if(r > 3) r = 3;
    return r;
}

// ── Init river state ─────────────────────────────────────────
void river_init(RiverState* r, int river_idx, const Trail* t) {
    rng_state = (uint32_t)furi_get_tick();
    memset(r, 0, sizeof(RiverState));
    r->active     = true;
    r->river_idx  = river_idx;
    r->cursor     = 1;  // default to ferry (safest default)
    r->affected_player = -1;

    // Compute depth
    float base  = RIVERS[river_idx].base_depth;
    float depth = base + seasonal_depth_mod(t->month);
    // Peak snowmelt in May/June adds significant depth on top of seasonal mod
    if(t->month == 5 || t->month == 6) depth += 1.5f;
    if(t->recent_rain) depth += 0.8f;
    if(depth < 0.5f) depth = 0.5f;
    r->depth   = depth;
    r->current = depth_to_current(depth);
}

// ── Ford outcome roll ────────────────────────────────────────
// Probability table based on depth + current.
// Returns FordOutcome and populates food_lost and affected_player.
static FordOutcome roll_ford(RiverState* r, const GameState* gs) {
    float depth   = r->depth;
    Current cur   = r->current;

    // Build weights [SAFE, ROUGH, DISASTROUS, CATASTROPHIC]
    int w[4] = {0, 0, 0, 0};

    if(depth < 2.5f && cur == CURRENT_SLOW) {
        w[0]=70; w[1]=25; w[2]=5;  w[3]=0;
    } else if(depth < 3.5f && cur <= CURRENT_MODERATE) {
        w[0]=40; w[1]=35; w[2]=20; w[3]=5;
    } else if(depth < 4.5f) {
        w[0]=20; w[1]=30; w[2]=35; w[3]=15;
    } else {
        w[0]=5;  w[1]=20; w[2]=35; w[3]=40;
    }

    int total = w[0]+w[1]+w[2]+w[3];
    int roll = rng_range(0, total-1);
    FordOutcome outcome;
    if(roll < w[0])              outcome = FORD_SAFE;
    else if(roll < w[0]+w[1])    outcome = FORD_ROUGH;
    else if(roll < w[0]+w[1]+w[2]) outcome = FORD_DISASTROUS;
    else                         outcome = FORD_CATASTROPHIC;

    // Food lost
    switch(outcome) {
        case FORD_SAFE:         r->food_lost = 0;                  break;
        case FORD_ROUGH:        r->food_lost = rng_range(20, 60);  break;
        case FORD_DISASTROUS:   r->food_lost = rng_range(40, 80);  break;
        case FORD_CATASTROPHIC: r->food_lost = rng_range(60, 120); break;
    }

    // Affected player (for DISASTROUS — one player)
    if(outcome == FORD_DISASTROUS) {
        // Pick a living player
        int living[MAX_PLAYERS], n = 0;
        for(int i = 0; i < gs->num_players; i++)
            if(gs->players[i].hp > 0.0f) living[n++] = i;
        r->affected_player = (n > 0) ? living[rng_range(0, n-1)] : 0;
    } else if(outcome == FORD_CATASTROPHIC) {
        r->affected_player = -1; // both
    }

    return outcome;
}

// ── Apply choice ─────────────────────────────────────────────
void river_commit(RiverState* r, GameState* gs) {
    Trail* t = &gs->trail;

    switch(r->cursor) {
        case 0: // Ford
            r->outcome = roll_ford(r, gs);
            // Apply food loss
            t->food_lbs -= r->food_lost;
            if(t->food_lbs < 0) t->food_lbs = 0;
            // Apply HP loss
            if(r->outcome == FORD_DISASTROUS) {
                gs->players[r->affected_player].hp -= 1.0f;
                if(gs->players[r->affected_player].hp < 0.0f)
                    gs->players[r->affected_player].hp = 0.0f;
            } else if(r->outcome == FORD_CATASTROPHIC) {
                for(int i = 0; i < gs->num_players; i++) {
                    if(gs->players[i].hp > 0.0f) {
                        gs->players[i].hp -= 0.5f;
                        if(gs->players[i].hp < 0.0f)
                            gs->players[i].hp = 0.0f;
                    }
                }
            }
            r->outcome_shown = false;
            break;

        case 1: // Ferry
            r->outcome      = FORD_SAFE;
            r->food_lost    = 0;
            t->money       -= RIVERS[r->river_idx].ferry_cost;
            if(t->money < 0) t->money = 0;
            r->outcome_shown = false;
            break;

        case 2: // Wait 1 day
            // Advance date, consume food, reduce depth slightly
            t->day++;
            if(t->day > 31) { t->day = 1; t->month++; }
            int living = 0;
            for(int i = 0; i < gs->num_players; i++)
                if(gs->players[i].hp > 0.0f) living++;
            t->food_lbs -= 3 * living;
            if(t->food_lbs < 0) t->food_lbs = 0;
            r->depth -= 0.5f;
            if(r->depth < 0.5f) r->depth = 0.5f;
            r->current = depth_to_current(r->depth);
            // Don't mark outcome — stay on river screen with updated depth
            return;
    }
    r->active = true; // outcome card will be shown
}

// ── Draw river crossing screen ────────────────────────────────
void river_draw(Canvas* c, const RiverState* r, const GameState* gs) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // ── Header ───────────────────────────────────────────────
    canvas_draw_box(c, 0, 0, 128, 9);
    canvas_set_color(c, ColorWhite);
    const char* seasons[] = {"","Win","Win","Spr","Spr","Spr","Sum","Sum","Sum","Aut","Aut","Win","Win"};
    int m = gs->trail.month;
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%s  %s",
             RIVERS[r->river_idx].name,
             (m>=1&&m<=12) ? seasons[m] : "");
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, hdr);
    canvas_set_color(c, ColorBlack);

    // ── River graphic — 3 sine wave lines, 50px wide ─────────
    int rx = 10, ry = 13, rw = 50;
    for(int line = 0; line < 3; line++) {
        int y = ry + line * 6;
        for(int x = 0; x < rw-1; x++) {
            int y0 = y + (int)(2.5f * sinf((x)      * 0.28f + line * 1.2f));
            int y1 = y + (int)(2.5f * sinf((x + 1)  * 0.28f + line * 1.2f));
            canvas_draw_line(c, rx+x, y0, rx+x+1, y1);
        }
    }

    // ── Depth and current indicators ─────────────────────────
    int ix = 68; // info panel x
    char dep_str[16];
    snprintf(dep_str, sizeof(dep_str), "Depth: %.1f ft", (double)r->depth);
    canvas_draw_str_aligned(c, ix, 13, AlignLeft, AlignTop, dep_str);

    const char* cur_names[] = { "SLOW", "MODERATE", "FAST" };
    char cur_str[18];
    snprintf(cur_str, sizeof(cur_str), "Curr: %s", cur_names[r->current]);
    canvas_draw_str_aligned(c, ix, 21, AlignLeft, AlignTop, cur_str);

    // Risk skulls
    int skulls = risk_level(r->depth, r->current);
    for(int s = 0; s < skulls; s++)
        sprite_skull(c, ix + s * 8, 30);

    // ── Options ───────────────────────────────────────────────
    canvas_draw_line(c, 0, 38, 127, 38);

    // Ferry cost
    char ferry_label[24];
    snprintf(ferry_label, sizeof(ferry_label), "  Take Ferry ($%d)",
             RIVERS[r->river_idx].ferry_cost);

    const char* labels[3] = {
        "  Ford River",
        ferry_label,
        "  Wait 1 Day",
    };

    // Risk suffixes
    const char* ford_risk[] = { "Low", "Med", "High", "!!!" };
    int fr = risk_level(r->depth, r->current);
    char ford_sfx[8];
    snprintf(ford_sfx, sizeof(ford_sfx), "%s", ford_risk[fr]);

    for(int i = 0; i < 3; i++) {
        int y = 40 + i * 8;
        bool sel = (r->cursor == i);
        if(sel) {
            canvas_draw_box(c, 0, y-1, 128, 8);
            canvas_set_color(c, ColorWhite);
            canvas_draw_str_aligned(c, 4, y, AlignLeft, AlignTop, "\x15");
        }
        canvas_draw_str_aligned(c, 6, y, AlignLeft, AlignTop, labels[i]);
        if(i == 0)  // Ford shows risk
            canvas_draw_str_aligned(c, 126, y, AlignRight, AlignTop, ford_sfx);
        if(i == 1)  // Ferry is always safe
            canvas_draw_str_aligned(c, 126, y, AlignRight, AlignTop, "Safe");
        canvas_set_color(c, ColorBlack);
    }
}

// ── Draw outcome card ─────────────────────────────────────────
void river_draw_outcome(Canvas* c, const RiverState* r, const GameState* gs) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, 0, 0, 128, 64);
    canvas_draw_box(c, 1, 1, 126, 10);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);

    const char* headers[] = {
        "SAFE CROSSING",
        "ROUGH CROSSING",
        "DISASTER!",
        "CATASTROPHE!",
    };
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, headers[r->outcome]);
    canvas_set_color(c, ColorBlack);

    switch(r->outcome) {
        case FORD_SAFE:
            canvas_draw_str_aligned(c, 64, 14, AlignCenter, AlignTop, "The wagon crossed");
            canvas_draw_str_aligned(c, 64, 22, AlignCenter, AlignTop, "without incident.");
            break;
        case FORD_ROUGH: {
            char ln[32];
            snprintf(ln, sizeof(ln), "The wagon tipped.");
            canvas_draw_str_aligned(c, 64, 14, AlignCenter, AlignTop, ln);
            snprintf(ln, sizeof(ln), "Lost %d lb of food.", r->food_lost);
            canvas_draw_str_aligned(c, 64, 22, AlignCenter, AlignTop, ln);
            break;
        }
        case FORD_DISASTROUS: {
            int p = r->affected_player;
            canvas_draw_str_aligned(c, 64, 14, AlignCenter, AlignTop, "The current swept");
            char ln[32];
            snprintf(ln, sizeof(ln), "%s under!", gs->players[p].name);
            canvas_draw_str_aligned(c, 64, 22, AlignCenter, AlignTop, ln);
            snprintf(ln, sizeof(ln), "Lost %d lb food.", r->food_lost);
            canvas_draw_str_aligned(c, 64, 30, AlignCenter, AlignTop, ln);
            canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignTop, "-1 HP");
            break;
        }
        case FORD_CATASTROPHIC:
            canvas_draw_str_aligned(c, 64, 14, AlignCenter, AlignTop, "The wagon capsized!");
            canvas_draw_str_aligned(c, 64, 22, AlignCenter, AlignTop, "Everyone swept away.");
            char ln2[32];
            snprintf(ln2, sizeof(ln2), "Lost %d lb food.", r->food_lost);
            canvas_draw_str_aligned(c, 64, 30, AlignCenter, AlignTop, ln2);
            canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignTop, "-0.5 HP each");
            break;
    }

    canvas_draw_line(c, 1, 50, 126, 50);
    canvas_draw_str_aligned(c, 64, 53, AlignCenter, AlignTop, "OK: Continue");
}
