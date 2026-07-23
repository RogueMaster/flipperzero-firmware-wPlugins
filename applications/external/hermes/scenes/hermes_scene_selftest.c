#include "../hermes_i.h"

static void hermes_scene_selftest_progress(void* context) {
    HermesApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        selftest_is_running(app->selftest) ? HermesCustomEventSelfTestTick :
                                             HermesCustomEventSelfTestDone);
}

/** OK on the view: run it, or run it again. */
static void hermes_scene_selftest_start(void* context) {
    HermesApp* app = context;

    selftest_view_set_running(app->selftest_view, true, 0);
    selftest_set_callback(app->selftest, hermes_scene_selftest_progress, app);

    if(!selftest_start(app->selftest, app->port)) {
        selftest_view_set_running(app->selftest_view, false, 0);
        hermes_notify_found(app, false);
    }
}

void hermes_scene_selftest_on_enter(void* context) {
    HermesApp* app = context;

    selftest_view_reset(app->selftest_view);
    if(app->port == HermesPortLpuart) {
        selftest_view_set_pins(app->selftest_view, 16, 15);
    } else {
        selftest_view_set_pins(app->selftest_view, 14, 13);
    }
    selftest_view_set_callback(app->selftest_view, hermes_scene_selftest_start, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewSelfTest);
}

bool hermes_scene_selftest_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case HermesCustomEventSelfTestTick:
        selftest_view_set_running(app->selftest_view, true, selftest_progress(app->selftest));
        return true;

    case HermesCustomEventSelfTestDone: {
        const SelfTestResult* r = selftest_result(app->selftest);
        selftest_view_set_result(app->selftest_view, r);
        hermes_notify_found(app, r->verdict == SelfTestPassed);
        return true;
    }

    default:
        return false;
    }
}

void hermes_scene_selftest_on_exit(void* context) {
    HermesApp* app = context;
    selftest_stop(app->selftest); // releases the port
}
