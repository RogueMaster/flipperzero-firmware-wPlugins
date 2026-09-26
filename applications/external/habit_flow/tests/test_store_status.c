#include "include/domain/hf_store_status.h"
#include <assert.h>
#include <stdio.h>

static void test_autosave_allowed(void) {
    assert(hf_store_autosave_allowed(HfStoreLoadOk));
    assert(!hf_store_autosave_allowed(HfStoreLoadMissing));
    assert(!hf_store_autosave_allowed(HfStoreLoadCorrupt));
    assert(!hf_store_autosave_allowed(HfStoreLoadStatError));
}

static void test_backup_slot_picks_first_free(void) {
    bool none_taken[HF_STORE_BACKUP_SLOTS] = {0};
    assert(hf_store_backup_slot(none_taken) == 0);

    bool all_taken[HF_STORE_BACKUP_SLOTS];
    for(int i = 0; i < HF_STORE_BACKUP_SLOTS; i++) {
        all_taken[i] = true;
    }
    assert(hf_store_backup_slot(all_taken) == -1);
}

// A second corruption, arriving while the first backup still exists, must move to the
// next free slot instead of overwriting it; running out of slots refuses entirely.
static void test_backup_slot_skips_taken_slots(void) {
    bool taken[HF_STORE_BACKUP_SLOTS] = {0};
    int first = hf_store_backup_slot(taken);
    assert(first == 0);
    taken[first] = true;

    int second = hf_store_backup_slot(taken);
    assert(second == 1);
    taken[second] = true;

    for(int i = 2; i < HF_STORE_BACKUP_SLOTS; i++) {
        taken[i] = true;
    }
    assert(hf_store_backup_slot(taken) == -1);
}

int main(void) {
    test_autosave_allowed();
    test_backup_slot_picks_first_free();
    test_backup_slot_skips_taken_slots();
    puts("test_store_status: ok");
    return 0;
}
