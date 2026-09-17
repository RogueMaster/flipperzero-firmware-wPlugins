#include "needs.h"
#include "tuning.h"
#include "care.h"
#include "states.h"   // is_night()
#include "random_generator.h"

/* random inclusive [min,max] */
static uint32_t rnd_incl(uint32_t min, uint32_t max) { return random_uniform(min, max + 1); }

GameEventFlags advance_hunger(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(now <= p->last_hunger_update) return EVT_NONE;
    uint32_t t0 = p->last_hunger_update;
    uint32_t events = (now - t0) / HU_DECAY_FREQ;
    p->last_hunger_update = t0 + events * HU_DECAY_FREQ;
    uint32_t old = p->hunger;
    for(uint32_t k = 1; k <= events; k++) {
        if(is_night(t0 + k * HU_DECAY_FREQ)) continue; // asleep: no hunger
        if(toss_a_coin(HU_DECAY_PROB)) {
            uint32_t d = rnd_incl(HU_DECAY_MIN, HU_DECAY_MAX);
            p->hunger = (p->hunger > d) ? p->hunger - d : 0;
        }
    }
    if(old > 0 && p->hunger == 0) {
        care_penalty(p, CARE_METER_ZERO);
        return EVT_STARVING;
    }
    return EVT_NONE;
}

GameEventFlags advance_happiness(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(now <= p->last_happiness_update) return EVT_NONE;
    uint32_t t0 = p->last_happiness_update;
    uint32_t events = (now - t0) / HAP_DECAY_FREQ;
    p->last_happiness_update = t0 + events * HAP_DECAY_FREQ;
    uint32_t old = p->happiness;
    for(uint32_t k = 1; k <= events; k++) {
        if(is_night(t0 + k * HAP_DECAY_FREQ)) continue; // asleep: no boredom
        if(toss_a_coin(HAP_DECAY_PROB)) {
            uint32_t d = rnd_incl(HAP_DECAY_MIN, HAP_DECAY_MAX);
            p->happiness = (p->happiness > d) ? p->happiness - d : 0;
        }
    }
    if(old > 0 && p->happiness == 0) care_penalty(p, CARE_METER_ZERO);
    return EVT_NONE;
}

GameEventFlags advance_health(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(now <= p->last_health_update) return EVT_NONE;
    uint32_t t0 = p->last_health_update;
    uint32_t events = (now - t0) / HP_CHECK_FREQ;
    p->last_health_update = t0 + events * HP_CHECK_FREQ;
    // Health drains only while starving or sick. Poop is a *sickness risk*
    // (see advance_sickness), not a direct health bleed.
    bool bad = (p->hunger == 0) || p->sick;
    GameEventFlags flags = EVT_NONE;
    for(uint32_t k = 1; k <= events && p->stage != DEAD; k++) {
        if(is_night(t0 + k * HP_CHECK_FREQ)) continue; // asleep: no drain (resting)
        if(bad) {
            uint32_t d = rnd_incl(HP_DRAIN_MIN, HP_DRAIN_MAX);
            if(p->health > d) {
                p->health -= d;
            } else {
                p->health = 0;
                p->stage = DEAD;
                flags |= EVT_DIED;
            }
        }
    }
    return flags;
}

void hunger_feed(struct GameState *gs) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == DEAD) return;
    if(p->hunger >= MAX_HU) {
        care_penalty(p, CARE_OVERFEED);
        return;
    }
    care_reward(p, CARE_FEED_HUNGRY);
    uint32_t g = rnd_incl(FEED_MIN, FEED_MAX);
    p->hunger = (p->hunger + g > MAX_HU) ? MAX_HU : p->hunger + g;
    gs->display_state = DISP_EATING;
}

void happiness_play(struct GameState *gs) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == DEAD) return;
    if(p->happiness < MAX_HAPPINESS) care_reward(p, CARE_PLAY_SAD);
    uint32_t g = rnd_incl(PLAY_MIN, PLAY_MAX);
    p->happiness = (p->happiness + g > MAX_HAPPINESS) ? MAX_HAPPINESS : p->happiness + g;
    gs->display_state = DISP_PLAYING;
}

void health_restore(struct GameState *gs, uint32_t amount) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == DEAD) return;
    p->health = (p->health + amount > MAX_HP) ? MAX_HP : p->health + amount;
}
