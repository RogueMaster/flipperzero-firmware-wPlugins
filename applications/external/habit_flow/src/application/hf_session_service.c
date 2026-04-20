#include "include/application/hf_session_service.h"
#include "include/domain/habit_date.h"
#include <furi.h>
#include <string.h>

void hf_yesterday_queue_reset(HfYesterdayQueue *q) {
    if (q) {
        memset(q, 0, sizeof(*q));
    }
}

void hf_session_on_resume(HabitStore *store, uint32_t today_packed, HfYesterdayQueue *yq) {
    furi_assert(store && yq);
    hf_yesterday_queue_reset(yq);

    if (store->session_date_packed == 0) {
        store->session_date_packed = today_packed;
        habit_store_save(store);
        return;
    }

    const int32_t diff = hf_date_diff_days(store->session_date_packed, today_packed);
    if (diff <= 0) {
        return;
    }
    if (diff > 1) {
        for (uint32_t i = 0; i < store->habit_count; i++) {
            hf_habit_on_long_gap(&store->habits[i]);
        }
        store->session_date_packed = today_packed;
        habit_store_save(store);
        return;
    }

    yq->count = 0;
    for (uint32_t i = 0; i < store->habit_count; i++) {
        if (store->habits[i].completed_today) {
            hf_habit_apply_overnight(&store->habits[i], true);
        } else {
            yq->indices[yq->count++] = i;
        }
    }
    habit_store_save(store);
    if (yq->count == 0) {
        store->session_date_packed = today_packed;
        habit_store_save(store);
    } else {
        yq->pos = 0;
    }
}

void hf_session_close_yesterday_flow(HabitStore *store, uint32_t today_packed,
                                     HfYesterdayQueue *yq) {
    furi_assert(store && yq);
    store->session_date_packed = today_packed;
    habit_store_save(store);
    hf_yesterday_queue_reset(yq);
}
