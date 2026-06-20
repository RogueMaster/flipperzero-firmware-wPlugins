#include "../dallas_tester_i.h"

// Runs on the worker thread: publish the reading and blink green once when a key first appears.
// dallas_test_view_set_result and notification_message are both thread-safe.
static void dallas_tester_test_result_cb(const DallasTestResult* result, void* context) {
    DallasTester* app = context;
    dallas_test_view_set_result(app->test_view, result);

    const bool present = result->samples_ok > 0;
    if(present && !app->last_present) {
        notification_message(app->notifications, &sequence_blink_green_10);
    }
    app->last_present = present;
}

void dallas_tester_scene_test_on_enter(void* context) {
    DallasTester* app = context;

    dallas_test_view_reset(app->test_view);
    app->last_present = false;
    view_dispatcher_switch_to_view(app->view_dispatcher, DallasTesterViewTest);

    dallas_test_worker_start(app->worker, dallas_tester_test_result_cb, app);
}

bool dallas_tester_scene_test_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // Back is handled by the view dispatcher navigation callback -> returns to the menu.
    return false;
}

void dallas_tester_scene_test_on_exit(void* context) {
    DallasTester* app = context;
    // Stops and joins the worker thread (releases the 1-Wire pin) before we leave the scene.
    dallas_test_worker_stop(app->worker);
    notification_message(app->notifications, &sequence_blink_stop);
}
