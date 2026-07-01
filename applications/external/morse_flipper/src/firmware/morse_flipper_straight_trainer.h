/*
 * Purpose: Define the straight trainer model and grading API.
 * Owns: target/answer buffers, metrics, settings, and trainer phase state.
 * Depends on: host-safe integer types only.
 * Tests: tests/test_straight_trainer.c.
 */

#ifndef yo3gnd_straight_trainer_2234
#define yo3gnd_straight_trainer_2234

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char target_char;
    char target_morse[8];
    char answer[16];
    char error_bars[48];
    char timing_view[96];
    char metrics_line[32];
    uint8_t target_mark_units[8];
    uint16_t target_marks_ms[8];
    uint16_t answer_marks_ms[16];
    uint16_t answer_spaces_ms[16];
    uint8_t worst_space_score;
    uint8_t worst_dit_score;
    uint8_t worst_dah_score;
    uint16_t ratio_x100;
    uint8_t target_total_units;
    uint8_t answer_total_units;
    uint8_t ref_units_max;
    uint16_t target_code;
    uint8_t target_symbol_count;
    uint8_t answer_len;
    uint8_t answer_mark_units[16];
    uint16_t average_mark_error_ms;
    uint8_t average_drift_percent;
    bool active;
    uint32_t rng_state;
} MorseFlipperStraightTrainer;

void morse_flipper_straight_trainer_init(MorseFlipperStraightTrainer* trainer);
void morse_flipper_straight_trainer_set_seed(MorseFlipperStraightTrainer* trainer, uint32_t seed);
void morse_flipper_straight_trainer_start(
    MorseFlipperStraightTrainer* trainer,
    const char* charset,
    uint16_t dit_ms);
void morse_flipper_straight_trainer_feed(
    MorseFlipperStraightTrainer* trainer,
    char elem,
    uint16_t mark_ms,
    uint16_t space_before_ms);
char morse_flipper_straight_trainer_target_char(const MorseFlipperStraightTrainer* trainer);
const char*
    morse_flipper_straight_trainer_target_morse(const MorseFlipperStraightTrainer* trainer);
const char* morse_flipper_straight_trainer_answer(const MorseFlipperStraightTrainer* trainer);
uint16_t morse_flipper_straight_trainer_average_mark_error_ms(
    const MorseFlipperStraightTrainer* trainer);
uint8_t morse_flipper_straight_trainer_average_drift_percent(
    const MorseFlipperStraightTrainer* trainer);
const char* morse_flipper_straight_trainer_error_bars(const MorseFlipperStraightTrainer* trainer);
const char* morse_flipper_straight_trainer_timing_view(const MorseFlipperStraightTrainer* trainer);
const char*
    morse_flipper_straight_trainer_metrics_line(const MorseFlipperStraightTrainer* trainer);
bool morse_flipper_straight_trainer_active(const MorseFlipperStraightTrainer* trainer);
uint8_t
    morse_flipper_straight_trainer_worst_space_score(const MorseFlipperStraightTrainer* trainer);
uint8_t morse_flipper_straight_trainer_worst_dit_score(const MorseFlipperStraightTrainer* trainer);
uint8_t morse_flipper_straight_trainer_worst_dah_score(const MorseFlipperStraightTrainer* trainer);
uint16_t morse_flipper_straight_trainer_ratio_x100(const MorseFlipperStraightTrainer* trainer);
uint8_t
    morse_flipper_straight_trainer_target_total_units(const MorseFlipperStraightTrainer* trainer);
uint8_t
    morse_flipper_straight_trainer_answer_total_units(const MorseFlipperStraightTrainer* trainer);
uint8_t morse_flipper_straight_trainer_ref_units_max(const MorseFlipperStraightTrainer* trainer);
uint8_t
    morse_flipper_straight_trainer_target_symbol_count(const MorseFlipperStraightTrainer* trainer);
uint8_t morse_flipper_straight_trainer_target_mark_units(
    const MorseFlipperStraightTrainer* trainer,
    uint8_t mark_idx);
uint8_t
    morse_flipper_straight_trainer_answer_symbol_count(const MorseFlipperStraightTrainer* trainer);
bool morse_flipper_straight_trainer_symbol_count_match(const MorseFlipperStraightTrainer* trainer);

#endif
