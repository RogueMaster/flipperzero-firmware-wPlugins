#pragma once

#include <gui/view.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct RadarView RadarView;

typedef struct {
    int8_t rssi; // signal strength -> distance from centre
    uint8_t angle; // 0..255 stable bearing derived from MAC
    bool following; // drawn as a pulsing threat blip
} RadarBlip;

typedef void (*RadarViewCallback)(void* context);

RadarView* radar_view_alloc(void);
void radar_view_free(RadarView* radar);
View* radar_view_get_view(RadarView* radar);

/** Push the current detection set + counters into the view. */
void radar_view_set_data(
    RadarView* radar,
    const RadarBlip* blips,
    size_t blip_count,
    size_t seen,
    size_t following,
    bool esp_connected);

/** Advance the sweep animation one frame. */
void radar_view_tick(RadarView* radar);

/** Called when OK is pressed (open detections list). */
void radar_view_set_ok_callback(RadarView* radar, RadarViewCallback cb, void* context);
