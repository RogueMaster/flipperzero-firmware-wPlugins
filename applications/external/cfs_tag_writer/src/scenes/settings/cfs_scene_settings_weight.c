#include "src/core/cfs_app_i.h"

static void cfs_settings_weight_number_callback(void* context, int32_t number) {
    CfsApp* app = context;
    app->number_value = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonSave);
}

static void cfs_settings_weight_show_list(CfsApp* app) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Weight -> Length (m)");
    char label[32];
    for(uint32_t i = 0; i < CFS_WEIGHT_COUNT; i++) {
        snprintf(label, sizeof(label), "%s: %u m", cfs_weight_label(i), app->settings.weight_len[i]);
        submenu_add_item(submenu, label, i, cfs_submenu_send_index, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

void cfs_scene_settings_weight_on_enter(void* context) {
    CfsApp* app = context;
    cfs_settings_weight_show_list(app);
}

bool cfs_scene_settings_weight_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event < CFS_WEIGHT_COUNT) {
        // Selected a weight to edit its length.
        scene_manager_set_scene_state(app->scene_manager, CfsSceneSettingsWeight, event.event);
        char header[24];
        snprintf(header, sizeof(header), "%s length (m)", cfs_weight_label(event.event));
        number_input_set_header_text(app->number_input, header);
        number_input_set_result_callback(
            app->number_input,
            cfs_settings_weight_number_callback,
            app,
            app->settings.weight_len[event.event],
            1,
            9999);
        view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewNumberInput);
        return true;
    }

    if(event.event == CfsCustomEventButtonSave) {
        uint32_t idx = scene_manager_get_scene_state(app->scene_manager, CfsSceneSettingsWeight);
        if(idx < CFS_WEIGHT_COUNT) {
            app->settings.weight_len[idx] = (uint16_t)app->number_value;
            cfs_settings_save(app->storage, &app->settings);
        }
        cfs_settings_weight_show_list(app); // repopulate + switch back to the list
        return true;
    }
    return false;
}

void cfs_scene_settings_weight_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}
