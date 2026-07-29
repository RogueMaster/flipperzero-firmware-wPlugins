// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/plugin_host.h"
#include "../plugins/qr_plugin_api.h"

#include <math.h>
#include <stdlib.h> // malloc/free for the scene-scoped snapshot

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

/**
 * Scene-scoped snapshot, allocated on entry and freed on exit. Was a static
 * array costing 1024 bytes of BSS for the whole app run. NULL degrades to
 * "no cameras", which is already a rendered state (see the g_cam_count == 0
 * branch), so there is no new failure mode to handle.
 */
static HandoffCam* g_cams;
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

/** QR encoder plugin, mapped in only while this screen is open. NULL when it
 *  could not be loaded -- the view then shows its "QR n/a" fallback. */
static PluginHost* g_qr_plugin = NULL;

void recon_scene_deflock_handoff_on_enter(void* context) {
    ReconApp* app = context;

    // Allocate the snapshot list. v0.48 (d0a12a3) moved this array off BSS to a
    // heap pointer in all three scenes that carried one, but only added the
    // allocation to the other two -- so g_cams was NULL on every entry here and
    // the `g_cams &&` guard below silently collected nothing. Share to DeFlock
    // reported "No marked cameras" no matter what was marked, from v0.48 to
    // v0.50. Same shape as recon_scene_guardian_sus.c and recon_scene_locator.c.
    if(!g_cams) g_cams = malloc(sizeof(HandoffCam) * HANDOFF_MAX);

    // Snapshot marked + geotagged cameras under the mutex into the local list.
    g_cam_count = 0;
    g_selected = 0;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    for(size_t i = 0; g_cams && i < app->flock_count && g_cam_count < HANDOFF_MAX; i++) {
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

    // Map the encoder in for the lifetime of this screen. A failure here is
    // not fatal: set_api(NULL) makes the view draw the text fallback, and the
    // coordinates are still readable and hand-enterable at deflock.org/report.
    const QrPluginApi* qr_api = NULL;
    g_qr_plugin = plugin_host_load(QR_PLUGIN_APP_ID, QR_PLUGIN_API_VERSION, (const void**)&qr_api);
    deflock_qr_view_set_api(app->deflock_qr_view, qr_api);

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
    ReconApp* app = context;
    // Drop the borrowed pointer BEFORE unmapping the plugin it points into, so
    // a later redraw of a stale model can never call through freed code.
    deflock_qr_view_set_api(app->deflock_qr_view, NULL);
    plugin_host_free(g_qr_plugin);
    g_qr_plugin = NULL;

    free(g_cams);
    g_cams = NULL;
    g_cam_count = 0;
    g_selected = 0;
}
