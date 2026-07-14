#include "../chameleon_app_i.h"

typedef enum {
    BleScanEventAnimationDone = 0,
} BleScanEvent;

static void chameleon_scene_ble_scan_animation_callback(void* context) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BleScanEventAnimationDone);
}

void chameleon_scene_ble_scan_on_enter(void* context) {
    ChameleonApp* app = context;

    // Show scanning animation
    chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationScan);
    chameleon_animation_view_set_callback(
        app->animation_view, chameleon_scene_ble_scan_animation_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
    chameleon_animation_view_start(app->animation_view);

    // Initialize BLE and start scan in background
    chameleon_app_connect_ble(app);
}

bool chameleon_scene_ble_scan_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BleScanEventAnimationDone) {
            // Animation finished, check scan results
            size_t device_count = ble_handler_get_device_count(app->ble_handler);

            if(device_count > 0) {
                // Found devices, go to connection scene
                scene_manager_next_scene(app->scene_manager, ChameleonSceneBleConnect);
            } else {
                // No devices found, show error animation then return
                chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationError);
                chameleon_animation_view_start(app->animation_view);

                // After error animation, return to main menu
                furi_delay_ms(2000);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, ChameleonSceneMainMenu);
            }
            consumed = true;
        }
    }

    return consumed;
}

void chameleon_scene_ble_scan_on_exit(void* context) {
    ChameleonApp* app = context;
    chameleon_animation_view_stop(app->animation_view);
}
