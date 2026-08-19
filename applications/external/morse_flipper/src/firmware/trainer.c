/*
 * Purpose: Run host-safe LCWO/Koch trainer sessions.
 * Owns: character set selection, prompts, answers, scoring, and session stats.
 * Depends on: trainer.h and cw.h.
 * Tests: tests/test_trainer.c.
 */

#include "trainer.h"
#include "cw.h"

#include <stdio.h>
#include <string.h>

static const char morse_trainer_koch_order[] = "KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X";
#define MORSE_TRAINER_DEFAULT_SEED 1U

static void morse_trainer_note_session_result(MorseTrainer* trainer, bool missed) {
    if(trainer == NULL || !trainer->session_active) {
        return;
    }

    if(trainer->last_failed) {
        trainer->session_fail_count++;
    }

    if(missed) {
        trainer->session_consecutive_missed++;
    } else {
        trainer->session_consecutive_missed = 0U;
    }

    trainer->session_score_sum = (uint16_t)(trainer->session_score_sum + trainer->last_score);
    trainer->session_scored_groups++;
    trainer->session_letter_hits =
        (uint16_t)(trainer->session_letter_hits + trainer->last_group_hits);
    trainer->session_letter_total =
        (uint16_t)(trainer->session_letter_total + trainer->last_group_total);
    if(!trainer->session_aborted && trainer->session_scored_groups >= trainer->session_groups)
        trainer->session_active = false;

    if(trainer->session_consecutive_missed >= 4U) {
        trainer->session_aborted = true;
        trainer->session_active = false;
    }
}

