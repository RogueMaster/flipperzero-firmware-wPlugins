#pragma once

#include <stdint.h>

// Shared button mask used by input bridge in main.cpp.
static constexpr uint8_t BTN_UP = 0x80;
static constexpr uint8_t BTN_DOWN = 0x10;
static constexpr uint8_t BTN_LEFT = 0x20;
static constexpr uint8_t BTN_RIGHT = 0x40;
static constexpr uint8_t BTN_A = 0x08;
static constexpr uint8_t BTN_B = 0x04;

extern uint8_t* buf;
extern uint8_t state;

uint16_t time_ms();
uint8_t poll_btns();

void game_setup();
void game_loop();
bool game_back_hold_action();
