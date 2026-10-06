#include "../phr_app.h"
#include "../advice.h"
#include "../signals.h"
#include "phr_scene.h"

void phr_scene_alert_on_enter(void* context) {
    PhrApp* app = context;
    if(!phr_app_pop_pending(app, &app->shown)) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    char lines[ADVICE_MAX_LINES][ADVICE_LINE_LEN];
    const PhrTelemetry* t = app->snap.have_any ? &app->snap.telemetry : NULL;
    size_t n = advice_get(app->shown.rule, t, lines);
    phr_alert_view_set(
        app->alert_view,
        app->shown.rule,
        app->shown.value,
        app->shown.threshold,
        phr_signal_is_critical(app->shown.rule),
        lines,
        n);
    view_dispatcher_switch_to_view(app->view_dispatcher, PhrViewAlert);
}

bool phr_scene_alert_on_event(void* context, SceneManagerEvent event) {
    PhrApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == PhrEventAckAlert) {
        // OK: acknowledge = snooze this alert for 10 minutes
        alerts_acknowledge(&app->alerts, app->shown.rule, phr_app_now_s());
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false; // Back: dismiss without snoozing
}

void phr_scene_alert_on_exit(void* context) {
    UNUSED(context);
}
