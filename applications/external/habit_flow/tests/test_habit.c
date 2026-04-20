#include "include/domain/habit.h"
#include "include/domain/habit_date.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_date_roundtrip(void) {
    const uint32_t p = hf_date_pack(2026, 4, 19);
    uint16_t y;
    uint8_t m, d;
    hf_date_unpack(p, &y, &m, &d);
    assert(y == 2026 && m == 4 && d == 19);
    assert(hf_date_add_days(p, 1) == hf_date_pack(2026, 4, 20));
    assert(hf_date_add_days(p, -1) == hf_date_pack(2026, 4, 18));
    assert(hf_date_diff_days(hf_date_pack(2026, 1, 1), hf_date_pack(2026, 1, 2)) == 1);
    assert(hf_date_diff_days(hf_date_pack(2025, 12, 31), hf_date_pack(2026, 1, 1)) == 1);
}

static void test_toggle_and_streak(void) {
    Habit h;
    hf_habit_set_defaults(&h, "Run");
    bool m = false;
    assert(h.streak == 0);
    hf_habit_toggle_today(&h, &m);
    assert(h.completed_today && h.streak == 1 && !m);
    hf_habit_toggle_today(&h, &m);
    assert(!h.completed_today && h.streak == 0);
}

static void test_overnight(void) {
    Habit h;
    hf_habit_set_defaults(&h, "Read");
    h.streak = 3;
    h.completed_today = true;
    hf_habit_apply_overnight(&h, true);
    assert(h.streak == 3 && !h.completed_today);
    assert(h.history[HF_HISTORY_DAYS - 1] == 1u);

    hf_habit_set_defaults(&h, "Read");
    h.streak = 5;
    h.completed_today = false;
    hf_habit_apply_overnight(&h, false);
    assert(h.streak == 0 && h.history[HF_HISTORY_DAYS - 1] == 0u);
}

static void test_mastery_toggle(void) {
    Habit h;
    hf_habit_set_defaults(&h, "Gym");
    h.goal_days = 3;
    h.streak = 2;
    h.completed_today = false;
    bool m = false;
    hf_habit_toggle_today(&h, &m);
    assert(h.streak == 3 && h.mastered && m);
    assert(hf_habit_shows_medal(&h));
    assert(hf_habit_progress_pct(&h) == 100u);
}

static void test_long_gap(void) {
    Habit h;
    hf_habit_set_defaults(&h, "X");
    h.streak = 9;
    h.history[3] = 1;
    hf_habit_on_long_gap(&h);
    assert(h.streak == 0);
    for (int i = 0; i < HF_HISTORY_DAYS; i++) {
        assert(h.history[i] == 0);
    }
}

int main(void) {
    test_date_roundtrip();
    test_toggle_and_streak();
    test_overnight();
    test_mastery_toggle();
    test_long_gap();
    puts("test_habit: ok");
    return 0;
}
