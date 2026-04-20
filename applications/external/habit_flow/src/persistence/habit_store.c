#include "include/persistence/habit_store.h"
#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define HF_STORE_MAGIC 0x48464231u
#define HF_STORE_VERSION 1u

#define APPS_DATA_DIR "/ext/apps_data/habitflow"
#define HABITFLOW_BIN APPS_DATA_DIR "/habitflow.bin"

typedef struct __attribute__((packed)) {
    char name[HF_HABIT_NAME_MAX];
    uint16_t streak;
    uint16_t max_streak;
    uint8_t completed_today;
    uint8_t history[HF_HISTORY_DAYS];
    uint16_t goal_days;
    uint8_t mastered;
} HabitSerialized;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t session_date_packed;
    uint32_t habit_count;
} HabitStoreHeader;

static void ser_to_habit(const HabitSerialized *s, Habit *h) {
    memset(h, 0, sizeof(*h));
    memcpy(h->name, s->name, sizeof(h->name));
    h->name[HF_HABIT_NAME_MAX - 1] = '\0';
    h->streak = s->streak;
    h->max_streak = s->max_streak;
    h->completed_today = s->completed_today != 0;
    memcpy(h->history, s->history, sizeof(h->history));
    h->goal_days = s->goal_days;
    if (h->goal_days < HF_GOAL_MIN) {
        h->goal_days = HF_DEFAULT_GOAL;
    }
    if (h->goal_days > HF_GOAL_MAX) {
        h->goal_days = HF_GOAL_MAX;
    }
    h->mastered = s->mastered != 0;
}

static void habit_to_ser(const Habit *h, HabitSerialized *s) {
    memset(s, 0, sizeof(*s));
    memcpy(s->name, h->name, sizeof(s->name));
    s->streak = h->streak;
    s->max_streak = h->max_streak;
    s->completed_today = h->completed_today ? 1u : 0u;
    memcpy(s->history, h->history, sizeof(s->history));
    s->goal_days = h->goal_days;
    s->mastered = h->mastered ? 1u : 0u;
}

void habit_store_init(HabitStore *store) {
    furi_assert(store);
    memset(store, 0, sizeof(*store));
}

bool habit_store_load(HabitStore *store) {
    furi_assert(store);
    habit_store_init(store);

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);
    bool ok = false;

    if (storage_file_open(file, HABITFLOW_BIN, FSAM_READ, FSOM_OPEN_EXISTING)) {
        HabitStoreHeader head = {0};
        if (storage_file_read(file, &head, sizeof(head)) == sizeof(head) &&
            head.magic == HF_STORE_MAGIC && head.version == HF_STORE_VERSION &&
            head.habit_count <= HF_HABITS_MAX) {
            store->session_date_packed = head.session_date_packed;
            store->habit_count = head.habit_count;
            ok = true;
            for (uint32_t i = 0; i < store->habit_count && ok; i++) {
                HabitSerialized ser = {0};
                if (storage_file_read(file, &ser, sizeof(ser)) != sizeof(ser)) {
                    ok = false;
                    break;
                }
                ser_to_habit(&ser, &store->habits[i]);
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if (!ok) {
        habit_store_init(store);
    }
    return ok;
}

bool habit_store_save(const HabitStore *store) {
    furi_assert(store);
    if (store->habit_count > HF_HABITS_MAX) {
        return false;
    }

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);
    storage_common_mkdir(storage, APPS_DATA_DIR);

    bool ok = false;
    if (storage_file_open(file, HABITFLOW_BIN, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const HabitStoreHeader head = {
            .magic = HF_STORE_MAGIC,
            .version = HF_STORE_VERSION,
            .session_date_packed = store->session_date_packed,
            .habit_count = store->habit_count,
        };
        if (storage_file_write(file, &head, sizeof(head)) == sizeof(head)) {
            ok = true;
            for (uint32_t i = 0; i < store->habit_count && ok; i++) {
                HabitSerialized ser = {0};
                habit_to_ser(&store->habits[i], &ser);
                if (storage_file_write(file, &ser, sizeof(ser)) != sizeof(ser)) {
                    ok = false;
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool habit_store_add(HabitStore *store, const Habit *habit) {
    furi_assert(store && habit);
    if (store->habit_count >= HF_HABITS_MAX) {
        return false;
    }
    store->habits[store->habit_count] = *habit;
    store->habit_count++;
    return habit_store_save(store);
}

bool habit_store_replace_at(HabitStore *store, size_t index, const Habit *habit) {
    furi_assert(store && habit);
    if (index >= store->habit_count) {
        return false;
    }
    store->habits[index] = *habit;
    return habit_store_save(store);
}

bool habit_store_delete_at(HabitStore *store, size_t index) {
    furi_assert(store);
    if (index >= store->habit_count || store->habit_count == 0) {
        return false;
    }
    for (size_t i = index + 1; i < store->habit_count; i++) {
        store->habits[i - 1] = store->habits[i];
    }
    store->habit_count--;
    memset(&store->habits[store->habit_count], 0, sizeof(Habit));
    return habit_store_save(store);
}
