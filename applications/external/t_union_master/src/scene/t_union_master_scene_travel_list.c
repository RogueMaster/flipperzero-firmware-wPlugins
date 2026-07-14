#include "../t_union_master_i.h"

void tum_scene_travel_list_cb(TUM_CustomEvent event, void* ctx) {
    TUM_App* app = ctx;
    switch(event) {
    case TUM_CustomEventSwitchToTransaction:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, TUM_CustomEventSwitchToTransaction);
        break;
    case TUM_CustomEventSwitchToTravelDetail:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, TUM_CustomEventSwitchToTravelDetail);
        break;
    default:
    }
}

void tum_scene_travel_list_save_status(TUM_App* app) {
    scene_manager_set_scene_state(
        app->scene_manager, TUM_SceneTravelList, travel_list_get_index(app->travel_list));
}

void tum_scene_travel_list_on_enter(void* ctx) {
    TUM_App* app = ctx;

    notification_message(app->notifications, &sequence_solid_yellow);

    travel_list_set_callback(app->travel_list, tum_scene_travel_list_cb, app);
    travel_list_set_index(
        app->travel_list, scene_manager_get_scene_state(app->scene_manager, TUM_SceneTravelList));
    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewTravelList);
}

bool tum_scene_travel_list_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
        tum_scene_travel_list_save_status(app);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TUM_SceneBaseInfo);
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TUM_CustomEventSwitchToTravelDetail:
            tum_scene_travel_list_save_status(app);
            scene_manager_next_scene(app->scene_manager, TUM_SceneTravelDetail);
            break;
        case TUM_CustomEventSwitchToTransaction:
            consumed = true;
            tum_scene_travel_list_save_status(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, TUM_SceneTransactionList);
            break;
        default:
        }
    }

    return consumed;
}

void tum_scene_travel_list_on_exit(void* ctx) {
    TUM_App* app = ctx;
    travel_list_reset(app->travel_list);
}
