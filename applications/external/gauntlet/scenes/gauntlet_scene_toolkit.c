#include "../gauntlet_i.h"

static void gauntlet_toolkit_cb(void* context, ToolkitEvent event) {
    UNUSED(event);
    GauntletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GauntletEventChallengeTools);
}

void gauntlet_scene_toolkit_on_enter(void* context) {
    GauntletApp* app = context;
    toolkit_view_set_callback(app->toolkit_view, gauntlet_toolkit_cb, app);
    toolkit_view_set_source(app->toolkit_view, app->tool_src);
    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewToolkit);
}

bool gauntlet_scene_toolkit_on_event(void* context, SceneManagerEvent event) {
    GauntletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == GauntletEventChallengeTools) {
        /* OK = use the decoded output as the flag (only meaningful in a challenge) */
        if(app->tool_from_challenge) {
            toolkit_view_get_output(app->toolkit_view, app->text_buf, sizeof(app->text_buf));
            scene_manager_next_scene(app->scene_manager, GauntletSceneSolve);
        }
        consumed = true;
    }
    return consumed;
}

void gauntlet_scene_toolkit_on_exit(void* context) {
    UNUSED(context);
}
