#pragma once
#include "game_state.h"
#include <stdbool.h>
#include <stdint.h>

// ── Event choice ──────────────────────────────────────────────
typedef struct {
    const char* label;          // shown on screen, e.g. "Rest 3 days"
    int         day_cost;       // extra days lost
    int         food_cost;      // food consumed (can be negative = gain)
    int         money_cost;     // dollars spent
    float       hp_delta;       // applied to affected player (-2.0, +1.0, etc.)
    bool        requires_money; // grey out if broke
} EventChoice;

// ── Event definition ──────────────────────────────────────────
#define MAX_CHOICES   3
#define MAX_BODY_LINES 6

typedef struct {
    const char*  header;                        // inverted title bar text
    const char*  body[MAX_BODY_LINES];          // scrollable lines (NULL = end)
    EventChoice  choices[MAX_CHOICES];
    int          num_choices;
    int          player_index;                  // which player is affected (-1 = all)
    int          weight_base;
    int          weight_low_food;
    int          weight_grueling;
    int          weight_bad_weather; // scales with weather_risk table (0-10)
    int          weight_mountain;    // extra in HIGH_PLAINS + MOUNTAINS zones
    int          weight_desert;      // extra in DESERT zone
} EventDef;

// ── Active event (runtime state) ──────────────────────────────
typedef struct {
    bool         active;
    const EventDef* def;
    int          scroll_y;          // which body line is topmost (0-based)
    int          choice_cursor;     // highlighted choice
    int          affected_player;   // resolved from def->player_index
} ActiveEvent;

// ── Day result ────────────────────────────────────────────────
typedef enum {
    DAY_OK,
    DAY_EVENT,      // random event fired — populate active_event
    DAY_STARVING,   // food hit 0
    DAY_ALL_DEAD,   // entire party gone
    DAY_ARRIVED,    // reached Oregon — win
} DayResult;

// ── Public API ────────────────────────────────────────────────
void      day_init        (void);
DayResult day_advance     (GameState* gs, ActiveEvent* ev);
void      day_rest        (GameState* gs, int* food_used_out); // rest 1 day: +1HP, -food, +date
void      event_apply_choice(GameState* gs, ActiveEvent* ev, int choice_idx);
bool      event_scroll_up (ActiveEvent* ev);
bool      event_scroll_down(ActiveEvent* ev);
