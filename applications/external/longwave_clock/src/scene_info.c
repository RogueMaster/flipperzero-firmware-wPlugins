#include "scene_info.h"
#include "app_state.h"
#include "scenes.h"

void lwc_info_scene_on_enter(void* context) {
    App* app = context;

    text_box_set_text(
        app->info_text,
        "Long wave time signal senders broadcast the exact time and date over LW radio. "
        "If you're in range, you can receive their signals with an inexpensive receiver tuned to their frequency. "
        "These come with a big enough ferrite core, and some low pass filtering. "
        "Keep the ferrite core away from the flipper (3-5cm) and away from E/M radiation (at least 1 metre away from a monitor). "
        "Best reception next to a window or outside with no obstructions. Nights are best. "
        "Keep the ferrite core level to ground and  perpendicular to the sender if possible.");

    view_dispatcher_switch_to_view(app->view_dispatcher, LWCInfoView);
}

bool lwc_info_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void lwc_info_scene_on_exit(void* context) {
    App* app = context;

    text_box_reset(app->about);
}
