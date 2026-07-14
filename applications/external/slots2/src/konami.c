#include <furi.h>

#include <stdbool.h>
#include <stdint.h>

#include "include/konami.h"
#include "include/notify.h"
#include "include/pcg32.h"
#include "include/slots.h"

bool check_konami(konami_t* konami, InputEvent* ev, bool* matched, void* ctx) {
    uint32_t now = furi_get_tick();
    if(now - konami->last_tick > KONAMI_TIMEOUT_MS) konami->curr_step = 0;

    konami->last_tick = now;
    if(ev->key == konami->sequence[konami->curr_step]) {
        konami->curr_step++;
        if(matched) *matched = true;

        if(konami->curr_step == konami->length) {
            konami->curr_step = 0;
            if(konami->callback) konami->callback(ctx);
            return true;
        }
    } else
        konami->curr_step = ev->key == konami->sequence[0] ? 1 : 0;

    return false;
}

KONAMI_CALLBACK(money) {
    slot_machine_t* sm = ctx;
    notify(NOTIFICATION_CHEAT_ENABLED);
    sm->credits += 1000;
}

KONAMI_CALLBACK(sprites) {
    slot_machine_t* sm = ctx;
    notify(NOTIFICATION_CHEAT_ENABLED);
    sm->state = STATE_SPRITES;
}

KONAMI_CALLBACK(rigged) {
    slot_machine_t* sm = ctx;
    notify(NOTIFICATION_CHEAT_ENABLED);
    sm->rigged = true;
}

KONAMI_CALLBACK(free_play) {
    slot_machine_t* sm = ctx;
    notify(NOTIFICATION_CHEAT_ENABLED);
    sm->free_play = true;
}

KONAMI_CALLBACK(nuke) {
    slot_machine_t* sm = ctx;
    notify(NOTIFICATION_CHEAT_ENABLED);
    sm->credits = 250;
    sm->last_win = 0;
    sm->bet_index = 5;
    sm->bet = 5;
}

KONAMI_CALLBACK(rtp) {
    slot_machine_t* sm = ctx;
    notify(NOTIFICATION_CHEAT_ENABLED);

    uint32_t total_bet = 0;
    uint32_t total_won = 0;
    int iterations = 500;

    pcg32_t rng = {0};
    pcg32_seed(&rng);

    for(int i = 0; i < iterations; i++) {
        total_bet += sm->bet;
        spin_reels(&rng, sm);
        total_won += calculate_win(sm);
    }

    sm->rtp = ((float)total_won / (float)total_bet) * 100.f;
    sm->state = STATE_RTP;
}

KONAMI_CALLBACK(high_roller) {
    slot_machine_t* sm = ctx;
    notify(NOTIFICATION_CHEAT_ENABLED);
    sm->high_roller = true;
    sm->bet_index = 7;
    sm->bet = 100;
}
