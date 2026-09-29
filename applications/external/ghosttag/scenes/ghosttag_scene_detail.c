#include "../ghosttag_i.h"
#include <string.h>

/* Left/Right walk the detection list without going back to it. Before v2.0
 * this screen had no usable key at all: every press did nothing, which is the
 * definition of a dead button. */
static void ghosttag_scene_detail_step(GhostTagApp* app, int delta) {
    TrackerRecord* snap = app->scratch;
    size_t n = tracker_db_snapshot(app->db, snap, TRACKER_DB_MAX);
    if(n == 0) return;

    size_t cur = 0;
    for(size_t i = 0; i < n; i++) {
        if(memcmp(snap[i].mac, app->detail_record.mac, 6) == 0) {
            cur = i;
            break;
        }
    }

    size_t next;
    if(delta < 0) {
        next = (cur == 0) ? n - 1 : cur - 1;
    } else {
        next = (cur + 1 >= n) ? 0 : cur + 1;
    }

    app->detail_record = snap[next];
    device_detail_view_set_record(app->device_detail_view, &app->detail_record);
    device_detail_view_set_position(app->device_detail_view, next + 1, n);
}

static void ghosttag_scene_detail_refresh(GhostTagApp* app) {
    TrackerRecord* snap = app->scratch;
    size_t n = tracker_db_snapshot(app->db, snap, TRACKER_DB_MAX);
    for(size_t i = 0; i < n; i++) {
        if(memcmp(snap[i].mac, app->detail_record.mac, 6) == 0) {
            /* Keep the live numbers moving. A frozen RSSI on a screen titled
             * "signal" reads as a hung app. */
            app->detail_record = snap[i];
            device_detail_view_set_record(app->device_detail_view, &app->detail_record);
            device_detail_view_set_position(app->device_detail_view, i + 1, n);
            return;
        }
    }
    device_detail_view_set_position(app->device_detail_view, 0, n);
}

static void ghosttag_scene_detail_prev_cb(void* context) {
    ghosttag_scene_detail_step(context, -1);
}

static void ghosttag_scene_detail_next_cb(void* context) {
    ghosttag_scene_detail_step(context, +1);
}

void ghosttag_scene_detail_on_enter(void* context) {
    GhostTagApp* app = context;
    device_detail_view_set_record(app->device_detail_view, &app->detail_record);
    device_detail_view_set_demo(app->device_detail_view, app->source == GhostTagSourceDemo);
    device_detail_view_set_step_callbacks(
        app->device_detail_view, ghosttag_scene_detail_prev_cb, ghosttag_scene_detail_next_cb, app);
    ghosttag_scene_detail_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewDeviceDetail);
}

bool ghosttag_scene_detail_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        /* This screen used to drop the alert entirely, and because taking a
         * pending alert clears it, the warning was then gone for good. */
        if(ghosttag_poll_alert(app)) return true;
        if(ghosttag_is_hunting(app)) ghosttag_scene_detail_refresh(app);
        return true;
    }
    return false;
}

void ghosttag_scene_detail_on_exit(void* context) {
    GhostTagApp* app = context;
    device_detail_view_set_step_callbacks(app->device_detail_view, NULL, NULL, NULL);
}
