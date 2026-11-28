#include "../tpms_app_i.h"

typedef enum {
    SubmenuIndexTPMSReceiver = 0,
    SubmenuIndexTPMSSaved,
    SubmenuIndexTPMSSettings,
    SubmenuIndexTPMSSimulation,
    SubmenuIndexTPMSAbout,
} SubmenuIndex;

static void tpms_scene_start_submenu_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tpms_scene_start_on_enter(void* context) {
    TPMSApp* app = context;

    submenu_add_item(
        app->submenu,
        "Receiver",
        SubmenuIndexTPMSReceiver,
        tpms_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Saved",
        SubmenuIndexTPMSSaved,
        tpms_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Settings",
        SubmenuIndexTPMSSettings,
        tpms_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Simulation",
        SubmenuIndexTPMSSimulation,
        tpms_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Info",
        SubmenuIndexTPMSAbout,
        tpms_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, TPMSSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewSubmenu);
}

bool tpms_scene_start_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TPMSSceneStart, event.event);
        if(event.event == SubmenuIndexTPMSReceiver) {
            app->editor_profile_loaded = false;
            app->relearn_auto_rx = false;
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiver);
            consumed = true;
        } else if(event.event == SubmenuIndexTPMSSaved) {
            app->editor_profile_loaded = false;
            scene_manager_next_scene(app->scene_manager, TPMSSceneSaved);
            consumed = true;
        } else if(event.event == SubmenuIndexTPMSSettings) {
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiverConfig);
            consumed = true;
        } else if(event.event == SubmenuIndexTPMSSimulation) {
            scene_manager_next_scene(app->scene_manager, TPMSSceneSimulation);
            consumed = true;
        } else if(event.event == SubmenuIndexTPMSAbout) {
            scene_manager_next_scene(app->scene_manager, TPMSSceneAbout);
            consumed = true;
        }
    }

    return consumed;
}

void tpms_scene_start_on_exit(void* context) {
    TPMSApp* app = context;
    submenu_reset(app->submenu);
}
