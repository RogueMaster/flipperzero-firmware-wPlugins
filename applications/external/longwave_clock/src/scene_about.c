#include "scene_about.h"
#include "app_state.h"
#include "scenes.h"
#include "flipper.h"

void lwc_about_scene_on_enter(void* context) {
    App* app = context;

    text_box_set_text(
        app->about,
        "Listen to longwave senders transmitting atomic precision time signals, "
        "either using GPIO and special modules or using demo mode to simulate reception.\n\n"
        "Made by Andrea Micheloni\n@m7i-org | m7i.org\n\n"
        "...because it's always time for learning.");

    view_dispatcher_switch_to_view(app->view_dispatcher, LWCAboutView);
}

bool lwc_about_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void lwc_about_scene_on_exit(void* context) {
    App* app = context;

    text_box_reset(app->about);
}
