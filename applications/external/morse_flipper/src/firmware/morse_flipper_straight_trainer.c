/*
 * Purpose: Generate and grade straight-key timing exercises.
 * Owns: target selection, mark timing comparison, metrics, and hints.
 * Depends on: morse_flipper_straight_trainer.h and cw.h.
 * Tests: tests/test_straight_trainer.c.
 */

#include "morse_flipper_straight_trainer.h"
#include "cw.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#ifndef COUNT_OF
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define STRAIGHT_TRAINER_DEFAULT_SEED 7U
#define STRAIGHT_WEIGHT_RARE          1U
#define STRAIGHT_WEIGHT_NORMAL        20U

static uint32_t straight_rand(MorseFlipperStraightTrainer* trainer) {
    trainer->rng_state = trainer->rng_state * 1103515245u + 12345u;
    return trainer->rng_state;
}

static uint16_t straight_code(char ch) {
    uint8_t cw_char = cw(ch);

    if(cw_char == CW_INVALID) return cw('E');
    return cw_char;
}

static uint8_t straight_weight(char ch) {
    return (ch == 'E' || ch == 'T' || ch == 'I') ? STRAIGHT_WEIGHT_RARE : STRAIGHT_WEIGHT_NORMAL;
}

static char straight_pick(MorseFlipperStraightTrainer* trainer, const char* charset) {
    uint32_t total = 0U;
    uint32_t roll;
    size_t i;

    if(charset == NULL || charset[0] == '\0') charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    for(i = 0; charset[i]; i++)
        total += straight_weight(charset[i]);
    if(total == 0U) return 'E';

    roll = straight_rand(trainer) % total;
    for(i = 0; charset[i]; i++) {
        uint8_t w = straight_weight(charset[i]);
        if(roll < w) return charset[i];
        roll -= w;
    }

    return charset[0];
}

static uint8_t straight_score(uint16_t got, uint16_t want) {
    uint32_t diff;
    uint32_t pct;

    if(want == 0U) return 0U;
    diff = got >= want ? (uint32_t)(got - want) : (uint32_t)(want - got);
    if(diff >= want) return 0U;
    pct = 100U - ((diff * 100U) / want);
    if(pct > 100U) pct = 100U;
    return (uint8_t)pct;
}

static const char* straight_score_txt(uint8_t sc, char* out, size_t out_sz) {
    if(out == NULL || out_sz < 3U) return "";
    if(sc >= 90U) {
        snprintf(out, out_sz, "OK");
        return out;
    }

    snprintf(out, out_sz, "%02u", (unsigned)sc);
    return out;
}

