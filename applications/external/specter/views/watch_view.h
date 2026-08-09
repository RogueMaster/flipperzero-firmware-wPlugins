#pragma once

#include <gui/view.h>
#include "../helpers/field_detector.h"

typedef struct WatchView WatchView;

typedef void (*WatchViewCallback)(void* context);

WatchView* watch_view_alloc(void);
void watch_view_free(WatchView* v);
View* watch_view_get_view(WatchView* v);

/* OK resets the watch (clears the count and the clock). */
void watch_view_set_reset_callback(WatchView* v, WatchViewCallback cb, void* ctx);

/* Snapshot pushed on every UI tick.
 *   watching_ms  - how long this watch has been armed
 *   first_ms/last_ms - offset from arm time of the first / most recent contact,
 *                      or WATCH_NO_TIME if there hasn't been one */
#define WATCH_NO_TIME UINT32_MAX
void watch_view_update(
    WatchView* v,
    const FieldStats* stats,
    uint32_t watching_ms,
    uint32_t first_ms,
    uint32_t last_ms);

void watch_view_tick(WatchView* v);
