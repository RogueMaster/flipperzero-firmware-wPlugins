#include "../flipcheckers.h"
#include "../helpers/flipcheckers_file.h"
#include "../helpers/flipcheckers_custom_event.h"
#include "../views/flipcheckers_scene_1.h"

void flipcheckers_scene_1_callback(FlipCheckersCustomEvent event, void* context) {
    furi_assert(context);
    FlipCheckers* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void flipcheckers_scene_scene_1_on_enter(void* context) {
    furi_assert(context);
    FlipCheckers* app = context;

    flipcheckers_scene_1_set_callback(app->flipcheckers_scene_1, flipcheckers_scene_1_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCheckersViewIdScene1);
}

bool flipcheckers_scene_scene_1_on_event(void* context, SceneManagerEvent event) {
    FlipCheckers* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FlipCheckersCustomEventScene1Left:
        case FlipCheckersCustomEventScene1Right:
            break;
        case FlipCheckersCustomEventScene1Up:
        case FlipCheckersCustomEventScene1Down:
            break;
        case FlipCheckersCustomEventScene1Back:
            notification_message(app->notification, &sequence_reset_red);
            notification_message(app->notification, &sequence_reset_green);
            notification_message(app->notification, &sequence_reset_blue);
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, FlipCheckersSceneMenu)) {
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;
        }
    }

    return consumed;
}

void flipcheckers_scene_scene_1_on_exit(void* context) {
    FlipCheckers* app = context;

    if(app->import_game == 1 && strlen(app->import_game_text) > 0) {
        flipcheckers_save_file(app->import_game_text, FlipCheckersFileBoard, NULL, false, true);
    }
}
