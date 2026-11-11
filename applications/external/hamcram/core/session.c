#include "session.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <string.h>

static uint32_t session_random(uint32_t limit) {
    return furi_hal_random_get() % limit;
}

static uint8_t session_make_perm(uint8_t flags, bool shuffle) {
    uint8_t order[POOL_ANSWERS] = {0, 1, 2, 3};
    if(shuffle) {
        /* "All these choices are correct" only reads right in last place */
        uint8_t n = (flags & POOL_FLAG_PIN_LAST) ? POOL_ANSWERS - 1 : POOL_ANSWERS;
        for(uint8_t i = n - 1; i > 0; i--) {
            uint8_t j = session_random(i + 1);
            uint8_t swap = order[i];
            order[i] = order[j];
            order[j] = swap;
        }
    }
    return order[0] | (order[1] << 2) | (order[2] << 4) | (order[3] << 6);
}

static bool session_eligible(const Pool* pool, uint16_t question, bool skip_figures) {
    return !(skip_figures && (pool->index[question].flags & POOL_FLAG_FIGURE));
}

static void session_clear(Session* session, SessionMode mode) {
    session->mode = mode;
    session->count = 0;
    session->pos = 0;
    session->answered = 0;
    session->correct = 0;
}

static void session_add(Session* session, const Pool* pool, uint16_t question, bool shuffle) {
    if(session->count >= session->capacity) return;
    SessionItem* item = &session->items[session->count++];
    item->question = question;
    item->perm = session_make_perm(pool->index[question].flags, shuffle);
    item->answer = 0;
}

Session* session_alloc(uint16_t capacity) {
    Session* session = malloc(sizeof(Session));
    memset(session, 0, sizeof(Session));
    session->capacity = capacity;
    session->items = malloc(sizeof(SessionItem) * (capacity ? capacity : 1));
    return session;
}

void session_free(Session* session) {
    if(!session) return;
    free(session->items);
    free(session);
}

bool session_start_exam(Session* session, const Pool* pool, bool skip_figures, bool shuffle) {
    session_clear(session, SessionModeExam);
    for(uint8_t g = 0; g < pool->header.num_groups; g++) {
        const PoolGroup* group = &pool->groups[g];
        uint16_t eligible = 0;
        for(uint16_t i = 0; i < group->num_questions; i++) {
            if(session_eligible(pool, group->first_question + i, skip_figures)) eligible++;
        }
        /* A group made only of figure questions still has to be on the exam */
        bool filter = eligible > 0;
        uint16_t pick = session_random(filter ? eligible : group->num_questions);
        for(uint16_t i = 0; i < group->num_questions; i++) {
            uint16_t question = group->first_question + i;
            if(filter && !session_eligible(pool, question, skip_figures)) continue;
            if(pick-- == 0) {
                session_add(session, pool, question, shuffle);
                break;
            }
        }
    }
    return session->count > 0;
}

bool session_start_study(
    Session* session,
    const Pool* pool,
    const Progress* progress,
    uint16_t first,
    uint16_t count,
    bool skip_figures,
    bool shuffle) {
    session_clear(session, SessionModeStudy);
    uint32_t end = (uint32_t)first + count;
    if(end > pool->header.num_questions) end = pool->header.num_questions;

    for(uint8_t pass = 0; pass < 2 && session->count == 0; pass++) {
        for(uint32_t q = first; q < end; q++) {
            if(!session_eligible(pool, q, skip_figures)) continue;
            if(pass == 0 && progress_mastered(progress, q)) continue;
            session_add(session, pool, q, shuffle);
        }
    }
    return session->count > 0;
}

bool session_start_review(
    Session* session,
    const Pool* pool,
    const Progress* progress,
    bool skip_figures,
    bool shuffle) {
    session_clear(session, SessionModeReview);
    for(uint8_t weight = PROGRESS_WEIGHT_MAX; weight > 0; weight--) {
        uint16_t start = session->count;
        for(uint16_t q = 0; q < pool->header.num_questions; q++) {
            if(progress_weight(progress, q) != weight) continue;
            if(!session_eligible(pool, q, skip_figures)) continue;
            session_add(session, pool, q, shuffle);
        }
        /* Mix up questions of equal weight so review order is not predictable */
        for(uint16_t i = session->count; i > start + 1; i--) {
            uint16_t j = start + session_random(i - start);
            SessionItem swap = session->items[i - 1];
            session->items[i - 1] = session->items[j];
            session->items[j] = swap;
        }
    }
    return session->count > 0;
}

bool session_start_exam_review(Session* session) {
    uint16_t kept = 0;
    for(uint16_t i = 0; i < session->count; i++) {
        if(session_item_correct(&session->items[i])) continue;
        session->items[kept++] = session->items[i];
    }
    session->mode = SessionModeExamReview;
    session->count = kept;
    session->pos = 0;
    return kept > 0;
}

void session_answer(Session* session, uint8_t answer, bool correct) {
    if(session->pos >= session->count) return;
    SessionItem* item = &session->items[session->pos];
    if(session_item_answered(item)) {
        session->answered--;
        if(session_item_correct(item)) session->correct--;
    }
    item->answer = SESSION_ANSWERED | (answer & 0x03) | (correct ? SESSION_CORRECT : 0);
    session->answered++;
    if(correct) session->correct++;
}
