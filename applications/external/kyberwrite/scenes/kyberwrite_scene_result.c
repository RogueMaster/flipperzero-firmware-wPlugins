#include "kyberwrite_scene.h"

static void result_popup_callback(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    scene_manager_handle_custom_event(app->scene_manager, KyberEventResultBack);
}

void kyberwrite_scene_result_on_enter(void* context) {
    furi_assert(context);
    KyberApp* app = context;

    popup_reset(app->result_popup);
    popup_set_callback(app->result_popup, result_popup_callback);
    popup_set_context(app->result_popup, app);
    popup_enable_timeout(app->result_popup);
    popup_set_timeout(app->result_popup, 3000);

    if(app->write_success) {
        popup_set_header(app->result_popup, "Crystal Ready!", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->result_popup,
            KYBER_CRYSTALS[app->crystal_index].name,
            64, 32, AlignCenter, AlignCenter);
        // notification_message(app->notifications, &sequence_success);
    } else {
        popup_set_header(app->result_popup, "Write Failed", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->result_popup,
            "Hold closer & retry",
            64, 32, AlignCenter, AlignCenter);
        notification_message(app->notifications, &sequence_error);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, KyberViewResult);
}

bool kyberwrite_scene_result_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    KyberApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == KyberEventResultBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, KyberSceneMenu);
        consumed = true;
    }
    return consumed;
}

void kyberwrite_scene_result_on_exit(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    popup_reset(app->result_popup);
}