static char morse_trainer_up(char ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static void morse_trainer_copy_reveal(MorseTrainer* trainer, const char* text) {
    uint8_t wi = 0U;
    uint8_t i = 0U;

    if(trainer == NULL) return;

    trainer->reveal[0] = '\0';
    if(text == NULL) return;

    while(text[i] != '\0' && wi + 1U < sizeof(trainer->reveal)) {
        char ch = morse_trainer_up(text[i++]);

        if(ch == ' ' || ch == '|') continue;
        trainer->reveal[wi++] = ch;
    }

    trainer->reveal[wi] = '\0';
}

static uint32_t morse_trainer_rand(MorseTrainer* trainer) {
    trainer->rng_state = trainer->rng_state * 1664525U + 1013904223U;
    return trainer->rng_state;
}

const char* morse_trainer_char_morse(char ch) {
    static char buf[8];
    uint8_t cw_char = cw(ch);

    if(cw_char == CW_INVALID) {
        buf[0] = '\0';
        return buf;
    }

    cw_to_text(cw_char, buf, sizeof(buf));
    return buf;
}

void morse_trainer_init(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    memset(trainer, 0, sizeof(*trainer));
    trainer->lesson = 1U;
    trainer->group_size = 3U;
    trainer->session_groups = 10U;
    trainer->local_dit_ms = 100U;
    trainer->rng_state = MORSE_TRAINER_DEFAULT_SEED;
    trainer->last_missed = false;
}

void morse_trainer_set_seed(MorseTrainer* trainer, uint32_t seed) {
    if(trainer == NULL) {
        return;
    }

    trainer->rng_state = seed ? seed : MORSE_TRAINER_DEFAULT_SEED;
}

size_t morse_trainer_lesson_count(void) {
    return (sizeof(morse_trainer_koch_order) - 2U);
}

void morse_trainer_lesson_label(uint8_t lesson, char* buf, size_t buf_sz) {
    uint8_t max_lesson;

    if(buf == NULL || buf_sz < 2U) {
        return;
    }

    max_lesson = (uint8_t)morse_trainer_lesson_count();
    if(lesson < 1U) {
        lesson = 1U;
    } else if(lesson > max_lesson) {
        lesson = max_lesson;
    }

    if(lesson == 1U) {
        snprintf(
            buf, buf_sz, "1 - %c %c", morse_trainer_koch_order[0], morse_trainer_koch_order[1]);
        return;
    }

    snprintf(buf, buf_sz, "%u - %c", (unsigned)lesson, morse_trainer_koch_order[lesson]);
}

void morse_trainer_set_lesson(MorseTrainer* trainer, uint8_t lesson) {
    uint8_t max_lesson = (uint8_t)morse_trainer_lesson_count();

    if(trainer == NULL) {
        return;
    }

    if(lesson < 1U) {
        lesson = 1U;
    }
    if(lesson > max_lesson) {
        lesson = max_lesson;
    }

    trainer->lesson = lesson;
}

uint8_t morse_trainer_lesson(const MorseTrainer* trainer) {
    if(trainer == NULL || trainer->lesson == 0U) {
        return 1U;
    }

    return trainer->lesson;
}

void morse_trainer_set_group_size(MorseTrainer* trainer, uint8_t group_size) {
    if(trainer == NULL) {
        return;
    }

    if(group_size < 1U) {
        group_size = 1U;
    }
    if(group_size > MORSE_TRAINER_GROUP_CAP - 1U) {
        group_size = MORSE_TRAINER_GROUP_CAP - 1U;
    }

    trainer->group_size = group_size;
}

uint8_t morse_trainer_group_size(const MorseTrainer* trainer) {
    return trainer ? trainer->group_size : 5U;
}

void morse_trainer_set_session_groups(MorseTrainer* trainer, uint8_t session_groups) {
    if(trainer == NULL) {
        return;
    }

    if(session_groups < 1U) {
        session_groups = 1U;
    }
    if(session_groups > 40U) {
        session_groups = 40U;
    }

    trainer->session_groups = session_groups;
}

uint8_t morse_trainer_session_groups(const MorseTrainer* trainer) {
    return trainer ? trainer->session_groups : 10U;
}

const char* morse_trainer_lesson_charset(const MorseTrainer* trainer) {
    static char buf[MORSE_TRAINER_CHARSET_CAP];
    size_t n;

    n = trainer ? ((size_t)morse_trainer_lesson(trainer) + 1U) : 2U;
    if(n >= sizeof(buf)) n = sizeof(buf) - 1U;

    memcpy(buf, morse_trainer_koch_order, n);
    buf[n] = '\0';
    return buf;
}

const char* morse_trainer_charset(const MorseTrainer* trainer) {
    if(trainer != NULL && trainer->charset_override[0] != '\0') return trainer->charset_override;
    return morse_trainer_lesson_charset(trainer);
}

const char* morse_trainer_last_group(const MorseTrainer* trainer) {
    return trainer ? trainer->last_group : "";
}

const char* morse_trainer_expected(const MorseTrainer* trainer) {
    return trainer ? trainer->expected : "";
}

const char* morse_trainer_next_group(MorseTrainer* trainer) {
    const char* charset;
    char prev[MORSE_TRAINER_GROUP_CAP];
    size_t clen = 0U;
    size_t wi = 0U;
    size_t i;
    uint8_t tries;

    if(trainer == NULL) return "";

    charset = morse_trainer_charset(trainer);
    strncpy(prev, trainer->last_group, sizeof(prev) - 1U);
    prev[sizeof(prev) - 1U] = '\0';
    while(charset[clen] != '\0')
        clen++;

    if(clen == 0U) {
        trainer->last_group[0] = '\0';
        trainer->expected[0] = '\0';
        trainer->expected_len = 0U;
        return trainer->last_group;
    }

    tries = 0U;
    do {
        for(i = 0; i + 1U < sizeof(trainer->last_group) && i < trainer->group_size; i++) {
            trainer->last_group[i] = charset[morse_trainer_rand(trainer) % clen];
        }
        trainer->last_group[i] = '\0';
        tries++;
    } while(clen > 1U && prev[0] != '\0' && strcmp(prev, trainer->last_group) == 0 && tries < 8U);

    if(clen > 1U && prev[0] != '\0' && strcmp(prev, trainer->last_group) == 0) {
        size_t last = strlen(trainer->last_group);
        char pick = charset[morse_trainer_rand(trainer) % clen];

        if(last != 0U) {
            if(pick == trainer->last_group[last - 1U])
                pick = charset[(morse_trainer_rand(trainer) + 1U) % clen];
            trainer->last_group[last - 1U] = pick;
        }
    }

    memset(trainer->expected_units, 0, sizeof(trainer->expected_units));

    for(i = 0; trainer->last_group[i] != '\0' && wi + 1U < sizeof(trainer->expected); i++) {
        uint16_t code = cw(trainer->last_group[i]);
        uint8_t marks = cw_symbol_count(code);
        uint8_t mark_idx;

        for(mark_idx = 0U; mark_idx < marks && wi + 1U < sizeof(trainer->expected); mark_idx++) {
            uint8_t units = cw_symbol_units(code, mark_idx);

            trainer->expected_units[wi] = units;
            trainer->expected[wi++] = units == 3U ? '-' : '.';
        }
    }
    trainer->expected[wi] = '\0';
    trainer->expected_len = (uint8_t)wi;
    return trainer->last_group;
}

void morse_trainer_start_repeat(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    morse_trainer_next_group(trainer);
    trainer->answer[0] = '\0';
    trainer->reveal[0] = '\0';
    memset(trainer->answer_units, 0, sizeof(trainer->answer_units));
    trainer->answer_len = 0U;
    trainer->wait_ms = 0U;
    trainer->last_score = -1;
    trainer->last_group_hits = 0U;
    trainer->last_group_total = 0U;
    trainer->last_failed = false;
    trainer->last_missed = false;
    trainer->phase = MorseTrainerPhaseListen;
}

void morse_trainer_finish_listen(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    trainer->phase = MorseTrainerPhaseRepeat;
    trainer->wait_ms = 0U;
}

void morse_trainer_finish_repeat(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    trainer->phase = MorseTrainerPhaseDone;
}

void morse_trainer_feed_element(MorseTrainer* trainer, char elem) {
    size_t len;

    if(trainer == NULL || trainer->phase != MorseTrainerPhaseRepeat) {
        return;
    }

    if(elem != '.' && elem != '-') {
        return;
    }

    len = strlen(trainer->answer);
    if(len + 1U >= sizeof(trainer->answer)) {
        return;
    }

    trainer->answer[len] = elem;
    trainer->answer[len + 1U] = '\0';
    trainer->answer_units[len] = elem == '-' ? 3U : 1U;
    trainer->answer_len = (uint8_t)(len + 1U);
}

void morse_trainer_tick(MorseTrainer* trainer, uint32_t ms, uint32_t timeout_ms) {
    if(trainer == NULL || trainer->phase != MorseTrainerPhaseRepeat) {
        return;
    }

    if(trainer->answer[0] != '\0') {
        return;
    }

    trainer->wait_ms += ms;
    if(timeout_ms == 0U) timeout_ms = 6000U;
    if(trainer->wait_ms < timeout_ms) {
        return;
    }

    trainer->last_score = 0;
    trainer->last_group_hits = 0U;
    trainer->last_group_total = (uint8_t)strlen(trainer->last_group);
    trainer->last_failed = true;
    trainer->last_missed = true;
    trainer->phase = MorseTrainerPhaseDone;
    morse_trainer_note_session_result(trainer, true);
}

int16_t morse_trainer_score_repeat(MorseTrainer* trainer) {
    size_t matched = 0U;

    if(trainer == NULL) {
        return -1;
    }

    while(matched < trainer->expected_len && matched < trainer->answer_len &&
          trainer->expected_units[matched] == trainer->answer_units[matched]) {
        matched++;
    }

    trainer->last_score = trainer->expected_len == 0U ?
                              0 :
                              (int16_t)(((int32_t)matched * 100) / (int32_t)trainer->expected_len);
    trainer->last_failed = trainer->last_score != 100 || trainer->answer[0] == '\0';
    trainer->last_missed = trainer->answer[0] == '\0';
    trainer->last_group_hits = (!trainer->last_failed && trainer->last_group[0] != '\0') ?
                                   (uint8_t)strlen(trainer->last_group) :
                                   0U;
    trainer->last_group_total = (uint8_t)strlen(trainer->last_group);
    trainer->phase = MorseTrainerPhaseDone;
    morse_trainer_note_session_result(trainer, trainer->last_missed);
    return trainer->last_score;
}

int16_t morse_trainer_score_repeat_text(MorseTrainer* trainer, const char* text) {
    size_t want_len;
    size_t got_len;
    size_t hit = 0U;
    size_t i;

    if(trainer == NULL) return -1;

    morse_trainer_copy_reveal(trainer, text);
    want_len = strlen(trainer->last_group);
    got_len = strlen(trainer->reveal);

    for(i = 0U; trainer->last_group[i] != '\0' && trainer->reveal[i] != '\0'; i++)
        if(trainer->last_group[i] == trainer->reveal[i]) hit++;

    trainer->last_group_hits = (uint8_t)hit;
    trainer->last_group_total = (uint8_t)(got_len > want_len ? got_len : want_len);
    trainer->last_score = want_len == 0U ? 0 : (int16_t)(((int32_t)hit * 100) / (int32_t)want_len);
    trainer->last_failed = trainer->reveal[0] == '\0' || want_len != got_len ||
                           strcmp(trainer->last_group, trainer->reveal) != 0;
    trainer->last_missed = trainer->reveal[0] == '\0';
    trainer->phase = MorseTrainerPhaseDone;
    morse_trainer_note_session_result(trainer, trainer->last_missed);
    return trainer->last_score;
}

void morse_trainer_reset_session(MorseTrainer* trainer) {
    if(trainer == NULL) return;

    trainer->wait_ms = 0U;
    trainer->phase = MorseTrainerPhaseIdle;
    trainer->last_score = -1;
    trainer->last_failed = false;
    trainer->last_missed = false;
    trainer->session_active = false;
    trainer->session_aborted = false;
    trainer->session_index = 0U;
    trainer->session_fail_count = 0U;
    trainer->session_consecutive_missed = 0U;
    trainer->last_group_hits = 0U;
    trainer->last_group_total = 0U;
    trainer->session_letter_hits = 0U;
    trainer->session_letter_total = 0U;
    trainer->session_score_sum = 0U;
    trainer->session_scored_groups = 0U;

    trainer->last_group[0] = '\0';
    trainer->expected[0] = '\0';
    trainer->answer[0] = '\0';
    trainer->reveal[0] = '\0';
    memset(trainer->expected_units, 0, sizeof(trainer->expected_units));
    memset(trainer->answer_units, 0, sizeof(trainer->answer_units));
    trainer->expected_len = 0U;
    trainer->answer_len = 0U;
}

void morse_trainer_start_session(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    morse_trainer_reset_session(trainer);
    trainer->session_active = true;
    trainer->session_index = 1U;
    morse_trainer_start_repeat(trainer);
}

bool morse_trainer_next_session_group(MorseTrainer* trainer) {
    if(trainer == NULL || !trainer->session_active) {
        return false;
    }

    if(trainer->session_index >= trainer->session_groups) {
        trainer->session_active = false;
        return false;
    }

    trainer->session_index++;
    morse_trainer_start_repeat(trainer);
    return true;
}

const char* morse_trainer_answer(const MorseTrainer* trainer) {
    return trainer ? trainer->answer : "";
}

const char* morse_trainer_reveal(const MorseTrainer* trainer) {
    return trainer ? trainer->reveal : "";
}

MorseTrainerPhase morse_trainer_phase(const MorseTrainer* trainer) {
    if(trainer == NULL) return MorseTrainerPhaseIdle;
    return trainer->phase;
}

const char* morse_trainer_phase_name(const MorseTrainer* trainer) {
    MorseTrainerPhase p;

    if(trainer == NULL) {
        return "idle";
    }

    p = trainer->phase;

    switch(p) {
    case MorseTrainerPhaseListen:
        return "listen";
    case MorseTrainerPhaseRepeat:
        return "repeat";
    case MorseTrainerPhaseDone:
        return "done";
    default:
        return "idle";
    }
}

int16_t morse_trainer_last_score(const MorseTrainer* trainer) {
    return trainer ? trainer->last_score : -1;
}

bool morse_trainer_last_failed(const MorseTrainer* trainer) {
    return trainer ? trainer->last_failed : true;
}

bool morse_trainer_last_missed(const MorseTrainer* trainer) {
    return trainer ? trainer->last_missed : true;
}

bool morse_trainer_session_active(const MorseTrainer* trainer) {
    return trainer ? trainer->session_active : false;
}

bool morse_trainer_session_has_next(const MorseTrainer* trainer) {
    if(trainer == NULL) return false;
    if(!trainer->session_active) return false;
    return trainer->session_index < trainer->session_groups;
}

uint8_t morse_trainer_session_index(const MorseTrainer* trainer) {
    return trainer ? trainer->session_index : 0U;
}

uint8_t morse_trainer_session_total(const MorseTrainer* trainer) {
    return trainer ? trainer->session_groups : 0U;
}

bool morse_trainer_session_aborted(const MorseTrainer* trainer) {
    return trainer ? trainer->session_aborted : false;
}

uint8_t morse_trainer_session_fail_count(const MorseTrainer* trainer) {
    return trainer ? trainer->session_fail_count : 0U;
}

uint8_t morse_trainer_session_consecutive_missed(const MorseTrainer* trainer) {
    return trainer ? trainer->session_consecutive_missed : 0U;
}

uint16_t morse_trainer_session_letter_hits(const MorseTrainer* trainer) {
    return trainer ? trainer->session_letter_hits : 0U;
}

uint16_t morse_trainer_session_letter_total(const MorseTrainer* trainer) {
    if(trainer == NULL) return 0U;
    return trainer->session_letter_total;
}

uint8_t morse_trainer_session_letter_percent(const MorseTrainer* trainer) {
    uint16_t total;
    uint16_t hits;

    if(trainer == NULL) return 100U;
    total = morse_trainer_session_letter_total(trainer);
    if(total == 0U) return 100U;
    hits = morse_trainer_session_letter_hits(trainer);
    if(hits > total) hits = total;
    return (uint8_t)(((uint32_t)hits * 100U) / total);
}

uint8_t morse_trainer_session_average_score(const MorseTrainer* trainer) {
    if(trainer == NULL || trainer->session_scored_groups == 0U) {
        return 0U;
    }

    return (uint8_t)(trainer->session_score_sum / trainer->session_scored_groups);
}

bool morse_trainer_session_completed(const MorseTrainer* trainer) {
    if(trainer == NULL) return false;
    return !trainer->session_active && !trainer->session_aborted &&
           trainer->phase == MorseTrainerPhaseDone &&
           trainer->session_index >= trainer->session_groups &&
           trainer->session_scored_groups >= trainer->session_groups;
}
