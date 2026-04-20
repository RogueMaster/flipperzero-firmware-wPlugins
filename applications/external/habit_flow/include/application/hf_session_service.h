#pragma once

#include "include/domain/habit.h"
#include "include/persistence/habit_store.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t indices[HF_HABITS_MAX];
    size_t count;
    size_t pos;
} HfYesterdayQueue;

void hf_yesterday_queue_reset(HfYesterdayQueue *q);

void hf_session_on_resume(HabitStore *store, uint32_t today_packed, HfYesterdayQueue *yq);

void hf_session_close_yesterday_flow(HabitStore *store, uint32_t today_packed,
                                     HfYesterdayQueue *yq);
