#include "include/store_status.h"
#include <assert.h>
#include <stdio.h>

static void test_backup_slot_first_free(void) {
    bool taken[STORE_BACKUP_SLOTS] = {0};
    assert(store_status_backup_slot(taken) == 0);

    taken[0] = true;
    assert(store_status_backup_slot(taken) == 1);

    taken[1] = true;
    taken[2] = true;
    assert(store_status_backup_slot(taken) == 3);

    printf("test_backup_slot_first_free: PASSED\n");
}

static void test_backup_slot_all_taken(void) {
    bool taken[STORE_BACKUP_SLOTS];
    for(int i = 0; i < STORE_BACKUP_SLOTS; i++) {
        taken[i] = true;
    }
    assert(store_status_backup_slot(taken) == -1);

    printf("test_backup_slot_all_taken: PASSED\n");
}

int main(void) {
    printf("Running store_status tests...\n");
    test_backup_slot_first_free();
    test_backup_slot_all_taken();
    printf("All tests PASSED!\n");
    return 0;
}
