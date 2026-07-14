#include "../t_union_master_i.h"
#include "../utils/database.h"

void tum_scene_test_on_enter(void* ctx) {
    TUM_App* app = ctx;

    widget_add_string_element_utf8(app->widget, 0, 0, AlignLeft, AlignTop, "测试Test");
    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewWidget);
}

bool tum_scene_test_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed =
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TUM_SceneStart);
    }

    return consumed;
}

void tum_scene_test_on_exit(void* ctx) {
    TUM_App* app = ctx;
    widget_reset(app->widget);
}
