#include "../disn3y_toolbox_app.h"

static void disn3y_toolbox_app_scene_mb_presets_callback(void* context, uint32_t index) {
    Disn3yToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void disn3y_toolbox_app_scene_mb_presets_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Presets");

    for(size_t i = 0; i < magicband_preset_count; i++) {
        submenu_add_item(
            submenu,
            magicband_presets[i].name,
            (uint32_t)i,
            disn3y_toolbox_app_scene_mb_presets_callback,
            app);
    }

    uint32_t selected =
        scene_manager_get_scene_state(app->scene_manager, Disn3yToolboxAppSceneMbPresets);
    submenu_set_selected_item(submenu, selected);

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewSubmenu);
}

bool disn3y_toolbox_app_scene_mb_presets_on_event(void* context, SceneManagerEvent event) {
    Disn3yToolboxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < magicband_preset_count) {
            scene_manager_set_scene_state(
                app->scene_manager, Disn3yToolboxAppSceneMbPresets, event.event);
            const MagicBandPreset* preset = &magicband_presets[event.event];
            app->beacon_data_len = magicband_preset_to_beacon_data(preset->hex, app->beacon_data);
            app->preset_mode = true;
            app->preset_name = preset->name;
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneMbBroadcast);
            consumed = true;
        }
    }

    return consumed;
}

void disn3y_toolbox_app_scene_mb_presets_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    submenu_reset(app->submenu);
}
