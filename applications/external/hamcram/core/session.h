/**
 * A session is the ordered list of questions the user is working through:
 * a practice exam, a study run over a topic, a review of missed questions,
 * or a look back at the questions missed in the exam just taken.
 */
#pragma once

#include "pool.h"
#include "progress.h"

typedef enum {
    SessionModeStudy,
    SessionModeReview,
    SessionModeExam,
    SessionModeExamReview,
} SessionMode;

#define SESSION_ANSWERED 0x10
#define SESSION_CORRECT  0x20

typedef struct {
    uint16_t question; /**< Index into the pool */
    uint8_t perm; /**< Display order of the answers, 2 bits per slot */
    uint8_t answer; /**< 0, or SESSION_ANSWERED | original answer index [| SESSION_CORRECT] */
} SessionItem;

typedef struct {
    SessionMode mode;
    uint16_t capacity;
    uint16_t count;
    uint16_t pos;
    uint16_t answered;
    uint16_t correct;
    SessionItem* items;
} Session;

Session* session_alloc(uint16_t capacity);
void session_free(Session* session);

/** One random question from every group, in group order, like the real exam */
bool session_start_exam(Session* session, const Pool* pool, bool skip_figures, bool shuffle);

/** Questions in [first, first + count) that are not mastered yet, in pool
 * order; all of them if every one is mastered. */
bool session_start_study(
    Session* session,
    const Pool* pool,
    const Progress* progress,
    uint16_t first,
    uint16_t count,
    bool skip_figures,
    bool shuffle);

/** Every question with a review weight, most-missed first */
bool session_start_review(
    Session* session,
    const Pool* pool,
    const Progress* progress,
    bool skip_figures,
    bool shuffle);

/** Narrow a finished exam down to the questions that were not answered
 * correctly, keeping the answer order the user saw. */
bool session_start_exam_review(Session* session);

/** Record the answer to the current question. `answer` is the original
 * (unshuffled) answer index. */
void session_answer(Session* session, uint8_t answer, bool correct);

static inline uint8_t session_perm_get(uint8_t perm, uint8_t slot) {
    return (perm >> (slot * 2)) & 0x03;
}

static inline bool session_item_answered(const SessionItem* item) {
    return (item->answer & SESSION_ANSWERED) != 0;
}

static inline bool session_item_correct(const SessionItem* item) {
    return (item->answer & SESSION_CORRECT) != 0;
}

static inline uint8_t session_item_answer(const SessionItem* item) {
    return item->answer & 0x03;
}
