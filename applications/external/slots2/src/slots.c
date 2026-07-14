#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "include/notify.h"
#include "include/pcg32.h"
#include "include/slots.h"

// clang-format off
static const uint8_t REEL_WEIGHTS[4][SYM_MAX] = {
    // CHR, LEM, ORG, PLM, GRP, WAT, BEL, BAR, SEV, DIA
    {  16,  15,  14,  13,  11,  9,   8,   6,   5,   3},
    {  16,  15,  14,  13,  11,  9,   8,   6,   5,   3},
    {  16,  15,  14,  13,  11,  9,   8,   6,   5,   3},
    {  16,  15,  14,  13,  11,  9,   8,   6,   5,   3},
};
// clang-format on

static const uint32_t BET_STEPS[8] = {1, 2, 5, 10, 15, 25, 50, 100};

slot_machine_t* slot_machine_create() {
    slot_machine_t* sm = malloc(sizeof(slot_machine_t));
    if(!sm) return NULL;

    memset(sm, 0, sizeof(slot_machine_t));

    sm->credits = 250;
    sm->bet_index = 5;
    sm->bet = 5;

    return sm;
}

void slot_machine_destroy(slot_machine_t* sm) {
    if(sm) free(sm);
}

void start_game(pcg32_t* rng, slot_machine_t* sm) {
    if(!sm->free_play && !sm->high_roller) {
        if(sm->bet > sm->credits) {
            notify(NOTIFICATION_NO_BALANCE);
            return;
        }
        sm->credits -= sm->bet;
    }

    sm->state = STATE_SPINNING;
    sm->high_roller = false;
    sm->last_win = 0;

    spin_reels(rng, sm);

    for(int i = 0; i < 4; i++)
        sm->reel_speed[i] = 6.f + (i * 2.f);
}

void inc_bet(slot_machine_t* sm) {
    if(sm->bet_index < 7) sm->bet = BET_STEPS[++sm->bet_index];
}

void dec_bet(slot_machine_t* sm) {
    if(sm->bet_index > 0) sm->bet = BET_STEPS[--sm->bet_index];
}

void animate_reels(pcg32_t* rng, slot_machine_t* sm) {
    bool stopped = true;

    for(int i = 0; i < 4; i++) {
        if(sm->reel_speed[i] <= 0) continue;

        stopped = false;
        sm->reel_offsets[i] += sm->reel_speed[i];

        if(sm->reel_offsets[i] >= 20.f) {
            sm->reel_offsets[i] = 0;

            sm->reels[i][2] = sm->reels[i][1];
            sm->reels[i][1] = sm->reels[i][0];

            if(sm->rigged)
                sm->reels[i][0] = SYM_DIAMOND;
            else
                sm->reels[i][0] = get_weighted_symbol(rng, i);

            sm->reel_speed[i] -= 0.6f;

            if(sm->reel_speed[i] < 0.5f) {
                sm->reel_speed[i] = 0;
                sm->reel_offsets[i] = 0;
            }
        }
    }

    if(stopped) {
        uint32_t win = calculate_win(sm);

        sm->state = STATE_IDLE;
        sm->rigged = false;
        sm->last_win = win;
        sm->credits += win;

        if(win > 0) notify(NOTIFICATION_WIN);
    }
}

uint32_t get_symbol_value(symbol_t sym, int length, uint32_t bet) {
#define MULT(base) ((length == 3) ? base : base * 5)

    uint32_t multiplier_num = 0;
    const uint32_t multiplier_den = 10; // denominator

    if(sym == SYM_CHERRY)
        multiplier_num = MULT(2);
    else if(sym == SYM_LEMON)
        multiplier_num = MULT(3);
    else if(sym == SYM_ORANGE)
        multiplier_num = MULT(5);
    else if(sym == SYM_PLUM)
        multiplier_num = MULT(7);
    else if(sym == SYM_GRAPES)
        multiplier_num = MULT(10);
    else if(sym == SYM_WATERMELON)
        multiplier_num = MULT(15);
    else if(sym == SYM_BELL)
        multiplier_num = MULT(25);
    else if(sym == SYM_BAR)
        multiplier_num = MULT(50);
    else if(sym == SYM_SEVEN)
        multiplier_num = MULT(100);
    else if(sym == SYM_DIAMOND)
        multiplier_num = MULT(250);

    uint32_t win = (bet * multiplier_num) / multiplier_den;
    if((bet * multiplier_num) % multiplier_den >= multiplier_den / 2) win++;
    if(multiplier_num > 0 && win == 0) win = 1;

    return win;
#undef MULT
}

symbol_t get_weighted_symbol(pcg32_t* rng, int reel) {
    if(reel < 0 || reel >= 4) reel = 0;

    uint32_t total_weight = 0;
    for(int sym = 0; sym < SYM_MAX; sym++)
        total_weight += REEL_WEIGHTS[reel][sym];

    uint32_t random_val = pcg32_random_bound(rng, total_weight);
    uint32_t current_sum = 0;

    for(int sym = 0; sym < SYM_MAX; sym++) {
        current_sum += REEL_WEIGHTS[reel][sym];
        if(random_val < current_sum) return (symbol_t)sym;
    }

    return SYM_CHERRY;
}

void spin_reels(pcg32_t* rng, slot_machine_t* sm) {
    for(int x = 0; x < 4; x++) {
        for(int y = 0; y < 3; y++) {
            sm->reels[x][y] = get_weighted_symbol(rng, x);
        }
    }
}

uint32_t calculate_win(slot_machine_t* sm) {
    uint32_t total_win = 0;

    for(int sym = 0; sym < SYM_MAX; sym++) {
        uint32_t sym_count[4] = {0};

        for(int x = 0; x < 4; x++) {
            for(int y = 0; y < 3; y++) {
                if(sm->reels[x][y] == (symbol_t)sym) sym_count[x]++;
            }
        }

        int consecutive_reels = 0;
        for(int x = 0; x < 4; x++) {
            if(sym_count[x] > 0)
                consecutive_reels++;
            else
                break;
        }

        if(consecutive_reels >= 3) {
            uint32_t ways = 1;
            for(int x = 0; x < consecutive_reels; x++)
                ways *= sym_count[x];
            total_win += ways * get_symbol_value((symbol_t)sym, consecutive_reels, sm->bet);
        }
    }

    return total_win;
}
