// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

#include <math.h>

// Snapshot of the marked, geotagged cameras taken on_enter. This scene is
// passive: it starts no ESP/GPS link and holds no UART -- it only renders a
// QR/URL the user scans with their phone. The list is a copy so the draw path
// never touches the live flock[] under app->mutex.
#define HANDOFF_MAX RECON_FLOCK_MAX

typedef struct {
    float lat;
    float lon;
    float heading;
    FlockConfidence confidence;
} HandoffCam;

static HandoffCam g_cams[HANDOFF_MAX];
static int g_cam_count;
static int g_selected;

// Forward the QR view's Left/Right paging into the scene as a custom event that
// carries the signed delta (-1/+1). Casting to uint32_t and back round-trips.
static void recon_scene_deflock_handoff_page_cb(void* context, int delta) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)delta);
}

// Build the per-camera handoff content and push it to the QR view. The URL lands
// the user on the DeFlock map at the camera so they can add it in the official
// app; the on-screen text mirrors the OSM/DeFlock tagging from recon_report.c.
static void recon_scene_deflock_handoff_show(ReconApp* app) {
    if(g_cam_count == 0) {
        deflock_qr_view_set_empty(app->deflock_qr_view);
        return;
    }
    if(g_selected < 0) g_selected = 0;
    if(g_selected >= g_cam_count) g_selected = g_cam_count - 1;

    HandoffCam* c = &g_cams[g_selected];

    char url[64];
    snprintf(
        url, sizeof(url), "https://deflock.org/?lat=%.6f&lng=%.6f", (double)c->lat, (double)c->lon);

    char coords[28];
    snprintf(coords, sizeof(coords), "%.6f,%.6f", (double)c->lat, (double)c->lon);

    char conf[16];
    snprintf(conf, sizeof(conf), "Conf: %s", flock_confidence_str(c->confidence));

    // OSM tag set (display only -- the report writer owns the real serialization).
    char tags[96];
    if(!isnan(c->heading)) {
        snprintf(
            tags,
            sizeof(tags),
            "man_made=surveillance\nsurveillance:type=ALPR\nmanufacturer=Flock Safety\ndirection=%.0f",
            (double)c->heading);
    } else {
        snprintf(
            tags,
            sizeof(tags),
            "man_made=surveillance\nsurveillance:type=ALPR\nmanufacturer=Flock Safety");
    }

    deflock_qr_view_set_content(
        app->deflock_qr_view, url, g_selected, g_cam_count, coords, conf, tags);
}

void recon_scene_deflock_handoff_on_enter(void* context) {
    ReconApp* app = context;

    // Snapshot marked + geotagged cameras under the mutex into the local list.
    g_cam_count = 0;
    g_selected = 0;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    for(size_t i = 0; i < app->flock_count && g_cam_count < HANDOFF_MAX; i++) {
        FlockEntry* e = &app->flock[i];
        if(e->marked && !isnan(e->lat) && !isnan(e->lon)) {
            HandoffCam* c = &g_cams[g_cam_count++];
            c->lat = e->lat;
            c->lon = e->lon;
            c->heading = e->heading;
            c->confidence = e->confidence;
        }
    }
    furi_mutex_release(app->mutex);

    deflock_qr_view_set_page_callback(
        app->deflock_qr_view, recon_scene_deflock_handoff_page_cb, app);

    recon_scene_deflock_handoff_show(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewDeflockQr);
}

bool recon_scene_deflock_handoff_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Left/Right paging arrives as a custom event carrying the delta (+1/-1).
        if(g_cam_count > 0) {
            int next = g_selected + (int)event.event;
            if(next < 0) next = g_cam_count - 1;
            if(next >= g_cam_count) next = 0;
            g_selected = next;
            recon_scene_deflock_handoff_show(app);
        }
        consumed = true;
    }
    return consumed;
}

void recon_scene_deflock_handoff_on_exit(void* context) {
    UNUSED(context);
    g_cam_count = 0;
    g_selected = 0;
}
