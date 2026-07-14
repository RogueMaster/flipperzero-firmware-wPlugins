#include "../breach_map_i.h"

void breach_map_scene_graph_on_enter(void* context) {
    BreachMapApp* app = context;
    graph_view_set_session(app->graph_view, app->session);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewGraph);
}

bool breach_map_scene_graph_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void breach_map_scene_graph_on_exit(void* context) {
    UNUSED(context);
}
