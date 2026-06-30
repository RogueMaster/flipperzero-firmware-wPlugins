#include "../flip_tdi_app_i.h"
#include "../views/flip_tdi_view_main.h"

void flip_tdi_scene_main_callback(FlipTDICustomEvent event, void* context) {
    furi_assert(context);
    FlipTDIApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void flip_tdi_scene_main_update(void* context) {
    furi_assert(context);
    FlipTDIApp* app = context;
    UNUSED(app);
}

void flip_tdi_scene_main_on_enter(void* context) {
    furi_assert(context);
    FlipTDIApp* app = context;

    flip_tdi_view_main_set_callback(
        app->flip_tdi_view_main_instance, flip_tdi_scene_main_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipTDIViewMain);
}

bool flip_tdi_scene_main_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipTDIApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FlipTDICustomEventMainMore:
            scene_manager_next_scene(app->scene_manager, FlipTDISceneMenu);
            break;
        default:
            break;
        }

    } else if(event.type == SceneManagerEventTypeTick) {
        flip_tdi_scene_main_update(app);
    }

    return consumed;
}

void flip_tdi_scene_main_on_exit(void* context) {
    furi_assert(context);
    FlipTDIApp* app = context;
    UNUSED(app);
}
