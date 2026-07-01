/*
 * Purpose: Define the reusable Morse keyer model and public operations.
 * Owns: keyer modes, queued actions, events, and runtime state layout.
 * Depends on: stdbool/stddef/stdint only.
 * Tests: tests/test_keyer.c and tests/test_vail_modes.c.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MORSE_KEYER_QUEUE_CAPACITY 5U
#define MORSE_KEYER_EVENT_CAPACITY 16U

typedef enum {
    MorseKeyerModePassthrough = 0,
    MorseKeyerModeStraight = 1,
    MorseKeyerModeBug = 2,
    MorseKeyerModeElBug = 3,
    MorseKeyerModeSingleDot = 4,
    MorseKeyerModeUltimatic = 5,
    MorseKeyerModePlainIambic = 6,
    MorseKeyerModeIambicA = 7,
    MorseKeyerModeIambicB = 8,
    MorseKeyerModeKeyahead = 9,
} MorseKeyerMode;

typedef enum {
    MorseKeyerPaddleDit = 0,
    MorseKeyerPaddleDah = 1,
    MorseKeyerPaddleCount = 2,
} MorseKeyerPaddle;

typedef enum {
    MorseKeyerEventPress = 0,
    MorseKeyerEventRelease = 1,
} MorseKeyerEventType;

typedef struct {
    uint8_t type;
    uint8_t paddle;
} MorseKeyerEvent;

typedef struct {
    uint8_t mode;
    uint16_t dit_duration_ms;
    bool paddle_down[MorseKeyerPaddleCount];
    bool output_active[MorseKeyerPaddleCount];
    bool relay_closed[MorseKeyerPaddleCount];
    bool pulse_active;
    uint32_t next_pulse_at;
    int8_t next_repeat_paddle;
    int8_t current_pulse_paddle;
    int8_t current_output_paddle;
    uint8_t set_queue[MORSE_KEYER_QUEUE_CAPACITY];
    uint8_t set_queue_len;
    uint8_t fifo_queue[MORSE_KEYER_QUEUE_CAPACITY];
    uint8_t fifo_queue_len;
    MorseKeyerEvent events[MORSE_KEYER_EVENT_CAPACITY];
    uint8_t event_head;
    uint8_t event_count;
} MorseKeyer;

void morse_keyer_init(MorseKeyer* keyer, uint8_t mode, uint16_t dit_duration_ms);
void morse_keyer_reset(MorseKeyer* keyer);

bool morse_keyer_mode_valid(uint8_t mode);
void morse_keyer_set_mode(MorseKeyer* keyer, uint8_t mode);
uint8_t morse_keyer_get_mode(const MorseKeyer* keyer);

void morse_keyer_set_dit_duration(MorseKeyer* keyer, uint16_t dit_duration_ms);
uint16_t morse_keyer_get_dit_duration(const MorseKeyer* keyer);

void morse_keyer_paddle_event(MorseKeyer* keyer, uint8_t paddle, bool pressed, uint32_t now_ms);
void morse_keyer_tick(MorseKeyer* keyer, uint32_t now_ms);

bool morse_keyer_pop_event(MorseKeyer* keyer, MorseKeyerEvent* event);
bool morse_keyer_output_active(const MorseKeyer* keyer, uint8_t paddle);

const char* morse_keyer_mode_name(uint8_t mode);
uint8_t morse_keyer_next_ui_mode(uint8_t mode);
