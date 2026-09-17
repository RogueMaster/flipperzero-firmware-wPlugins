#include "discipline.h"
#include "tuning.h"
#include "care.h"
#include "random_generator.h"

bool has_real_need(const struct GameState *gs) {
    const struct PersistentGameState *p = &gs->persistent;
    return (p->hunger == 0) || (p->happiness == 0) || p->sick || (p->poop > 0);
}

GameEventFlags advance_attention(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == EGG || p->stage == DEAD) { p->last_attention_update = now; return EVT_NONE; }
    if(now <= p->last_attention_update) return EVT_NONE;
    uint32_t events = (now - p->last_attention_update) / ATTENTION_CALL_FREQ;
    p->last_attention_update += events * ATTENTION_CALL_FREQ;
    if(p->attention_call || has_real_need(gs)) return EVT_NONE; // already calling / genuine need
    uint32_t prob = ATTENTION_CALL_BASE_PROB + (100u - p->discipline) / 5u; // lower discipline -> more calls
    if(prob > 100u) prob = 100u;
    GameEventFlags flags = EVT_NONE;
    while(events-- > 0 && !p->attention_call) {
        if(toss_a_coin(prob)) { p->attention_call = 1; flags |= EVT_CALL; }
    }
    return flags;
}

bool scold(struct GameState *gs) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == DEAD) return false;
    if(p->attention_call && !has_real_need(gs)) {
        // Correct scold: the pet was misbehaving
        p->attention_call = 0;
        uint32_t d = p->discipline + DISCIPLINE_SCOLD_GAIN;
        p->discipline = d > 100u ? 100u : d;
        care_reward(p, CARE_SCOLD_CORRECT);
        return true;
    }
    // Misread: scolding a pet with a real need (or no call) is bad care
    care_penalty(p, CARE_SCOLD_WRONG);
    p->discipline = (p->discipline > DISCIPLINE_SPOIL_LOSS) ? p->discipline - DISCIPLINE_SPOIL_LOSS : 0;
    return false;
}
