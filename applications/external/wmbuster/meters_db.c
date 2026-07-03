#include "meters_db.h"
#include <string.h>

WmbusMeter* meters_db_upsert(WmbusApp* app, uint16_t m, uint32_t id) {
    /* Linear scan: cap is small (32), no need for hash. */
    for(size_t i = 0; i < app->meter_count; i++) {
        if(app->meters[i].manuf == m && app->meters[i].id == id) {
            app->meters[i].last_seen_tick = furi_get_tick();
            app->meters[i].telegram_count++;
            return &app->meters[i];
        }
    }
    if(app->meter_count < WMBUS_MAX_METERS) {
        WmbusMeter* row = &app->meters[app->meter_count++];
        memset(row, 0, sizeof(*row));
        row->manuf = m;
        row->id = id;
        row->last_seen_tick = furi_get_tick();
        row->telegram_count = 1;
        return row;
    }
    /* Evict LRU */
    size_t lru = 0;
    for(size_t i = 1; i < app->meter_count; i++)
        if(app->meters[i].last_seen_tick < app->meters[lru].last_seen_tick) lru = i;
    memset(&app->meters[lru], 0, sizeof(app->meters[lru]));
    app->meters[lru].manuf = m;
    app->meters[lru].id = id;
    app->meters[lru].last_seen_tick = furi_get_tick();
    app->meters[lru].telegram_count = 1;
    return &app->meters[lru];
}

void meters_db_clear(WmbusApp* app) {
    app->meter_count = 0;
    memset(app->meters, 0, sizeof(app->meters));
}
