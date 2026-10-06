#include "../openprinttag_i.h"

// The read result: the summary card and two lists with the details, drawn by the tag view

void openprinttag_scene_display_on_enter(void* context) {
    OpenPrintTag* app = context;

    openprinttag_tag_view_set_data(
        app->tag_view, &app->tag_data, app->has_tag_uid ? app->tag_uid : NULL);

    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewTagView);
}

bool openprinttag_scene_display_on_event(void* context, SceneManagerEvent event) {
    OpenPrintTag* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        // The scene behind this one is the success popup, which would time out and open
        // this screen again, so go straight back to the main menu
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, OpenPrintTagSceneStart);
        return true;
    }

    return false;
}

void openprinttag_scene_display_on_exit(void* context) {
    UNUSED(context);
}
