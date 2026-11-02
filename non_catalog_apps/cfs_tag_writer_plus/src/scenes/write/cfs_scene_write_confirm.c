#include "src/core/cfs_app_i.h"
#include "src/core/cfs_ui.h"

void cfs_scene_write_confirm_on_enter(void* context) {
    CfsApp* app = context;
    CfsCardModel card;
    cfs_ui_fill_card(&app->pending, app->pending_weight_index, &card);
    card.button_left = "Save";
    card.button_center = "Write";
    card.button_right = "x2";
    cfs_screen_show_card(
        app->screen,
        &card,
        CfsCustomEventButtonSave,
        CfsCustomEventButtonWrite,
        CfsCustomEventButtonWriteDual);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewScreen);
}

bool cfs_scene_write_confirm_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case CfsCustomEventButtonWrite:
    case CfsCustomEventButtonWriteDual:
        app->dual_tag = (event.event == CfsCustomEventButtonWriteDual);
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteProgress);
        return true;
    case CfsCustomEventButtonSave:
        app->save_from_write = true;
        scene_manager_next_scene(app->scene_manager, CfsSceneSaveKind);
        return true;
    default:
        return false;
    }
}

void cfs_scene_write_confirm_on_exit(void* context) {
    CfsApp* app = context;
    cfs_screen_reset(app->screen);
}
