#include "wol_flipper.h"

#define TARGETS_INDEX_ADD 0xFF

static char targets_labels[WOL_MAX_TARGETS][WOL_NAME_LEN + 4];

static void wol_scene_targets_callback(void* context, uint32_t index) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void wol_scene_targets_on_enter(void* context) {
    WolApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->list_mode_wake ? "Wake device" : "Targets");

    for(size_t i = 0; i < app->config.target_count; i++) {
        snprintf(targets_labels[i], sizeof(targets_labels[i]), "%s", app->config.targets[i].name);
        submenu_add_item(app->submenu, targets_labels[i], i, wol_scene_targets_callback, app);
    }

    if(app->config.target_count < WOL_MAX_TARGETS) {
        submenu_add_item(
            app->submenu,
            app->config.target_count ? "Add target" : "Add first target",
            TARGETS_INDEX_ADD,
            wol_scene_targets_callback,
            app);
    }

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, WolSceneTargets));
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewSubmenu);
}

bool wol_scene_targets_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == TARGETS_INDEX_ADD) {
        scene_manager_set_scene_state(
            app->scene_manager, WolSceneTargets, app->config.target_count);
        app->edit_is_new = true;
        app->target_index = app->config.target_count;
        wol_target_default(&app->edit);
        scene_manager_next_scene(app->scene_manager, WolSceneTargetEdit);
        return true;
    }

    if(event.event < app->config.target_count) {
        scene_manager_set_scene_state(app->scene_manager, WolSceneTargets, event.event);
        app->target_index = event.event;

        if(app->list_mode_wake) {
            app->wifi_test_mode = false;
            scene_manager_next_scene(app->scene_manager, WolSceneSend);
        } else {
            app->edit_is_new = false;
            app->edit = app->config.targets[app->target_index];
            scene_manager_next_scene(app->scene_manager, WolSceneTargetEdit);
        }
        return true;
    }

    return false;
}

void wol_scene_targets_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}
