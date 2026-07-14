#include "../t_union_master_i.h"

void tum_scene_transaction_list_cb(TUM_CustomEvent event, void* ctx) {
    TUM_App* app = ctx;
    switch(event) {
    case TUM_CustomEventSwitchToTransactionDetail:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, TUM_CustomEventSwitchToTransactionDetail);
        break;
    case TUM_CustomEventSwitchToBaseinfo:
        view_dispatcher_send_custom_event(app->view_dispatcher, TUM_CustomEventSwitchToBaseinfo);
        break;
    case TUM_CustomEventSwitchToTravel:
        view_dispatcher_send_custom_event(app->view_dispatcher, TUM_CustomEventSwitchToTravel);
    default:
    }
}

void tum_scene_transaction_list_save_status(TUM_App* app) {
    scene_manager_set_scene_state(
        app->scene_manager,
        TUM_SceneTransactionList,
        transaction_list_get_index(app->transaction_list));
}

void tum_scene_transaction_list_on_enter(void* ctx) {
    TUM_App* app = ctx;

    notification_message(app->notifications, &sequence_solid_yellow);

    transaction_list_set_callback(app->transaction_list, tum_scene_transaction_list_cb, app);
    transaction_list_set_index(
        app->transaction_list,
        scene_manager_get_scene_state(app->scene_manager, TUM_SceneTransactionList));
    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewTransactionList);
}

bool tum_scene_transaction_list_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
        tum_scene_transaction_list_save_status(app);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TUM_SceneBaseInfo);
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TUM_CustomEventSwitchToTransactionDetail:
            tum_scene_transaction_list_save_status(app);
            scene_manager_next_scene(app->scene_manager, TUM_SceneTransactionDetail);
            break;
        case TUM_CustomEventSwitchToBaseinfo:
            consumed = true;
            tum_scene_transaction_list_save_status(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, TUM_SceneBaseInfo);
            break;
        case TUM_CustomEventSwitchToTravel:
            consumed = true;
            tum_scene_transaction_list_save_status(app);
            scene_manager_next_scene(app->scene_manager, TUM_SceneTravelList);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void tum_scene_transaction_list_on_exit(void* ctx) {
    TUM_App* app = ctx;
    transaction_list_reset(app->transaction_list);
}
