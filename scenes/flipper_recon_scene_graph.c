#include "../flipper_recon_i.h"

void flipper_recon_scene_graph_on_enter(void* context) {
    FlipperReconApp* app = context;
    graph_view_set_session(app->graph_view, app->session);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewGraph);
}

bool flipper_recon_scene_graph_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flipper_recon_scene_graph_on_exit(void* context) {
    UNUSED(context);
}
