#include "tpms_store.h"

#include <furi.h>

void tpms_store_reset(TpmsStore* store) {
    furi_check(store);
    memset(store, 0, sizeof(TpmsStore));
}

static int tpms_store_find(const TpmsStore* store, uint8_t protocol, uint32_t id) {
    for(uint8_t i = 0; i < store->count; i++) {
        if(store->items[i].id == id && store->items[i].protocol == protocol) return i;
    }
    return -1;
}

/** Who to evict when the table is full: the one silent for longest. */
static uint8_t tpms_store_oldest(const TpmsStore* store, uint32_t now_tick) {
    uint8_t victim = 0;
    uint32_t worst = 0;
    for(uint8_t i = 0; i < store->count; i++) {
        const uint32_t age = now_tick - store->items[i].last_tick;
        if(age >= worst) {
            worst = age;
            victim = i;
        }
    }
    return victim;
}

uint8_t
    tpms_store_update(TpmsStore* store, const TpmsFrame* frame, int16_t rssi_x10, uint32_t tick) {
    furi_check(store);
    furi_check(frame);

    int index = tpms_store_find(store, frame->protocol, frame->id);
    if(index < 0) {
        if(store->count < TPMS_STORE_CAPACITY) {
            index = store->count++;
        } else {
            index = tpms_store_oldest(store, tick);
        }
        memset(&store->items[index], 0, sizeof(TpmsSensor));
        store->items[index].protocol = frame->protocol;
        store->items[index].id = frame->id;
        store->items[index].first_tick = tick;
        store->items[index].peak_rssi_x10 = rssi_x10;
        store->items[index].peak_tick = tick;
    }

    TpmsSensor* sensor = &store->items[index];

    if(frame->have & TPMS_HAS_PRESSURE) sensor->pressure_kpa_x100 = frame->pressure_kpa_x100;
    if(frame->have & TPMS_HAS_TEMP) sensor->temperature_c = frame->temperature_c;
    sensor->have |= frame->have;
    /* Battery state is a live reading, not something to accumulate. */
    if(frame->have & TPMS_HAS_BATTERY) {
        sensor->have &= (uint8_t)~TPMS_BATTERY_LOW;
        sensor->have |= (uint8_t)(frame->have & TPMS_BATTERY_LOW);
    }

    sensor->flags = frame->flags;
    sensor->raw_len = frame->raw_len;
    memcpy(sensor->raw, frame->raw, frame->raw_len);

    sensor->rssi_x10 = rssi_x10;
    sensor->frames++;
    sensor->last_tick = tick;

    const bool peak_expired = (tick - sensor->peak_tick) >
                              furi_ms_to_ticks(TPMS_STORE_PEAK_HOLD_MS);
    if(rssi_x10 >= sensor->peak_rssi_x10 || peak_expired) {
        sensor->peak_rssi_x10 = rssi_x10;
        sensor->peak_tick = tick;
    }

    store->total_frames++;
    return (uint8_t)index;
}

bool tpms_sensor_is_stale(const TpmsSensor* sensor, uint32_t now_tick) {
    furi_check(sensor);
    return (now_tick - sensor->last_tick) > furi_ms_to_ticks(TPMS_STORE_STALE_MS);
}
