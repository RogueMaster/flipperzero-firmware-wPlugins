#include "../../include/persistence/progress.h"

void tutu_progress_default(TutuProgress *p) {
    p->highest_unlocked = 0;
    for (int i = 0; i < TUTU_PROGRESS_BYTES; i++)
        p->completed[i] = 0;
}

bool tutu_progress_is_completed(const TutuProgress *p, uint16_t n) {
    return (p->completed[n >> 3] >> (n & 7)) & 1u;
}

void tutu_progress_mark_completed(TutuProgress *p, uint16_t n) {
    p->completed[n >> 3] |= (uint8_t)(1u << (n & 7));
}

uint16_t tutu_progress_complete_and_unlock(TutuProgress *p, uint16_t n, uint16_t count) {
    tutu_progress_mark_completed(p, n);
    uint16_t next = (n + 1 < count) ? (uint16_t)(n + 1) : (uint16_t)(count - 1);
    if (next > p->highest_unlocked)
        p->highest_unlocked = next;
    return p->highest_unlocked;
}

bool tutu_progress_is_unlocked(const TutuProgress *p, uint16_t n) {
    return n <= p->highest_unlocked;
}
