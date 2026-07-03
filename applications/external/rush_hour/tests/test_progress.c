#include "../include/persistence/progress.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    TutuProgress p;
    tutu_progress_default(&p);
    assert(p.highest_unlocked == 0);
    assert(tutu_progress_is_unlocked(&p, 0));
    assert(!tutu_progress_is_unlocked(&p, 1));
    assert(!tutu_progress_is_completed(&p, 0));

    tutu_progress_mark_completed(&p, 5);
    assert(tutu_progress_is_completed(&p, 5));
    assert(!tutu_progress_is_completed(&p, 6));

    uint16_t hu = tutu_progress_complete_and_unlock(&p, 0, 100);
    assert(hu == 1);
    assert(tutu_progress_is_completed(&p, 0));
    assert(tutu_progress_is_unlocked(&p, 1));

    // completing a later level extends unlock but never regresses it
    tutu_progress_complete_and_unlock(&p, 3, 100);
    assert(p.highest_unlocked == 4);
    tutu_progress_complete_and_unlock(&p, 0, 100); // already done; unlock must not drop
    assert(p.highest_unlocked == 4);

    // clamp at the last level
    uint16_t last = tutu_progress_complete_and_unlock(&p, 99, 100);
    assert(last == 99);

    printf("test_progress: all passed\n");
    return 0;
}
