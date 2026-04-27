#pragma once
#include <stdint.h>
#include <stdbool.h>

// ── Screen identifiers ────────────────────────────────────────
typedef enum {
    SCREEN_TITLE,
    SCREEN_NAME_EDIT,
    SCREEN_TRAIL,
    SCREEN_EVENT,
    SCREEN_HUNT,
    SCREEN_RIVER,
    SCREEN_RIVER_OUTCOME,
    SCREEN_MAP,
    SCREEN_REST,
    SCREEN_FORT_ARRIVAL,  // splash: arrived at fort — Right to trade, Back to continue
    SCREEN_FORT,          // the actual store
    SCREEN_DEATH,
    SCREEN_GAME_OVER,
    SCREEN_WIN,
} Screen;

// ── Party member ─────────────────────────────────────────────
#define MAX_PLAYERS   2
#define MAX_HP        4
#define NAME_LEN      11   // 10 usable chars + null ("Offspring" = 9 fits cleanly)

typedef struct {
    char  name[NAME_LEN];
    float hp;       // 0.0 .. MAX_HP, supports 0.5 increments
    bool  alive;
} Player;

#define MAX_HP        4
#define MAX_HP_F      4.0f

typedef enum {
    REGION_PRAIRIE    = 0,
    REGION_PLAINS     = 1,
    REGION_HIGH_PLAINS= 2,
    REGION_MOUNTAINS  = 3,
    REGION_DESERT     = 4,
    REGION_FOREST     = 5,
    REGION_COUNT      = 6,
} Region;

static inline Region region_from_miles(uint32_t miles) {
    if(miles < 300)  return REGION_PRAIRIE;
    if(miles < 640)  return REGION_PLAINS;
    if(miles < 947)  return REGION_HIGH_PLAINS;
    if(miles < 1200) return REGION_MOUNTAINS;
    if(miles < 1600) return REGION_DESERT;
    return REGION_FOREST;
}

// Plains regions use buffalo + deer; off-plains use wolf + elk
static inline bool region_is_plains(Region r) {
    return r <= REGION_HIGH_PLAINS;
}

// ── Trail / inventory ─────────────────────────────────────────
typedef struct {
    int      food_lbs;
    int      money;
    int      day;
    int      month;
    uint32_t miles;
    int      pace;
    int      rations;
    bool     recent_rain;
    int      ammo;
    int      last_fort_visited;
    uint8_t  rivers_visited;
    int      grueling_days;     // consecutive days at grueling pace → oxen fatigue
} Trail;

// ── Master game state ─────────────────────────────────────────
typedef struct {
    Screen  screen;
    Player  players[MAX_PLAYERS];
    int     num_players;
    Trail   trail;
    bool    running;
    int     settings_cursor; // 0=pace 1=rations on settings page
} GameState;
