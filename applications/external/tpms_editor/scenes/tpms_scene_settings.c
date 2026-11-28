#include "../tpms_app_i.h"
#include "../tpms_save.h"
#include <stdio.h>

typedef enum {
    TPMSSettingsRadio = 0,
    TPMSSettingsAutosave,
    TPMSSettingsProtocolList,
    TPMSSettingsProtocolTest,
} TPMSSettingsIndex;

static void tpms_scene_settings_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void tpms_scene_settings_populate(TPMSApp* app) {
    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu,
        "Radio settings",
        TPMSSettingsRadio,
        tpms_scene_settings_callback,
        app);

    char autosave_label[32];
    snprintf(
        autosave_label,
        sizeof(autosave_label),
        "Autosave: %s",
        app->autosave_enabled ? "ON" : "OFF");
    submenu_add_item(
        app->submenu,
        autosave_label,
        TPMSSettingsAutosave,
        tpms_scene_settings_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Protocol list",
        TPMSSettingsProtocolList,
        tpms_scene_settings_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Protocol test",
        TPMSSettingsProtocolTest,
        tpms_scene_settings_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, TPMSSceneSettings));
}

void tpms_scene_settings_on_enter(void* context) {
    TPMSApp* app = context;
    tpms_scene_settings_populate(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewSubmenu);
}

bool tpms_scene_settings_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, TPMSSceneSettings, event.event);
    if(event.event == TPMSSettingsRadio) {
        scene_manager_next_scene(app->scene_manager, TPMSSceneReceiverConfig);
        return true;
    }
    if(event.event == TPMSSettingsAutosave) {
        const bool target = !app->autosave_enabled;
        if(tpms_autosave_set_enabled(app, target)) {
            scene_manager_set_scene_state(
                app->scene_manager, TPMSSceneSettings, TPMSSettingsAutosave);
            tpms_scene_settings_populate(app);
            notification_message(app->notifications, &sequence_blink_green_10);
        } else {
            notification_message(app->notifications, &sequence_blink_red_10);
        }
        return true;
    }
    if(event.event == TPMSSettingsProtocolList) {
        scene_manager_next_scene(app->scene_manager, TPMSSceneProtocolList);
        return true;
    }
    if(event.event == TPMSSettingsProtocolTest) {
        scene_manager_next_scene(app->scene_manager, TPMSSceneProtocolTest);
        return true;
    }
    return false;
}

void tpms_scene_settings_on_exit(void* context) {
    TPMSApp* app = context;
    submenu_reset(app->submenu);
}
