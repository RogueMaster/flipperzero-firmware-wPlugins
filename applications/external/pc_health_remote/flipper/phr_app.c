#include "phr_app.h"
#include <stdlib.h>
#include <string.h>
#include "scenes/phr_scene.h"
#include "signals.h"
#include "advice.h"

#define TAG "PhrApp"

uint32_t phr_app_now_s(void) {
    return furi_get_tick() / 1000;
}

/* ---------- transport management ---------- */

void phr_app_apply_transport(PhrApp* app) {
    if(app->transport) {
        app->transport_api->stop(app->transport);
        app->transport = NULL;
    }
    phr_link_reset(app->link);
    alerts_reset_state(&app->alerts);
    app->transport_api = phr_transport_by_id(app->settings.transport);
    app->transport = app->transport_api->start(app->link);
    app->transport_failed = (app->transport == NULL);
    if(app->transport_failed) FURI_LOG_E(TAG, "Transport %s failed to start", app->transport_api->name);
}

/* ---------- alert queue ---------- */

static void pending_push(PhrApp* app, const AlertEvent* ev) {
    for(size_t i = 0; i < app->pending_count; i++) {
        if(app->pending[i].rule == ev->rule) {
            app->pending[i] = *ev; // refresh instead of duplicating
            return;
        }
    }
    if(app->pending_count == PHR_ALERT_QUEUE) {
        memmove(&app->pending[0], &app->pending[1], sizeof(AlertEvent) * (PHR_ALERT_QUEUE - 1));
        app->pending_count--;
    }
    app->pending[app->pending_count++] = *ev;
}

bool phr_app_pop_pending(PhrApp* app, AlertEvent* out) {
    if(app->pending_count == 0) return false;
    *out = app->pending[0];
    app->pending_count--;
    memmove(&app->pending[0], &app->pending[1], sizeof(AlertEvent) * app->pending_count);
    return true;
}

/* ---------- periodic work ---------- */

static void update_backlight(PhrApp* app) {
    bool want = app->snap.state == PhrLinkActive;
    if(want && !app->backlight_forced) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
        app->backlight_forced = true;
    } else if(!want && app->backlight_forced) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        app->backlight_forced = false;
    }
}

void phr_app_poll(PhrApp* app) {
    if(app->start_pending) {
        app->start_pending = false;
        phr_app_apply_transport(app);
    }

    phr_link_snapshot(app->link, &app->snap);
    if(app->transport) app->transport_api->tick(app->transport);
    update_backlight(app);

    const char* status = app->transport ? app->transport_api->status(app->transport) :
                                          (app->transport_failed ? "Transport failed" : "Starting...");
    const char* device = app->transport ? app->transport_api->device_name(app->transport) : "";
    phr_dashboard_view_update(
        app->dashboard, &app->snap, app->transport_api->name, device, status);

    // Alert engine runs once per second.
    uint32_t now_s = phr_app_now_s();
    if(now_s == app->last_alert_eval_s) return;
    app->last_alert_eval_s = now_s;

    const PhrTelemetry* t = (app->snap.state == PhrLinkActive) ? &app->snap.telemetry : NULL;
    uint32_t silence_s = (app->snap.state == PhrLinkWaiting) ? 0 : app->snap.silence_ms / 1000;
    AlertEvent events[AlertRuleCount];
    size_t n = alerts_update(&app->alerts, now_s, t, silence_s, events);
    for(size_t i = 0; i < n; i++) {
        if(events[i].signal != AlertSignalOff) {
            phr_signal_play(
                app->notifications,
                (AlertSignal)events[i].signal,
                events[i].rule);
        }
        pending_push(app, &events[i]);
    }
}

/* ---------- view dispatcher callbacks ---------- */

static bool phr_custom_event_cb(void* ctx, uint32_t event) {
    PhrApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool phr_back_event_cb(void* ctx) {
    PhrApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void phr_tick_cb(void* ctx) {
    PhrApp* app = ctx;
    phr_app_poll(app);
    scene_manager_handle_tick_event(app->scene_manager);
}

static void phr_dashboard_ok_cb(void* ctx) {
    PhrApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, PhrEventOpenSettings);
}

static void phr_alert_ok_cb(void* ctx) {
    PhrApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, PhrEventAckAlert);
}

/* ---------- lifecycle ---------- */

static PhrApp* phr_app_alloc(void) {
    PhrApp* app = malloc(sizeof(PhrApp));
    memset(app, 0, sizeof(PhrApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    phr_settings_load(app->storage, &app->settings, &app->alerts);
    app->link = phr_link_alloc();
    app->transport_api = phr_transport_by_id(app->settings.transport);
    app->start_pending = true;

    app->dashboard = phr_dashboard_view_alloc();
    phr_dashboard_view_set_ok_callback(app->dashboard, phr_dashboard_ok_cb, app);
    app->alert_view = phr_alert_view_alloc();
    phr_alert_view_set_ok_callback(app->alert_view, phr_alert_ok_cb, app);
    app->list = variable_item_list_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&phr_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, phr_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, phr_back_event_cb);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, phr_tick_cb, 250);

    view_dispatcher_add_view(
        app->view_dispatcher, PhrViewDashboard, phr_dashboard_view_get_view(app->dashboard));
    view_dispatcher_add_view(
        app->view_dispatcher, PhrViewAlert, phr_alert_view_get_view(app->alert_view));
    view_dispatcher_add_view(
        app->view_dispatcher, PhrViewList, variable_item_list_get_view(app->list));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void phr_app_free(PhrApp* app) {
    if(app->transport) {
        app->transport_api->stop(app->transport);
        app->transport = NULL;
    }
    if(app->backlight_forced) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    }

    view_dispatcher_remove_view(app->view_dispatcher, PhrViewDashboard);
    view_dispatcher_remove_view(app->view_dispatcher, PhrViewAlert);
    view_dispatcher_remove_view(app->view_dispatcher, PhrViewList);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    phr_dashboard_view_free(app->dashboard);
    phr_alert_view_free(app->alert_view);
    variable_item_list_free(app->list);
    phr_link_free(app->link);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t pc_health_remote_app(void* p) {
    UNUSED(p);
    PhrApp* app = phr_app_alloc();
    scene_manager_next_scene(app->scene_manager, PhrSceneMain);
    view_dispatcher_run(app->view_dispatcher);
    phr_app_free(app);
    return 0;
}
