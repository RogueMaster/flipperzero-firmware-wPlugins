#include "../chameleon_app_i.h"

typedef enum {
    TagReadEventAnimationDone = 0,
} TagReadEvent;

static void chameleon_scene_tag_read_animation_callback(void* context) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TagReadEventAnimationDone);
}

void chameleon_scene_tag_read_on_enter(void* context) {
    ChameleonApp* app = context;

    // Show transfer animation for tag reading
    chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationTransfer);
    chameleon_animation_view_set_callback(
        app->animation_view, chameleon_scene_tag_read_animation_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
    chameleon_animation_view_start(app->animation_view);
}

bool chameleon_scene_tag_read_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TagReadEventAnimationDone) {
            // Show success animation
            chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationSuccess);
            chameleon_animation_view_start(app->animation_view);

            // After success animation, return to previous scene
            furi_delay_ms(2000);
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void chameleon_scene_tag_read_on_exit(void* context) {
    ChameleonApp* app = context;
    chameleon_animation_view_stop(app->animation_view);
}
