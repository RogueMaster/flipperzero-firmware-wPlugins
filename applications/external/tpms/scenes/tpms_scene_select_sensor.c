#include "../tpms_app_i.h"

typedef enum {
    SubmenuIndexAllProtocols,
    SubmenuIndexSchraderGG4,
    SubmenuIndexSchraderSMD3MA4,
    SubmenuIndexSchraderEG53MA4,
    SubmenuIndexAbarth124,
} SubmenuIndex;

void tpms_scene_select_sensor_submenu_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tpms_scene_select_sensor_on_enter(void* context) {
    TPMSApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu,
        "All Sensors",
        SubmenuIndexAllProtocols,
        tpms_scene_select_sensor_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Schrader GG4",
        SubmenuIndexSchraderGG4,
        tpms_scene_select_sensor_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Schrader SMD3MA4 (Subaru)",
        SubmenuIndexSchraderSMD3MA4,
        tpms_scene_select_sensor_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Schrader EG53MA4 (GM/Chevy)",
        SubmenuIndexSchraderEG53MA4,
        tpms_scene_select_sensor_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Abarth 124 / Fiat / MX-5",
        SubmenuIndexAbarth124,
        tpms_scene_select_sensor_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, TPMSSceneSelectSensor));

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewSubmenu);
}

bool tpms_scene_select_sensor_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexAllProtocols:
            app->protocol_filter = TPMSProtocolFilterAll;
            break;
        case SubmenuIndexSchraderGG4:
            app->protocol_filter = TPMSProtocolFilterSchraderGG4;
            break;
        case SubmenuIndexSchraderSMD3MA4:
            app->protocol_filter = TPMSProtocolFilterSchraderSMD3MA4;
            break;
        case SubmenuIndexSchraderEG53MA4:
            app->protocol_filter = TPMSProtocolFilterSchraderEG53MA4;
            break;
        case SubmenuIndexAbarth124:
            app->protocol_filter = TPMSProtocolFilterAbarth124;
            break;
        default:
            break;
        }
        scene_manager_set_scene_state(app->scene_manager, TPMSSceneSelectSensor, event.event);
        scene_manager_next_scene(app->scene_manager, TPMSSceneReceiver);
        consumed = true;
    }

    return consumed;
}

void tpms_scene_select_sensor_on_exit(void* context) {
    TPMSApp* app = context;
    submenu_reset(app->submenu);
}
