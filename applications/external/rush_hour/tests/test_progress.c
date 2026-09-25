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

    // Missing save data: no slot taken yet, so the first quarantine goes to slot 0.
    bool none_taken[TUTU_PROGRESS_BACKUP_SLOTS] = {0};
    assert(tutu_progress_backup_slot(none_taken) == 0);

    // A second corruption, arriving while the first backup still exists, must move to the
    // next free slot instead of overwriting it (keeps the first copy).
    bool taken[TUTU_PROGRESS_BACKUP_SLOTS] = {0};
    int first = tutu_progress_backup_slot(taken);
    assert(first == 0);
    taken[first] = true;
    int second = tutu_progress_backup_slot(taken);
    assert(second == 1);

    bool all_taken[TUTU_PROGRESS_BACKUP_SLOTS];
    for(int i = 0; i < TUTU_PROGRESS_BACKUP_SLOTS; i++)
        all_taken[i] = true;
    assert(tutu_progress_backup_slot(all_taken) == -1);

    printf("test_progress: all passed\n");
    return 0;
}
