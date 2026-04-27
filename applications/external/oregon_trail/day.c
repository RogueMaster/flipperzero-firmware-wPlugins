#include "day.h"
#include "game_state.h"
#include <furi.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

// ── LCG RNG ───────────────────────────────────────────────────
static uint32_t rng_state;
void day_init(void) { rng_state = (uint32_t)furi_get_tick(); }
static uint32_t rng_next(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}
static int rng_range(int lo, int hi) {
    return lo + (int)(rng_next() % (uint32_t)(hi - lo + 1));
}

// ── Food consumption per player per day (lbs) ─────────────────
static const int FOOD_PER_PLAYER_PER_DAY[4] = { 0, 5, 2, 1 };

// ── Miles per day by pace ─────────────────────────────────────
static const int MILES_PER_DAY[4] = { 0, 15, 22, 30 };

#define MILES_TO_OREGON 2000

// ── Calendar ─────────────────────────────────────────────────
static const int DAYS_IN_MONTH[] =
    { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static void advance_date(Trail* t) {
    t->day++;
    if(t->day > DAYS_IN_MONTH[t->month]) {
        t->day = 1;
        t->month++;
        if(t->month > 12) t->month = 1;
    }
}

// ── Weather risk table [region][month-1] ─────────────────────
// Values 0–10: higher = more severe weather event risk.
// Based on MECC's climate zone concept, simplified to 6 zones × 12 months.
// May (idx 4) departure is optimal — low risk early, escalating by October.
static const uint8_t WEATHER_RISK[6][12] = {
//   J  F  M  A  M  J  J  A  S  O  N  D
    {3, 2, 3, 3, 1, 1, 1, 2, 2, 4, 4, 5},  // PRAIRIE
    {3, 3, 4, 3, 1, 1, 2, 2, 3, 5, 5, 6},  // PLAINS
    {4, 4, 4, 4, 2, 2, 2, 3, 4, 6, 7, 7},  // HIGH PLAINS
    {7, 7, 6, 5, 3, 2, 3, 3, 5, 8, 9, 9},  // MOUNTAINS (most dangerous)
    {2, 2, 3, 3, 3, 5, 6, 6, 4, 3, 3, 3},  // DESERT (heat risk in summer)
    {5, 5, 4, 4, 3, 2, 2, 2, 4, 6, 7, 7},  // FOREST
};

static int weather_risk(const Trail* t) {
    Region r = region_from_miles(t->miles);
    int m = t->month - 1;
    if(m < 0) m = 0;
    if(m > 11) m = 11;
    return WEATHER_RISK[(int)r][m];
}

// ── Event table ───────────────────────────────────────────────
static const EventDef EVENTS[] = {

    // ── Illness ───────────────────────────────────────────────
    {
        .header = "! ILLNESS",
        .body   = { "%s has dysentery.", "Clean water and rest",
                    "are the only cure.", NULL },
        .choices = {
            { "Continue on",       0, 0,   0, -1.0f, false },
            { "Rest 2 days",       2, 0,   0, +1.0f, false },
            { "Buy medicine  $20", 0, 0, -20, +2.0f, true  },
        },
        .num_choices     = 3, .player_index = -1,
        .weight_base     = 8, .weight_low_food = 6,
        .weight_grueling = 4, .weight_bad_weather = 2,
        .weight_mountain = 2, .weight_desert = 0,
    },
    {
        .header = "! ILLNESS",
        .body   = { "%s has typhoid.", "The trail ahead is",
                    "long and difficult.", NULL },
        .choices = {
            { "Continue on",       0, 0,   0, -2.0f, false },
            { "Rest 5 days",       5, 0,   0, +1.0f, false },
            { "Buy medicine  $25", 0, 0, -25, +2.0f, true  },
        },
        .num_choices     = 3, .player_index = -1,
        .weight_base     = 5, .weight_low_food = 4,
        .weight_grueling = 6, .weight_bad_weather = 3,
        .weight_mountain = 3, .weight_desert = 0,
    },
    {
        .header = "! ILLNESS",
        .body   = { "%s has a bad cold.", "Push on or rest",
                    "to recover.", NULL },
        .choices = {
            { "Continue on", 0, 0, 0, -0.5f, false },
            { "Rest 1 day",  1, 0, 0,  0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 10, .weight_low_food = 3,
        .weight_grueling = 4,  .weight_bad_weather = 3,
        .weight_mountain = 2,  .weight_desert = 0,
    },
    {
        .header = "! ILLNESS",
        .body   = { "%s has pneumonia.", "Mountain cold air",
                    "filled their lungs.", NULL },
        .choices = {
            { "Continue on",       0, 0,   0, -2.0f, false },
            { "Rest 4 days",       4, 0,   0, +1.0f, false },
            { "Buy medicine  $30", 0, 0, -30, +2.0f, true  },
        },
        .num_choices     = 3, .player_index = -1,
        .weight_base     = 0,  .weight_low_food = 0,
        .weight_grueling = 0,  .weight_bad_weather = 4,
        .weight_mountain = 8,  .weight_desert = 0,   // mountains only
    },

    // ── Injury ────────────────────────────────────────────────
    {
        .header = "! INJURY",
        .body   = { "%s broke an arm", "fixing the wagon.",
                    "Lost 2 days work.", NULL },
        .choices = {
            { "Continue on", 0, 0, 0, -1.0f, false },
            { "Rest 3 days", 3, 0, 0, +1.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 5, .weight_low_food = 0,
        .weight_grueling = 3, .weight_bad_weather = 0,
        .weight_mountain = 2, .weight_desert = 0,
    },
    {
        .header = "! INJURY",
        .body   = { "%s was bitten by", "a rattlesnake.",
                    "Seek help quickly.", NULL },
        .choices = {
            { "Continue on",       0, 0,   0, -2.0f, false },
            { "Rest 4 days",       4, 0,   0, +1.0f, false },
            { "Buy antidote  $30", 0, 0, -30, +3.0f, true  },
        },
        .num_choices     = 3, .player_index = -1,
        .weight_base     = 0,  .weight_low_food = 0,
        .weight_grueling = 2,  .weight_bad_weather = 0,
        .weight_mountain = 0,  .weight_desert = 6,   // desert only
    },

    // ── Wagon trouble ─────────────────────────────────────────
    {
        .header = "WAGON TROUBLE",
        .body   = { "An axle cracked on", "the rocky pass.",
                    "Repairs needed.", NULL },
        .choices = {
            { "Hire repairman $25", 1, 0, -25, 0.0f, true  },
            { "Fix it yourself",   3, 0,   0, 0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 4,  .weight_low_food = 0,
        .weight_grueling = 5,  .weight_bad_weather = 1,
        .weight_mountain = 5,  .weight_desert = 0,
    },
    {
        .header = "WAGON TROUBLE",
        .body   = { "A wheel came off", "crossing the creek.",
                    "The wagon is stuck.", NULL },
        .choices = {
            { "Hire repairman $15", 1, 0, -15, 0.0f, true  },
            { "Fix it yourself",   2, 0,   0, 0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 6,  .weight_low_food = 0,
        .weight_grueling = 3,  .weight_bad_weather = 1,
        .weight_mountain = 2,  .weight_desert = 0,
    },
    {
        .header = "WAGON TROUBLE",
        .body   = { "The desert heat has", "shrunk the wagon",
                    "wheels dangerously.", NULL },
        .choices = {
            { "Hire repairman $20", 1, 0, -20, 0.0f, true  },
            { "Soak wheels",        2, 0,   0, 0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 0,  .weight_low_food = 0,
        .weight_grueling = 2,  .weight_bad_weather = 0,
        .weight_mountain = 0,  .weight_desert = 8,
    },

    // ── Weather ───────────────────────────────────────────────
    {
        .header = "BAD WEATHER",
        .body   = { "Heavy rain has", "turned the trail",
                    "to thick mud.", NULL },
        .choices = {
            { "Push through",    0, 0, 0, 0.0f, false },
            { "Wait it out  2d", 2, 0, 0, 0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 2,  .weight_low_food = 0,
        .weight_grueling = 0,  .weight_bad_weather = 6,
        .weight_mountain = 3,  .weight_desert = 0,
    },
    {
        .header = "EARLY FROST",
        .body   = { "A hard frost has", "hit the Cascades.", 
                    "Ice on the trail.", NULL },
        .choices = {
            { "Push through",   0, 0, 0, -0.5f, false },
            { "Wait 2 days",    2, 0, 0,  0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 0,  .weight_low_food = 0,
        .weight_grueling = 0,  .weight_bad_weather = 8,
        .weight_mountain = 0,  .weight_desert = 0,
        // fires in forest via weather_risk[FOREST][Oct+] = 6-7, × weight_bad_weather/5 = ~10-11
    },
    {
        .header = "BLIZZARD",
        .body   = { "A sudden blizzard", "has buried the trail.",
                    "You must wait.", NULL },
        .choices = {
            { "Wait 3 days", 3, -15, 0,  0.0f, false },
            { "Push on",     0,   0, 0, -1.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 0,  .weight_low_food = 0,
        .weight_grueling = 0,  .weight_bad_weather = 0,
        .weight_mountain = 10, .weight_desert = 0,  // mountains only
    },
    {
        .header = "FLASH FLOOD",
        .body   = { "A wall of water", "swept the trail.",
                    "Take cover!", NULL },
        .choices = {
            { "Wait 2 days", 2, 0, 0,  0.0f, false },
            { "Climb high",  1, 0, 0, -1.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 0,  .weight_low_food = 0,
        .weight_grueling = 0,  .weight_bad_weather = 5,
        .weight_mountain = 0,  .weight_desert = 4,
    },

    // ── Good fortune ──────────────────────────────────────────
    {
        .header = "GOOD FORTUNE",
        .body   = { "You found a trail", "guide who knows",
                    "a faster route!", NULL },
        .choices = {
            { "Follow  $5",       0, 0, -5, 0.0f, true  },
            { "Stay the course",  0, 0,  0, 0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 4,  .weight_low_food = 0,
        .weight_grueling = 0,  .weight_bad_weather = 0,
        .weight_mountain = 0,  .weight_desert = 0,
    },
    {
        .header = "GOOD FORTUNE",
        .body   = { "Wild berries along", "the trail.",
                    "+20 lbs of food!", NULL },
        .choices = {
            { "Gather berries", 0, -20, 0, 0.0f, false },
            { "Keep moving",    0,   0, 0, 0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 5,  .weight_low_food = 5,
        .weight_grueling = 0,  .weight_bad_weather = 0,
        .weight_mountain = 0,  .weight_desert = 0,
    },
    {
        .header = "GOOD FORTUNE",
        .body   = { "A passing merchant", "sells supplies",
                    "at fair prices.", NULL },
        .choices = {
            { "Buy food  $15", 0, -40, -15, 0.0f, true },
            { "No thanks",     0,   0,   0, 0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 4,  .weight_low_food = 6,
        .weight_grueling = 0,  .weight_bad_weather = 0,
        .weight_mountain = 0,  .weight_desert = 0,
    },

    // ── Supplies warning ──────────────────────────────────────
    {
        .header = "! SUPPLIES LOW",
        .body   = { "Food is running", "dangerously low.",
                    "Consider hunting.", NULL },
        .choices = {
            { "Continue on",   0, 0, 0, 0.0f, false },
            { "Reduce rations",0, 0, 0, 0.0f, false },
        },
        .num_choices     = 2, .player_index = -1,
        .weight_base     = 0,  .weight_low_food = 15,
        .weight_grueling = 0,  .weight_bad_weather = 0,
        .weight_mountain = 0,  .weight_desert = 0,
    },
};

#define NUM_EVENTS ((int)(sizeof(EVENTS) / sizeof(EVENTS[0])))

// ── Event selection ───────────────────────────────────────────
static const EventDef* pick_event(const GameState* gs) {
    int weights[NUM_EVENTS];
    int total = 0;

    bool low_food  = gs->trail.food_lbs < 50;
    bool grueling  = gs->trail.pace == 3;
    int  wrisks    = weather_risk(&gs->trail);  // 0–10
    Region region  = region_from_miles(gs->trail.miles);
    bool in_mountain = (region == REGION_HIGH_PLAINS || region == REGION_MOUNTAINS);
    bool in_desert   = (region == REGION_DESERT);

    for(int i = 0; i < NUM_EVENTS; i++) {
        weights[i] = EVENTS[i].weight_base
                   + (low_food     ? EVENTS[i].weight_low_food    : 0)
                   + (grueling     ? EVENTS[i].weight_grueling     : 0)
                   + (wrisks * EVENTS[i].weight_bad_weather / 5)
                   + (in_mountain  ? EVENTS[i].weight_mountain     : 0)
                   + (in_desert    ? EVENTS[i].weight_desert       : 0);
        if(weights[i] < 0) weights[i] = 0;
        total += weights[i];
    }

    if(total == 0) return NULL;

    int roll = rng_range(0, total - 1);
    for(int i = 0; i < NUM_EVENTS; i++) {
        roll -= weights[i];
        if(roll < 0) return &EVENTS[i];
    }
    return &EVENTS[NUM_EVENTS - 1];
}

// Pick a living player (random if player_index == -1)
static int pick_player(const GameState* gs, int player_index) {
    if(player_index >= 0 && player_index < gs->num_players)
        return player_index;
    // Collect living players
    int living[MAX_PLAYERS], n = 0;
    for(int i = 0; i < gs->num_players; i++)
        if(gs->players[i].hp > 0.0f) living[n++] = i;
    if(n == 0) return 0;
    return living[rng_range(0, n - 1)];
}

// ── Day advance ───────────────────────────────────────────────
DayResult day_advance(GameState* gs, ActiveEvent* ev) {
    Trail* t = &gs->trail;

    // Consume food — only living players eat
    int living = 0;
    for(int i = 0; i < gs->num_players; i++)
        if(gs->players[i].hp > 0.0f) living++;
    int consumed = FOOD_PER_PLAYER_PER_DAY[t->rations] * living;
    t->food_lbs -= consumed;
    if(t->food_lbs < 0) t->food_lbs = 0;

    // Advance miles
    t->miles += MILES_PER_DAY[t->pace];

    // Advance date
    advance_date(t);

    // ── Pace and rations HP effects ───────────────────────────
    // Strenuous: -0.5 HP every 4 days. Grueling: -0.5 HP every 2 days.
    // Meager: -0.5 HP every 6 days.   Bare bones: -0.5 HP every 3 days.
    // Use day counter mod to spread damage evenly.
    int day_num = t->day + t->month * 31; // rough monotonic counter
    bool drain_pace   = (t->pace == 2 && day_num % 4 == 0) ||
                        (t->pace == 3 && day_num % 2 == 0);
    bool drain_ration = (t->rations == 2 && day_num % 6 == 0) ||
                        (t->rations == 3 && day_num % 3 == 0);

    if(drain_pace || drain_ration) {
        for(int i = 0; i < gs->num_players; i++) {
            if(gs->players[i].hp > 0.0f) {
                gs->players[i].hp -= 0.5f;
                if(gs->players[i].hp < 0.0f) gs->players[i].hp = 0.0f;
            }
        }
    }

    // ── Oxen fatigue ──────────────────────────────────────────
    if(t->pace == 3) {
        t->grueling_days++;
        if(t->grueling_days >= 5) {
            // Force oxen exhausted event — mandatory 2-day rest
            t->grueling_days = 0;
            t->pace = 1; // reset to steady
            ev->active          = true;
            ev->def             = NULL; // handled specially below
            ev->scroll_y        = 0;
            ev->choice_cursor   = 0;
            ev->affected_player = 0;
            // Use a static EventDef for oxen exhaustion
            static const EventDef OXEN_EVENT = {
                .header = "OXEN EXHAUSTED",
                .body   = { "The oxen can't go", "on. Forced rest", "for 2 days.", NULL },
                .choices = { { "Rest 2 days", 2, 0, 0, 0.0f, false } },
                .num_choices = 1, .player_index = -1,
                .weight_base = 0,
            };
            ev->def = &OXEN_EVENT;
            return DAY_EVENT;
        }
    } else {
        t->grueling_days = 0; // reset on any non-grueling day
    }

    // Win condition
    if(t->miles >= MILES_TO_OREGON) return DAY_ARRIVED;

    // Starvation — if food is 0, all living players lose 1.0 HP
    if(t->food_lbs == 0) {
        for(int i = 0; i < gs->num_players; i++) {
            if(gs->players[i].hp > 0.0f) {
                gs->players[i].hp -= 1.0f;
                if(gs->players[i].hp < 0.0f) gs->players[i].hp = 0.0f;
            }
        }
        bool all_dead = true;
        for(int i = 0; i < gs->num_players; i++)
            if(gs->players[i].hp > 0.0f) { all_dead = false; break; }
        if(all_dead) return DAY_ALL_DEAD;
        return DAY_STARVING;
    }

    // Random event — fires roughly every 5-7 days on average
    // Roll 1-10; event fires on 1-2 (20% chance per day).
    // Grueling pace raises to 30%, low food raises to 35%.
    int event_threshold = 2;
    if(t->pace == 3)      event_threshold++;
    if(t->food_lbs < 50)  event_threshold++;

    int roll = rng_range(1, 10);
    if(roll <= event_threshold) {
        const EventDef* def = pick_event(gs);
        if(def) {
            ev->active          = true;
            ev->def             = def;
            ev->scroll_y        = 0;
            ev->choice_cursor   = 0;
            ev->affected_player = pick_player(gs, def->player_index);
            return DAY_EVENT;
        }
    }

    return DAY_OK;
}

// ── Apply a player's choice ───────────────────────────────────
void event_apply_choice(GameState* gs, ActiveEvent* ev, int choice_idx) {
    if(!ev->active || !ev->def) return;
    if(choice_idx < 0 || choice_idx >= ev->def->num_choices) return;

    const EventChoice* ch = &ev->def->choices[choice_idx];
    Trail*  t  = &gs->trail;
    Player* p  = &gs->players[ev->affected_player];

    // Day cost — advance calendar and consume food each day
    if(ch->day_cost > 0) {
        int living = 0;
        for(int i = 0; i < gs->num_players; i++)
            if(gs->players[i].hp > 0.0f) living++;
        int food_per_day = FOOD_PER_PLAYER_PER_DAY[t->rations] * living;
        for(int d = 0; d < ch->day_cost; d++) {
            advance_date(t);
            t->food_lbs -= food_per_day;
            if(t->food_lbs < 0) t->food_lbs = 0;
        }
    }

    // Food (negative food_cost = gain food)
    t->food_lbs -= ch->food_cost;
    if(t->food_lbs < 0) t->food_lbs = 0;

    // Money
    t->money += ch->money_cost;  // money_cost is negative for spending
    if(t->money < 0) t->money = 0;

    // HP delta clamped to [0.0, MAX_HP_F]
    if(ch->hp_delta != 0.0f) {
        p->hp += ch->hp_delta;
        if(p->hp < 0.0f)     p->hp = 0.0f;
        if(p->hp > MAX_HP_F) p->hp = MAX_HP_F;
    }

    ev->active = false;
}

// ── Rest day ─────────────────────────────────────────────────
// Advance date, consume food (living players only), restore 0.5 HP each.
void day_rest(GameState* gs, int* food_used_out) {
    Trail* t = &gs->trail;

    int living = 0;
    for(int i = 0; i < gs->num_players; i++)
        if(gs->players[i].hp > 0.0f) living++;
    int food_per = FOOD_PER_PLAYER_PER_DAY[t->rations];
    int consumed = food_per * living;
    t->food_lbs  = (t->food_lbs > consumed) ? t->food_lbs - consumed : 0;

    for(int i = 0; i < gs->num_players; i++) {
        Player* p = &gs->players[i];
        if(p->hp > 0.0f && p->hp < MAX_HP_F) {
            p->hp += 0.5f;
            if(p->hp > MAX_HP_F) p->hp = MAX_HP_F;
        }
    }

    advance_date(t);

    if(food_used_out) *food_used_out = consumed;
}

// ── Scroll helpers ────────────────────────────────────────────
#define VISIBLE_LINES 5

bool event_scroll_up(ActiveEvent* ev) {
    if(!ev->active || ev->scroll_y == 0) return false;
    ev->scroll_y--;
    return true;
}

bool event_scroll_down(ActiveEvent* ev) {
    if(!ev->active) return false;
    // Count body lines
    int n = 0;
    while(n < MAX_BODY_LINES && ev->def->body[n]) n++;
    if(ev->scroll_y + VISIBLE_LINES < n) {
        ev->scroll_y++;
        return true;
    }
    return false;
}
