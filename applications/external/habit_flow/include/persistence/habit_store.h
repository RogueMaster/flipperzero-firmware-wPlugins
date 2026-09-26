#pragma once

#include "include/domain/habit.h"
#include "include/domain/hf_store_status.h"
#include "include/ports/hf_store_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t session_date_packed;
    uint32_t habit_count;
    Habit habits[HF_HABITS_MAX];
    bool save_blocked; // transient: sticky refusal to save until the next successful load
} HabitStore;

void habit_store_init(HabitStore* store);

// Loads the store through `port`. A missing file is not an error and leaves `notice`
// empty. A present-but-unreadable file, or one whose last save was interrupted (see
// hf_store_port.h on why `rename` needs this), is recovered where possible; otherwise it
// is quarantined to "<file>.bad" (or the next free ".bad1".."bad9" slot) and `notice` is
// filled with a message for the user. `notice` may be NULL.
HfStoreLoadStatus
    habit_store_load(const HfStorePort* port, HabitStore* store, char* notice, size_t notice_size);

// May set `store->save_blocked` if `bin` is unreadable and `.tmp` can't safely stand in
// for it either (see habit_store.c): only known-good data may ever be risked on a write.
bool habit_store_save(const HfStorePort* port, HabitStore* store);

bool habit_store_add(const HfStorePort* port, HabitStore* store, const Habit* habit);

bool habit_store_replace_at(
    const HfStorePort* port,
    HabitStore* store,
    size_t index,
    const Habit* habit);

bool habit_store_delete_at(const HfStorePort* port, HabitStore* store, size_t index);
