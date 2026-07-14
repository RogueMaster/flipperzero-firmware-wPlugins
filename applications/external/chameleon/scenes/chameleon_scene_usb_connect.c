#include "../chameleon_app_i.h"

typedef enum {
    UsbConnectEventAnimationDone = 0,
} UsbConnectEvent;

static void chameleon_scene_usb_connect_animation_callback(void* context) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, UsbConnectEventAnimationDone);
}

void chameleon_scene_usb_connect_on_enter(void* context) {
    ChameleonApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Connecting...", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "USB Connection", 64, 32, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);

    furi_delay_ms(1000);

    // Attempt USB connection
    if(chameleon_app_connect_usb(app)) {
        app->connection_status = ChameleonStatusConnected;

        // Show the handshake animation for successful connection!
        chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationHandshake);
        chameleon_animation_view_set_callback(
            app->animation_view, chameleon_scene_usb_connect_animation_callback, app);

        view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
        chameleon_animation_view_start(app->animation_view);
    } else {
        app->connection_status = ChameleonStatusError;

        // Show error animation
        chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationError);
        chameleon_animation_view_set_callback(
            app->animation_view, chameleon_scene_usb_connect_animation_callback, app);

        view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
        chameleon_animation_view_start(app->animation_view);
    }
}

bool chameleon_scene_usb_connect_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == UsbConnectEventAnimationDone) {
            // Animation finished, go back to main menu
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, ChameleonSceneMainMenu);
            consumed = true;
        }
    }

    return consumed;
}

void chameleon_scene_usb_connect_on_exit(void* context) {
    ChameleonApp* app = context;
    popup_reset(app->popup);
    chameleon_animation_view_stop(app->animation_view);
}
