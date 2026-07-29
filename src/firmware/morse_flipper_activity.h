#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MorseFlipperActivityCorrectAnswer = 0,
    MorseFlipperActivityListeningSession,
} MorseFlipperActivityKind;

typedef struct {
    uint16_t day;
    uint8_t deed_awards;
    uint8_t correct_answers;
} MorseFlipperActivityDaily;

void morse_flipper_activity_daily_reset(MorseFlipperActivityDaily* daily);
bool morse_flipper_activity_daily_note(
    MorseFlipperActivityDaily* daily,
    uint16_t day,
    MorseFlipperActivityKind kind);

void morse_flipper_activity_note_rx(bool correct_answer);
void morse_flipper_activity_note_listening_session(uint16_t practice_day);
