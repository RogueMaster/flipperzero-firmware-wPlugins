#include "states.h"
#include "tuning.h"
#include "care.h"
#include "random_generator.h"

GameEventFlags advance_hygiene(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == EGG || p->stage == DEAD) return EVT_NONE;
    if(now <= p->last_poop_update) return EVT_NONE;
    uint32_t t0 = p->last_poop_update;
    uint32_t events = (now - t0) / POOP_FREQ;
    p->last_poop_update = t0 + events * POOP_FREQ;
    GameEventFlags flags = EVT_NONE;
    for(uint32_t k = 1; k <= events && p->poop < MAX_POOP; k++) {
        if(is_night(t0 + k * POOP_FREQ)) continue; // asleep: no pooping
        if(toss_a_coin(POOP_PROB)) {
            if(p->poop == 0) { p->poop_since = now; p->poop_penalized = 0; }
            p->poop++;
            flags |= EVT_POOPED;
        }
    }
    if(p->poop > 0 && !p->poop_penalized && (now - p->poop_since) >= POOP_TOLERANCE) {
        care_penalty(p, CARE_POOP_IGNORED);
        p->poop_penalized = 1;
    }
    return flags;
}

bool clean_poop(struct GameState *gs) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->poop == 0) return false;
    p->poop = 0; p->poop_since = 0; p->poop_penalized = 0;
    care_reward(p, CARE_CLEAN);
    return true;
}

GameEventFlags advance_sickness(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == EGG || p->stage == DEAD) return EVT_NONE;
    if(now <= p->last_sick_update) return EVT_NONE;
    uint32_t t0 = p->last_sick_update;
    uint32_t events = (now - t0) / SICK_CHECK_FREQ;
    p->last_sick_update = t0 + events * SICK_CHECK_FREQ;
    GameEventFlags flags = EVT_NONE;
    if(!p->sick) {
        uint32_t prob = SICK_BASE_PROB
                        + (p->hunger == 0 ? SICK_HUNGRY_BONUS : 0)
                        + (p->poop > 0 ? SICK_DIRTY_BONUS : 0);
        for(uint32_t k = 1; k <= events && !p->sick; k++) {
            if(is_night(t0 + k * SICK_CHECK_FREQ)) continue; // asleep: no illness onset
            if(toss_a_coin(prob)) {
                p->sick = 1; p->sick_since = now; p->sick_penalized = 0;
                flags |= EVT_SICK;
            }
        }
    } else if(!p->sick_penalized && (now - p->sick_since) >= SICK_TOLERANCE) {
        care_penalty(p, CARE_SICK_IGNORED);
        p->sick_penalized = 1;
    }
    return flags;
}

bool cure_sickness(struct GameState *gs) {
    struct PersistentGameState *p = &gs->persistent;
    if(!p->sick) return false;
    p->sick = 0; p->sick_since = 0; p->sick_penalized = 0;
    care_reward(p, CARE_HEAL);
    return true;
}

bool is_night(uint32_t now) {
    uint32_t hour = (now / 3600u) % 24u;
    return (hour >= NIGHT_START) || (hour < NIGHT_END);
}

bool is_asleep(const struct GameState *gs, uint32_t now) {
    // The pet sleeps through the night on its own. Lights only change whether
    // that sleep is peaceful (a care reward/penalty), never whether it sleeps.
    return is_night(now) &&
           gs->persistent.stage != DEAD && gs->persistent.stage != EGG;
}

void set_lights(struct GameState *gs, bool off, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    bool was_off = p->lights_off;
    p->lights_off = off ? 1 : 0;
    // Reward turning the lights out at night (letting it sleep); resync the
    // sleep cursor so we don't retro-penalise the period before lights-out.
    if(off && !was_off) {
        p->last_sleep_update = now;
        if(is_night(now)) care_reward(p, CARE_LIGHTS_OUT);
    }
}

GameEventFlags advance_sleep(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == EGG || p->stage == DEAD) { p->last_sleep_update = now; return EVT_NONE; }
    if(now <= p->last_sleep_update) return EVT_NONE;
    uint32_t events = (now - p->last_sleep_update) / HP_CHECK_FREQ;
    p->last_sleep_update += events * HP_CHECK_FREQ;
    if(events > 0 && is_night(now) && !p->lights_off) {
        // Lights left on while it sleeps: a mild care mistake, not harmful.
        care_penalty(p, CARE_SLEEP_DISTURBED);
    }
    return EVT_NONE;
}