static void straight_update_error_view(MorseFlipperStraightTrainer* trainer) {
    size_t i;
    size_t at;
    size_t tview_at;
    uint32_t total_pct = 0U;
    uint32_t counted = 0U;
    uint8_t bars;
    uint32_t dit_sum = 0U;
    uint32_t dah_sum = 0U;
    uint32_t dit_cnt = 0U;
    uint32_t dah_cnt = 0U;
    char s_txt[4];
    char di_txt[4];
    char da_txt[4];

    if(!trainer) return;

    at = 0U;
    tview_at = 0U;
    trainer->error_bars[0] = 0;
    trainer->timing_view[0] = 0;
    trainer->metrics_line[0] = 0;
    trainer->worst_space_score = 100U;
    trainer->worst_dit_score = 100U;
    trainer->worst_dah_score = 100U;
    trainer->ratio_x100 = 0U;
    trainer->answer_total_units = 0U;

    for(i = 0; i < trainer->answer_len && i < COUNT_OF(trainer->target_marks_ms); i++) {
        int32_t diff_ms;
        uint16_t want_ms;
        uint8_t sc;

        if(!trainer->answer_marks_ms[i]) continue;
        want_ms = trainer->target_marks_ms[i] ? trainer->target_marks_ms[i] : 1U;
        diff_ms = (int32_t)trainer->answer_marks_ms[i] - (int32_t)want_ms;
        bars = (uint8_t)((diff_ms < 0 ? -diff_ms : diff_ms) / 10);
        if(!bars) bars = 1U;
        if(at + 5u >= sizeof(trainer->error_bars)) break;

        trainer->error_bars[at++] = trainer->answer[i];
        trainer->error_bars[at++] = ':';
        if(diff_ms < -5) {
            while(bars-- && at + 1u < sizeof(trainer->error_bars))
                trainer->error_bars[at++] = '<';
        } else if(diff_ms > 5) {
            while(bars-- && at + 1u < sizeof(trainer->error_bars))
                trainer->error_bars[at++] = '>';
        } else {
            trainer->error_bars[at++] = '=';
        }
        trainer->error_bars[at++] = ' ';
        trainer->error_bars[at] = 0;

        tview_at += (size_t)snprintf(
            trainer->timing_view + tview_at,
            sizeof(trainer->timing_view) - tview_at,
            "%c%u/%u ",
            trainer->answer[i],
            (unsigned)trainer->answer_marks_ms[i],
            (unsigned)want_ms);
        total_pct += (uint32_t)((diff_ms < 0 ? -diff_ms : diff_ms) * 100U / want_ms);
        counted++;

        trainer->answer_total_units += trainer->answer_mark_units[i];
        if(i != 0U) trainer->answer_total_units++;

        sc = straight_score(trainer->answer_marks_ms[i], want_ms);
        if(trainer->answer_mark_units[i] == 1U) {
            if(sc < trainer->worst_dit_score) trainer->worst_dit_score = sc;
            dit_sum += trainer->answer_marks_ms[i];
            dit_cnt++;
        } else {
            if(sc < trainer->worst_dah_score) trainer->worst_dah_score = sc;
            dah_sum += trainer->answer_marks_ms[i];
            dah_cnt++;
        }

        if(i > 0U && trainer->answer_spaces_ms[i - 1U] != 0U) {
            uint16_t want_gap =
                trainer->target_mark_units[0] ?
                    (uint16_t)(trainer->target_marks_ms[0] / trainer->target_mark_units[0]) :
                    0U;
            uint8_t sp = straight_score(trainer->answer_spaces_ms[i - 1U], want_gap);
            if(sp < trainer->worst_space_score) trainer->worst_space_score = sp;
        }

        if(tview_at >= sizeof(trainer->timing_view)) break;
    }

    if(dit_cnt == 0U) trainer->worst_dit_score = 100U;
    if(dah_cnt == 0U) trainer->worst_dah_score = 100U;
    if(trainer->answer[1] == '\0') trainer->worst_space_score = 100U;

    if(dit_cnt != 0U && dah_cnt != 0U) {
        uint32_t avg_dit = dit_sum / dit_cnt;
        uint32_t avg_dah = dah_sum / dah_cnt;
        if(avg_dit != 0U) trainer->ratio_x100 = (uint16_t)((avg_dah * 100U) / avg_dit);
    }

    if(at > 0 && at < sizeof(trainer->error_bars)) trainer->error_bars[at - 1U] = 0;
    if(tview_at > 0 && tview_at < sizeof(trainer->timing_view))
        trainer->timing_view[tview_at - 1U] = 0;
    trainer->average_drift_percent = counted ? (uint8_t)(total_pct / counted) : 0U;

    snprintf(
        trainer->metrics_line,
        sizeof(trainer->metrics_line),
        "S %2s di %2s da %2s r%u.%02u",
        straight_score_txt(trainer->worst_space_score, s_txt, sizeof(s_txt)),
        straight_score_txt(trainer->worst_dit_score, di_txt, sizeof(di_txt)),
        straight_score_txt(trainer->worst_dah_score, da_txt, sizeof(da_txt)),
        (unsigned)(trainer->ratio_x100 / 100U),
        (unsigned)(trainer->ratio_x100 % 100U));
}

void morse_flipper_straight_trainer_init(MorseFlipperStraightTrainer* trainer) {
    if(!trainer) return;
    memset(trainer, 0, sizeof(*trainer));
    trainer->target_char = 'E';
    trainer->target_morse[0] = '.';
    trainer->target_code = cw('E');
    trainer->target_symbol_count = cw_symbol_count(trainer->target_code);
    trainer->rng_state = STRAIGHT_TRAINER_DEFAULT_SEED;
    trainer->worst_space_score = 100U;
    trainer->worst_dit_score = 100U;
    trainer->worst_dah_score = 100U;
}

void morse_flipper_straight_trainer_set_seed(MorseFlipperStraightTrainer* trainer, uint32_t seed) {
    if(!trainer) return;
    trainer->rng_state = seed ? seed : STRAIGHT_TRAINER_DEFAULT_SEED;
}

void morse_flipper_straight_trainer_start(
    MorseFlipperStraightTrainer* trainer,
    const char* charset,
    uint16_t dit_ms) {
    size_t i;
    uint16_t code;
    uint8_t mark_count;

    if(!trainer) return;
    if(charset == NULL || charset[0] == '\0') charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if(!dit_ms) dit_ms = 60U;

    trainer->target_char = straight_pick(trainer, charset);
    code = straight_code(trainer->target_char);
    mark_count = cw_symbol_count(code);
    memset(trainer->target_morse, 0, sizeof(trainer->target_morse));
    memset(trainer->answer, 0, sizeof(trainer->answer));
    memset(trainer->error_bars, 0, sizeof(trainer->error_bars));
    memset(trainer->timing_view, 0, sizeof(trainer->timing_view));
    memset(trainer->metrics_line, 0, sizeof(trainer->metrics_line));
    memset(trainer->target_mark_units, 0, sizeof(trainer->target_mark_units));
    memset(trainer->target_marks_ms, 0, sizeof(trainer->target_marks_ms));
    memset(trainer->answer_marks_ms, 0, sizeof(trainer->answer_marks_ms));
    memset(trainer->answer_spaces_ms, 0, sizeof(trainer->answer_spaces_ms));
    memset(trainer->answer_mark_units, 0, sizeof(trainer->answer_mark_units));
    trainer->average_mark_error_ms = 0U;
    trainer->average_drift_percent = 0U;
    trainer->worst_space_score = 100U;
    trainer->worst_dit_score = 100U;
    trainer->worst_dah_score = 100U;
    trainer->ratio_x100 = 0U;
    trainer->target_code = code;
    trainer->target_symbol_count = mark_count;
    trainer->target_total_units = cw_total_units(code);
    trainer->answer_total_units = 0U;
    trainer->answer_len = 0U;
    trainer->ref_units_max = 0U;

    cw_to_text(code, trainer->target_morse, sizeof(trainer->target_morse));
    for(i = 0; i < mark_count && i < COUNT_OF(trainer->target_marks_ms); i++) {
        uint8_t u = cw_symbol_units(code, (uint8_t)i);
        trainer->target_mark_units[i] = u;
        trainer->target_marks_ms[i] = (uint16_t)(u * dit_ms);
    }

    for(i = 0; charset[i]; i++) {
        uint8_t u = cw_total_units(straight_code(charset[i]));
        if(u > trainer->ref_units_max) trainer->ref_units_max = u;
    }

    trainer->active = true;
}

