#include "../t_union_master_i.h"

void tum_scene_base_info_cb(TUM_CustomEvent event, void* ctx) {
    TUM_App* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void tum_scene_base_info_on_enter(void* ctx) {
    TUM_App* app = ctx;
    notification_message_block(app->notifications, &sequence_set_green_255);

    card_info_set_callback(app->card_info, tum_scene_base_info_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewCardInfo);
}

bool tum_scene_base_info_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        t_union_msg_reset(app->card_message);
        t_union_msgext_reset(app->card_message_ext);
        consumed =
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TUM_SceneStart);
    } else if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_next_scene(app->scene_manager, TUM_SceneTransactionList);
    }

    return consumed;
}

void tum_scene_base_info_on_exit(void* ctx) {
    TUM_App* app = ctx;
    notification_message_block(app->notifications, &sequence_reset_green);
}
