#include <stdio.h>
#include "expedition.h"
#include "hunt.h"          /* catch_roll */
#include "tuning.h"
#include "random_generator.h"

void expedition_start(struct GameState *gs, uint16_t minutes, uint32_t now) {
    gs->persistent.on_expedition = 1;
    gs->persistent.expedition_start = now;
    gs->persistent.expedition_minutes = minutes;
}

static uint32_t end_time(const struct PersistentGameState *p) {
    return p->expedition_start + (uint32_t)p->expedition_minutes * EXPED_SEC_PER_MIN;
}

bool expedition_done(const struct GameState *gs, uint32_t now) {
    const struct PersistentGameState *p = &gs->persistent;
    if(!p->on_expedition) return false;
    return now >= end_time(p);
}

uint32_t expedition_remaining_sec(const struct GameState *gs, uint32_t now) {
    const struct PersistentGameState *p = &gs->persistent;
    if(!p->on_expedition) return 0;
    uint32_t e = end_time(p);
    return (now >= e) ? 0 : (e - now);
}

static void tier_params(uint16_t minutes, int *finds, uint8_t *activity, uint32_t *risk) {
    if(minutes >= EXPED_EPIC_MIN) { *finds = EXPED_FINDS_EPIC; *activity = EXPED_ACT_EPIC; *risk = EXPED_RISK_EPIC; }
    else if(minutes >= EXPED_LONG_MIN) { *finds = EXPED_FINDS_LONG; *activity = EXPED_ACT_LONG; *risk = EXPED_RISK_LONG; }
    else { *finds = EXPED_FINDS_SHORT; *activity = EXPED_ACT_SHORT; *risk = EXPED_RISK_SHORT; }
}

GameEventFlags expedition_resolve(struct GameState *gs, char *log, size_t logn) {
    struct PersistentGameState *p = &gs->persistent;
    int finds; uint8_t activity; uint32_t risk;
    tier_params(p->expedition_minutes, &finds, &activity, &risk);

    uint32_t treasure = 0; int eggs = 0, food = 0;
    for(int i = 0; i < finds; i++) {
        struct Catch c = catch_roll(activity, (uint8_t)random_uniform(0, HUNT_BANDS));
        if(c.category == CATCH_TREASURE) {
            p->hoard += c.value; treasure += c.value;
            if(c.tier == TIER_LARGE) p->treasure_large++;
            else if(c.tier == TIER_MED) p->treasure_med++;
            else p->treasure_small++;
        } else if(c.category == CATCH_EGG) {
            if(c.tier) p->eggs_rare++; else p->eggs_common++;
            p->eggs_caught++; eggs++;
        } else {
            food += c.value; p->prey_caught++;
        }
    }
    p->hunger = (p->hunger + food > MAX_HU) ? MAX_HU : p->hunger + food;

    int hp_loss = 0;
    if(toss_a_coin(risk)) {
        hp_loss = (int)random_uniform(EXPED_HURT_MIN, EXPED_HURT_MAX + 1);
        p->health = (p->health > (uint32_t)hp_loss) ? p->health - hp_loss : 1; // never kills
    }

    p->on_expedition = 0;
    snprintf(log, logn, "Home!\n+%lu hoard  %d egg%s\nFed +%d%s",
             (unsigned long)treasure, eggs, eggs == 1 ? "" : "s", food,
             hp_loss ? "\nCame back hurt!" : "");
    return EVT_EXPED_RETURN;
}
