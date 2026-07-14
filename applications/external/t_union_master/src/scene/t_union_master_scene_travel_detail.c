#include "../t_union_master_i.h"

void tum_scene_travel_detail_cb(TUM_CustomEvent event, void* ctx) {
    TUM_App* app = ctx;
    UNUSED(app);
    UNUSED(event);
}

void tum_scene_travel_detail_on_enter(void* ctx) {
    TUM_App* app = ctx;

    travel_detail_set_callback(app->travel_detail, tum_scene_travel_detail_cb, app);
    travel_detail_set_index(
        app->travel_detail,
        scene_manager_get_scene_state(app->scene_manager, TUM_SceneTravelList));
    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewTravelDetail);
}

bool tum_scene_travel_detail_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    UNUSED(app);
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_set_scene_state(
            app->scene_manager, TUM_SceneTravelList, travel_detail_get_index(app->travel_detail));
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TUM_SceneTravelList);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        default:
            break;
        }
    }

    return consumed;
}

void tum_scene_travel_detail_on_exit(void* ctx) {
    TUM_App* app = ctx;
    travel_detail_reset(app->travel_detail);
}
