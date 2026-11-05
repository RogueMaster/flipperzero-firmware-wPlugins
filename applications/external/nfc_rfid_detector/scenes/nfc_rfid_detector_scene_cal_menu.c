#include "../nfc_rfid_detector_app_i.h"

typedef enum {
    CalMenuIndexCal,
    CalMenuIndexHelp,
} CalMenuIndex;

static void nfc_rfid_detector_scene_cal_menu_callback(void* context, uint32_t index) {
    NfcRfidDetectorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_rfid_detector_scene_cal_menu_on_enter(void* context) {
    NfcRfidDetectorApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_set_header(submenu, "Config");
    submenu_add_item(
        submenu,
        "Set calibration manually",
        CalMenuIndexCal,
        nfc_rfid_detector_scene_cal_menu_callback,
        app);
    submenu_add_item(
        submenu,
        "Calibrate with a computer",
        CalMenuIndexHelp,
        nfc_rfid_detector_scene_cal_menu_callback,
        app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, NfcRfidDetectorSceneCalMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcRfidDetectorViewSubmenu);
}

bool nfc_rfid_detector_scene_cal_menu_on_event(void* context, SceneManagerEvent event) {
    NfcRfidDetectorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcRfidDetectorCustomEventCalSessionStart) {
            scene_manager_next_scene(app->scene_manager, NfcRfidDetectorSceneCalSession);
            consumed = true;
        } else if(event.event == CalMenuIndexCal) {
            scene_manager_set_scene_state(
                app->scene_manager, NfcRfidDetectorSceneCalMenu, CalMenuIndexCal);
            scene_manager_next_scene(app->scene_manager, NfcRfidDetectorSceneCal);
            consumed = true;
        } else if(event.event == CalMenuIndexHelp) {
            scene_manager_set_scene_state(
                app->scene_manager, NfcRfidDetectorSceneCalMenu, CalMenuIndexHelp);
            scene_manager_next_scene(app->scene_manager, NfcRfidDetectorSceneCalHelp);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_rfid_detector_scene_cal_menu_on_exit(void* context) {
    NfcRfidDetectorApp* app = context;
    submenu_reset(app->submenu);
}
