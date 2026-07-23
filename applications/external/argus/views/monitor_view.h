#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MONITOR_MAX_BLIPS 16

typedef struct {
    uint8_t angle; // 0..255 bearing around the iris
    int8_t rssi; // maps to distance from the pupil
    bool clone; // evil twin -> threat marker / pulse
} MonitorBlip;

typedef struct MonitorView MonitorView;
typedef void (*MonitorViewCallback)(void* context);

MonitorView* monitor_view_alloc(void);
void monitor_view_free(MonitorView* v);
View* monitor_view_get_view(MonitorView* v);

void monitor_view_set_ok_callback(MonitorView* v, MonitorViewCallback cb, void* context);

void monitor_view_update(
    MonitorView* v,
    const MonitorBlip* blips,
    size_t blip_count,
    uint32_t deauth_total,
    uint32_t deauth_rate,
    size_t ap_count,
    size_t twin_count,
    bool esp_connected,
    bool under_attack,
    const char* guard);

void monitor_view_tick(MonitorView* v); // advance the sweep / animation
