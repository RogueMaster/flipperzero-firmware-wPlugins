#include "evolution.h"
#include "tuning.h"
#include "care.h"
#include "random_generator.h"

/* Cumulative age (seconds from birth) required to ENTER each stage. */
static uint32_t enter_age(enum LifeStage stage) {
    switch(stage) {
        case HATCHLING: return AGE_HATCHLING;
        case WYRMLING:  return AGE_WYRMLING;
        case DRAKE:     return AGE_DRAKE;
        case ADULT:     return AGE_ADULT;
        default:        return 0xFFFFFFFFu;
    }
}

GameEventFlags check_evolution(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == DEAD || p->stage == ADULT) return EVT_NONE;
    if(now < p->birth_timestamp) return EVT_NONE;
    uint32_t age = now - p->birth_timestamp;
    GameEventFlags flags = EVT_NONE;
    while(p->stage < ADULT && age >= enter_age((enum LifeStage)(p->stage + 1))) {
        p->stage++;
        p->stage_entered_timestamp = p->birth_timestamp + enter_age((enum LifeStage)p->stage);
        if(p->stage == HATCHLING) {
            // Needs only start ticking once it hatches; resync all cursors so
            // the egg period does not retro-decay the newborn.
            uint32_t t = p->stage_entered_timestamp;
            p->last_hunger_update = t;
            p->last_happiness_update = t;
            p->last_health_update = t;
            p->last_poop_update = t;
            p->last_sick_update = t;
            p->last_sleep_update = t;
            p->last_attention_update = t;
        }
        if(p->stage == ADULT) {
            p->alignment = (uint8_t)care_band(p->care_score);
        }
        flags |= EVT_EVOLVED;
    }
    return flags;
}

GameEventFlags check_old_age(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage != ADULT) { p->last_oldage_update = now; return EVT_NONE; }
    uint32_t age = (now >= p->birth_timestamp) ? now - p->birth_timestamp : 0;
    // Immortal while impeccably cared for; not yet old enough -> keep cursor current.
    if(p->care_score >= CARE_IMMORTAL || age < AGE_ELDER) {
        p->last_oldage_update = now;
        return EVT_NONE;
    }
    if(now <= p->last_oldage_update) return EVT_NONE;
    uint32_t events = (now - p->last_oldage_update) / OLD_AGE_CHECK_FREQ;
    p->last_oldage_update += events * OLD_AGE_CHECK_FREQ;
    int32_t deficit = CARE_IMMORTAL - p->care_score;        // 1..CARE_IMMORTAL
    uint32_t prob = (uint32_t)(OLD_AGE_BASE_PROB * deficit) / CARE_IMMORTAL;
    if(prob < 1u) prob = 1u;
    if(prob > 100u) prob = 100u;
    GameEventFlags flags = EVT_NONE;
    while(events-- > 0 && p->stage == ADULT) {
        if(toss_a_coin(prob)) {
            p->stage = DEAD;
            p->health = 0;
            flags |= EVT_DIED;
        }
    }
    return flags;
}
