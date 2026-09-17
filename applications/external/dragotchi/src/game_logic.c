#include "game_logic.h"
#include "tuning.h"
#include "needs.h"
#include "states.h"
#include "discipline.h"
#include "evolution.h"
#include "expedition.h"
#include "care.h"
#include "random_generator.h"

static void refresh_display(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == DEAD) gs->display_state = DISP_DEAD;
    else if(is_asleep(gs, now)) gs->display_state = DISP_SLEEPING;
    else if(p->sick) gs->display_state = DISP_SICK;
    else gs->display_state = DISP_IDLE;
}

GameEventFlags advance_state(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    if(p->stage == DEAD) { gs->display_state = DISP_DEAD; return EVT_NONE; }
    if(p->on_expedition) {
        if(expedition_done(gs, now)) {
            GameEventFlags rf = expedition_resolve(gs, gs->journey_log, sizeof(gs->journey_log));
            // resync need cursors so the away period is not retro-decayed
            p->last_hunger_update = now; p->last_happiness_update = now; p->last_health_update = now;
            p->last_poop_update = now; p->last_sick_update = now; p->last_sleep_update = now;
            p->last_attention_update = now; p->last_oldage_update = now;
            gs->journey_ready = 1;
            gs->next_animation_index = 0;
            return rf;
        }
        gs->display_state = DISP_SLEEPING; // away & calm; needs paused
        return EVT_NONE;
    }
    GameEventFlags f = EVT_NONE;
    if(p->stage != EGG) {
        f |= advance_hunger(gs, now);
        f |= advance_happiness(gs, now);
        f |= advance_hygiene(gs, now);
        f |= advance_sickness(gs, now);
        f |= advance_health(gs, now);   // uses hunger/sick/poop set above
        f |= advance_sleep(gs, now);
        f |= advance_attention(gs, now);
    }
    f |= check_evolution(gs, now);
    f |= check_old_age(gs, now);
    refresh_display(gs, now);
    if(f & EVT_EVOLVED) gs->display_state = DISP_EVOLVING; // brief poof
    return f;
}

GameEventFlags do_feed(struct GameState *gs, uint32_t now) {
    (void)now;
    hunger_feed(gs);
    return EVT_FED;
}
GameEventFlags do_play(struct GameState *gs, uint32_t now) {
    (void)now;
    happiness_play(gs);
    return EVT_PLAYED;
}
GameEventFlags do_clean(struct GameState *gs, uint32_t now) {
    (void)now;
    return clean_poop(gs) ? EVT_CLEANED : EVT_NONE;
}
GameEventFlags do_medicine(struct GameState *gs, uint32_t now) {
    (void)now;
    if(cure_sickness(gs)) {
        health_restore(gs, random_uniform(MEDICINE_MIN, MEDICINE_MAX + 1));
        return EVT_HEALED;
    }
    // Not sick: medicine still tops up a little health (no penalty, no-op-ish)
    return EVT_NONE;
}
GameEventFlags do_scold(struct GameState *gs, uint32_t now) {
    (void)now;
    scold(gs);
    return EVT_NONE;
}
GameEventFlags do_lights(struct GameState *gs, uint32_t now) {
    bool newval = gs->persistent.lights_off ? false : true; // toggle
    set_lights(gs, newval, now);
    return EVT_NONE;
}
