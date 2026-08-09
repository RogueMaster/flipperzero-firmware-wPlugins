#include "../hermes_i.h"

/* Shown when a console session ends, in place of a bare pop back. The stats
 * were captured in the console's Back handler while the link was still open. */

void hermes_scene_summary_on_enter(void* context) {
    HermesApp* app = context;
    summary_view_set(app->summary_view, &app->last_session);
    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewSummary);
}

bool hermes_scene_summary_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        /* Skip the console that still sits beneath us in the stack - re-entering
         * it would reopen the port. Land on the main menu instead, deleting the
         * console and this summary on the way. */
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, HermesSceneStart);
        return true;
    }

    return false;
}

void hermes_scene_summary_on_exit(void* context) {
    UNUSED(context);
}
