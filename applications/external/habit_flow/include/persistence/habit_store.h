#pragma once

#include "include/domain/habit.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t session_date_packed;
    uint32_t habit_count;
    Habit habits[HF_HABITS_MAX];
} HabitStore;

void habit_store_init(HabitStore *store);

bool habit_store_load(HabitStore *store);

bool habit_store_save(const HabitStore *store);

bool habit_store_add(HabitStore *store, const Habit *habit);

bool habit_store_replace_at(HabitStore *store, size_t index, const Habit *habit);

bool habit_store_delete_at(HabitStore *store, size_t index);
