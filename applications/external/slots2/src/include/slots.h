/*
 * File: slots.h
 * Author: vh8t
 * Created: 28/12/2025
 */

#ifndef SLOTS_H
#define SLOTS_H

#include <stdbool.h>
#include <stdint.h>

#include "pcg32.h"

typedef enum {
    SYM_CHERRY,
    SYM_LEMON,
    SYM_ORANGE,
    SYM_PLUM,
    SYM_GRAPES,
    SYM_WATERMELON,
    SYM_BELL,
    SYM_BAR,
    SYM_SEVEN,
    SYM_DIAMOND,
    SYM_MAX,
} symbol_t;

typedef enum {
    STATE_IDLE,
    STATE_SPINNING,
    STATE_WINNING,
    STATE_SPRITES,
    STATE_RTP,
} game_state_t;

typedef struct {
    uint8_t reels[4][3]; // symbol_t[4][3]
    uint8_t next_reels[4][3]; // symbol_t[4][3]
    float reel_offsets[4];
    float reel_speed[4];
    float rtp;
    uint32_t credits;
    uint32_t last_win;
    uint32_t bet;
    uint8_t bet_index;
    uint8_t state; // game_state_t
    bool free_play;
    bool rigged;
    bool high_roller;
} slot_machine_t;

slot_machine_t* slot_machine_create();
void slot_machine_destroy(slot_machine_t* sm);

void start_game(pcg32_t* rng, slot_machine_t* sm);
void inc_bet(slot_machine_t* sm);
void dec_bet(slot_machine_t* sm);

void animate_reels(pcg32_t* rng, slot_machine_t* sm);

uint32_t get_symbol_value(symbol_t sym, int length, uint32_t bet);

symbol_t get_weighted_symbol(pcg32_t* rng, int reel);
void spin_reels(pcg32_t* rng, slot_machine_t* sm);
uint32_t calculate_win(slot_machine_t* sm);

#endif // SLOTS_H
