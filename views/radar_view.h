#pragma once

#include <gui/view.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct RadarView RadarView;

typedef struct {
    int8_t rssi; /* signal strength -> distance from the centre. REAL. */
    uint8_t slot; /* 0..255 stable per-device angle. ARBITRARY - see below. */
    bool following;
    bool threat; /* a tracker type that can realistically be used to stalk */
} RadarBlip;

/**
 * What the radar is honest about.
 *
 * The distance of a blip from the centre is real: it is the measured RSSI,
 * mapped onto the range rings. The ANGLE is not a bearing. GhostTag has no
 * antenna array and no compass, so it cannot know which direction anything is
 * in. The angle is a stable hash of the device address, so each tracker keeps
 * its own spot on the dial and you can watch it move in and out - and that is
 * all it means. The view therefore draws range rings, which mean something,
 * and no crosshair, which would imply cardinal directions it does not have.
 */
typedef enum {
    RadarLinkDemo, /* simulated source - stamped DEMO */
    RadarLinkUp, /* the board is talking to us */
    RadarLinkWaiting, /* hunting, but no board has ever answered */
    RadarLinkLost, /* a board answered earlier and has now gone quiet */
} RadarLinkState;

typedef void (*RadarViewCallback)(void* context);

RadarView* radar_view_alloc(void);
void radar_view_free(RadarView* radar);
View* radar_view_get_view(RadarView* radar);

void radar_view_set_data(
    RadarView* radar,
    const RadarBlip* blips,
    size_t blip_count,
    size_t seen,
    size_t tags,
    size_t following,
    RadarLinkState link);

/** Advance the sweep animation one frame. */
void radar_view_tick(RadarView* radar);

/** OK opens the detections list. */
void radar_view_set_ok_callback(RadarView* radar, RadarViewCallback cb, void* context);

/** Left opens Help & About - the way out when there is no board attached. */
void radar_view_set_help_callback(RadarView* radar, RadarViewCallback cb, void* context);