void morse_flipper_straight_trainer_feed(
    MorseFlipperStraightTrainer* trainer,
    char elem,
    uint16_t mark_ms,
    uint16_t space_before_ms) {
    size_t len;
    size_t i;
    uint32_t total_error = 0U;
    uint32_t counted = 0U;

    if(!trainer || !trainer->active) return;
    if(elem != '.' && elem != '-') return;

    for(len = 0; trainer->answer[len]; len++) {
    }
    if(len + 1u >= sizeof(trainer->answer)) return;

    trainer->answer[len] = elem;
    trainer->answer[len + 1U] = 0;
    trainer->answer_mark_units[len] = elem == '-' ? 3U : 1U;
    trainer->answer_len = (uint8_t)(len + 1U);
    trainer->answer_marks_ms[len] = mark_ms;
    if(len != 0U) trainer->answer_spaces_ms[len - 1U] = space_before_ms;

    for(i = 0; i < trainer->target_symbol_count && i < COUNT_OF(trainer->target_marks_ms); i++) {
        if(!trainer->answer_marks_ms[i]) continue;
        total_error += trainer->answer_marks_ms[i] >= trainer->target_marks_ms[i] ?
                           (uint32_t)(trainer->answer_marks_ms[i] - trainer->target_marks_ms[i]) :
                           (uint32_t)(trainer->target_marks_ms[i] - trainer->answer_marks_ms[i]);
        counted++;
    }

    trainer->average_mark_error_ms = counted ? (uint16_t)(total_error / counted) : 0U;
    straight_update_error_view(trainer);
}

char morse_flipper_straight_trainer_target_char(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->target_char : '?';
}

const char*
    morse_flipper_straight_trainer_target_morse(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->target_morse : "";
}

const char* morse_flipper_straight_trainer_answer(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->answer : "";
}

uint16_t morse_flipper_straight_trainer_average_mark_error_ms(
    const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->average_mark_error_ms : 0U;
}

uint8_t morse_flipper_straight_trainer_average_drift_percent(
    const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->average_drift_percent : 0U;
}

const char* morse_flipper_straight_trainer_error_bars(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->error_bars : "";
}

const char*
    morse_flipper_straight_trainer_timing_view(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->timing_view : "";
}

const char*
    morse_flipper_straight_trainer_metrics_line(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->metrics_line : "";
}

bool morse_flipper_straight_trainer_active(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->active : false;
}

uint8_t
    morse_flipper_straight_trainer_worst_space_score(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->worst_space_score : 0U;
}

uint8_t
    morse_flipper_straight_trainer_worst_dit_score(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->worst_dit_score : 0U;
}

uint8_t
    morse_flipper_straight_trainer_worst_dah_score(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->worst_dah_score : 0U;
}

uint16_t morse_flipper_straight_trainer_ratio_x100(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->ratio_x100 : 0U;
}

uint8_t
    morse_flipper_straight_trainer_target_total_units(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->target_total_units : 0U;
}

uint8_t
    morse_flipper_straight_trainer_answer_total_units(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->answer_total_units : 0U;
}

uint8_t morse_flipper_straight_trainer_ref_units_max(const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->ref_units_max : 0U;
}

uint8_t morse_flipper_straight_trainer_target_symbol_count(
    const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->target_symbol_count : 0U;
}

uint8_t morse_flipper_straight_trainer_target_mark_units(
    const MorseFlipperStraightTrainer* trainer,
    uint8_t mark_idx) {
    if(trainer == NULL || mark_idx >= COUNT_OF(trainer->target_mark_units)) return 0U;
    return trainer->target_mark_units[mark_idx];
}

uint8_t morse_flipper_straight_trainer_answer_symbol_count(
    const MorseFlipperStraightTrainer* trainer) {
    return trainer ? trainer->answer_len : 0U;
}

bool morse_flipper_straight_trainer_symbol_count_match(const MorseFlipperStraightTrainer* trainer) {
    if(trainer == NULL) return false;

    return morse_flipper_straight_trainer_target_symbol_count(trainer) ==
           morse_flipper_straight_trainer_answer_symbol_count(trainer);
}
