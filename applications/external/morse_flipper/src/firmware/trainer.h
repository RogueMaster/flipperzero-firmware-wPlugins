/*
 * Purpose: Define LCWO/Koch trainer state and public operations.
 * Owns: trainer phase enum, buffers, lesson settings, and scoring fields.
 * Depends on: host-safe integer types only.
 * Tests: tests/test_trainer.c.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MORSE_TRAINER_CHARSET_CAP 64U
#define MORSE_TRAINER_GROUP_CAP   16U
#define MORSE_TRAINER_ANSWER_CAP  96U

typedef enum {
    MorseTrainerPhaseIdle = 0,
    MorseTrainerPhaseListen = 1,
    MorseTrainerPhaseRepeat = 2,
    MorseTrainerPhaseDone = 3,
} MorseTrainerPhase;

typedef struct {
    uint8_t lesson;
    uint8_t group_size;
    uint8_t session_groups;
    uint16_t local_dit_ms;
    uint32_t rng_state;
    uint32_t wait_ms;
    MorseTrainerPhase phase;
    int16_t last_score;
    bool last_failed;
    bool last_missed;
    bool session_active;
    bool session_aborted;
    uint8_t session_index;
    uint8_t session_fail_count;
    uint8_t session_consecutive_missed;
    uint8_t last_group_hits;
    uint16_t session_letter_hits;
    uint16_t session_score_sum;
    uint8_t session_scored_groups;
    uint8_t custom_set_idx;
    char custom_name[24];
    char charset_override[MORSE_TRAINER_CHARSET_CAP];
    char last_group[MORSE_TRAINER_GROUP_CAP];
    char expected[MORSE_TRAINER_ANSWER_CAP];
    char answer[MORSE_TRAINER_ANSWER_CAP];
    char reveal[MORSE_TRAINER_GROUP_CAP];
    uint8_t expected_units[MORSE_TRAINER_ANSWER_CAP];
    uint8_t answer_units[MORSE_TRAINER_ANSWER_CAP];
    uint8_t expected_len;
    uint8_t answer_len;
} MorseTrainer;

void morse_trainer_init(MorseTrainer* trainer);
void morse_trainer_set_seed(MorseTrainer* trainer, uint32_t seed);

size_t morse_trainer_lesson_count(void);
void morse_trainer_lesson_label(uint8_t lesson, char* buf, size_t buf_sz);
void morse_trainer_set_lesson(MorseTrainer* trainer, uint8_t lesson);
uint8_t morse_trainer_lesson(const MorseTrainer* trainer);
void morse_trainer_set_group_size(MorseTrainer* trainer, uint8_t group_size);
uint8_t morse_trainer_group_size(const MorseTrainer* trainer);
void morse_trainer_set_session_groups(MorseTrainer* trainer, uint8_t session_groups);
uint8_t morse_trainer_session_groups(const MorseTrainer* trainer);
const char* morse_trainer_charset(const MorseTrainer* trainer);
const char* morse_trainer_last_group(const MorseTrainer* trainer);
const char* morse_trainer_expected(const MorseTrainer* trainer);
const char* morse_trainer_char_morse(char ch);
const char* morse_trainer_next_group(MorseTrainer* trainer);
void morse_trainer_start_repeat(MorseTrainer* trainer);
void morse_trainer_finish_listen(MorseTrainer* trainer);
void morse_trainer_finish_repeat(MorseTrainer* trainer);
void morse_trainer_feed_element(MorseTrainer* trainer, char elem);
void morse_trainer_tick(MorseTrainer* trainer, uint32_t ms, uint32_t timeout_ms);
int16_t morse_trainer_score_repeat(MorseTrainer* trainer);
int16_t morse_trainer_score_repeat_text(MorseTrainer* trainer, const char* text);
void morse_trainer_reset_session(MorseTrainer* trainer);
void morse_trainer_start_session(MorseTrainer* trainer);
bool morse_trainer_next_session_group(MorseTrainer* trainer);
const char* morse_trainer_answer(const MorseTrainer* trainer);
const char* morse_trainer_reveal(const MorseTrainer* trainer);
MorseTrainerPhase morse_trainer_phase(const MorseTrainer* trainer);
const char* morse_trainer_phase_name(const MorseTrainer* trainer);
int16_t morse_trainer_last_score(const MorseTrainer* trainer);
bool morse_trainer_last_failed(const MorseTrainer* trainer);
bool morse_trainer_last_missed(const MorseTrainer* trainer);
bool morse_trainer_session_active(const MorseTrainer* trainer);
bool morse_trainer_session_has_next(const MorseTrainer* trainer);
uint8_t morse_trainer_session_index(const MorseTrainer* trainer);
uint8_t morse_trainer_session_total(const MorseTrainer* trainer);
bool morse_trainer_session_aborted(const MorseTrainer* trainer);
uint8_t morse_trainer_session_fail_count(const MorseTrainer* trainer);
uint8_t morse_trainer_session_consecutive_missed(const MorseTrainer* trainer);
uint16_t morse_trainer_session_letter_hits(const MorseTrainer* trainer);
uint16_t morse_trainer_session_letter_total(const MorseTrainer* trainer);
uint8_t morse_trainer_session_letter_percent(const MorseTrainer* trainer);
uint8_t morse_trainer_session_average_score(const MorseTrainer* trainer);
bool morse_trainer_session_completed(const MorseTrainer* trainer);
