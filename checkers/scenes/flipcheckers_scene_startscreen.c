#include "../flipcheckers.h"
#include "../helpers/flipcheckers_file.h"
#include "../helpers/flipcheckers_custom_event.h"
#include "../views/flipcheckers_startscreen.h"

void flipcheckers_scene_startscreen_callback(FlipCheckersCustomEvent event, void* context) {
    furi_assert(context);
    FlipCheckers* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void flipcheckers_scene_startscreen_on_enter(void* context) {
    furi_assert(context);
    FlipCheckers* app = context;

    if(flipcheckers_has_file(FlipCheckersFileBoard, NULL, false)) {
        if(flipcheckers_load_file(app->import_game_text, FlipCheckersFileBoard, NULL)) {
            app->import_game = 1;
        }
    }

    flipcheckers_startscreen_set_callback(
        app->flipcheckers_startscreen, flipcheckers_scene_startscreen_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCheckersViewIdStartscreen);
}

bool flipcheckers_scene_startscreen_on_event(void* context, SceneManagerEvent event) {
    FlipCheckers* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FlipCheckersCustomEventStartscreenLeft:
        case FlipCheckersCustomEventStartscreenRight:
            break;
        case FlipCheckersCustomEventStartscreenUp:
        case FlipCheckersCustomEventStartscreenDown:
            break;
        case FlipCheckersCustomEventStartscreenOk:
            scene_manager_next_scene(app->scene_manager, FlipCheckersSceneMenu);
            consumed = true;
            break;
        case FlipCheckersCustomEventStartscreenBack:
            notification_message(app->notification, &sequence_reset_red);
            notification_message(app->notification, &sequence_reset_green);
            notification_message(app->notification, &sequence_reset_blue);
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, FlipCheckersSceneStartscreen)) {
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;
        }
    }

    return consumed;
}

void flipcheckers_scene_startscreen_on_exit(void* context) {
    UNUSED(context);
}
