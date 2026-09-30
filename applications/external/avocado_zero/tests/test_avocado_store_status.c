#include "include/domain/avocado_store_status.h"
#include <assert.h>
#include <stdio.h>

static void test_autosave_allowed(void) {
    assert(avocado_store_autosave_allowed(AvocadoStoreLoadOk));
    assert(!avocado_store_autosave_allowed(AvocadoStoreLoadMissing));
    assert(!avocado_store_autosave_allowed(AvocadoStoreLoadCorrupt));
}

static void test_backup_slot_picks_first_free(void) {
    bool none_taken[AVOCADO_STORE_BACKUP_SLOTS] = {0};
    assert(avocado_store_backup_slot(none_taken) == 0);
}

// A second corruption, arriving while the first backup still exists, must move to the next
// free slot instead of overwriting it (keeps the first copy).
static void test_backup_slot_skips_taken_slots(void) {
    bool taken[AVOCADO_STORE_BACKUP_SLOTS] = {0};
    int first = avocado_store_backup_slot(taken);
    assert(first == 0);
    taken[first] = true;

    int second = avocado_store_backup_slot(taken);
    assert(second == 1);
}

static void test_backup_slot_all_taken_returns_negative(void) {
    bool all_taken[AVOCADO_STORE_BACKUP_SLOTS];
    for(int i = 0; i < AVOCADO_STORE_BACKUP_SLOTS; i++) {
        all_taken[i] = true;
    }
    assert(avocado_store_backup_slot(all_taken) == -1);
}

int main(void) {
    test_autosave_allowed();
    printf("test_autosave_allowed: PASS\n");

    test_backup_slot_picks_first_free();
    printf("test_backup_slot_picks_first_free: PASS\n");

    test_backup_slot_skips_taken_slots();
    printf("test_backup_slot_skips_taken_slots: PASS\n");

    test_backup_slot_all_taken_returns_negative();
    printf("test_backup_slot_all_taken_returns_negative: PASS\n");

    return 0;
}
