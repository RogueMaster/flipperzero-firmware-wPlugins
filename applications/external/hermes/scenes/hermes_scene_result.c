#include "../hermes_i.h"

static void hermes_scene_result_callback(void* context, uint32_t index) {
    HermesApp* app = context;

    if(index == UINT32_MAX) {
        /* Nothing was found, so the centre button offers a retry instead. */
        view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventResultRetry);
        return;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventResultPicked + index);
}

void hermes_scene_result_on_enter(void* context) {
    HermesApp* app = context;
    result_view_set_callback(app->result_view, hermes_scene_result_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewResult);
}

bool hermes_scene_result_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == HermesCustomEventResultRetry) {
        scene_manager_previous_scene(app->scene_manager); // back to detect, which restarts on enter
        return true;
    }

    if(event.event >= HermesCustomEventResultPicked) {
        const uint32_t index = event.event - HermesCustomEventResultPicked;
        ResultEntry entry;
        if(result_view_get_entry(app->result_view, index, &entry)) {
            app->link.baud = entry.baud;
            app->link.framing = entry.framing;
            app->link.verified = entry.verified;
            scene_manager_next_scene(app->scene_manager, HermesSceneConsole);
        }
        return true;
    }

    return false;
}

void hermes_scene_result_on_exit(void* context) {
    UNUSED(context);
}
