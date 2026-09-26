#pragma once

#include "include/domain/habit.h"
#include "include/persistence/habit_store.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t indices[HF_HABITS_MAX];
    size_t count;
    size_t pos;
} HfYesterdayQueue;

// `autosave_allowed` gates every save this function makes on its own (the daily rollover);
// it must be false whenever launch didn't load a clean store, so it never persists over
// lost or unknown data before the user has made an explicit change.
void hf_session_on_resume(
    const HfStorePort* port,
    HabitStore* store,
    uint32_t today_packed,
    HfYesterdayQueue* yq,
    bool autosave_allowed);

void hf_session_close_yesterday_flow(
    const HfStorePort* port,
    HabitStore* store,
    uint32_t today_packed,
    HfYesterdayQueue* yq);
