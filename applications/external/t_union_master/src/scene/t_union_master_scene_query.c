#include "../t_union_master_i.h"

void tum_scene_query_cb(TUM_QueryWorkerEvent event, uint8_t value, void* ctx) {
    TUM_App* app = ctx;
    switch(event) {
    case TUM_QueryWorkerEventStart:
        query_progress_update_total(app->query_progress, value);
        break;
    case TUM_QueryWorkerEventProgress:
        query_progress_update_progress(app->query_progress, value);
        break;
    case TUM_QueryWorkerEventFinish:
        view_dispatcher_send_custom_event(app->view_dispatcher, TUM_CustomEventSwitchToBaseinfo);
        break;
    default:
        break;
    }
}

void tum_scene_query_on_enter(void* ctx) {
    TUM_App* app = ctx;
    notification_message(app->notifications, &sequence_success);
    notification_message(app->notifications, &tum_sequence_blink_start_green);

    scene_manager_set_scene_state(app->scene_manager, TUM_SceneTransactionList, 0);
    scene_manager_set_scene_state(app->scene_manager, TUM_SceneTravelList, 0);

    tum_query_worker_set_cb(app->query_worker, tum_scene_query_cb, app);
    tum_query_worker_start(app->query_worker);

    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewQureyProgress);
}

bool tum_scene_query_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = true;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TUM_CustomEventSwitchToBaseinfo) {
            notification_message(app->notifications, &tum_sequence_query_ok);
            scene_manager_next_scene(app->scene_manager, TUM_SceneBaseInfo);
        }
    }

    return consumed;
}

void tum_scene_query_on_exit(void* ctx) {
    TUM_App* app = ctx;
    notification_message(app->notifications, &sequence_blink_stop);
    query_progress_clear(app->query_progress);
}
