#include "../../fire_string.h"

// ─────────────────────────────────────────────
// Worker thread
// ─────────────────────────────────────────────

static int32_t usb_worker(void* context) {
    furi_check(context);
    FireString* app = context;
    FURI_LOG_I(TAG, "usb_worker %p starting", furi_thread_get_id(app->thread));

    // Save previous USB config or mark as locked
    if(furi_hal_usb_is_locked()) {
        app->hid->usb_if_prev = NULL;
    } else {
        app->hid->usb_if_prev = furi_hal_usb_get_config();
        furi_check(furi_hal_usb_set_config(NULL, NULL));
        furi_delay_ms(100); // Allow USB to settle after config change
    }

    // Resolve HID interface and initialize — failures are logged but not fatal
    app->hid->interface = BadUsbHidInterfaceUsb;
    app->hid->api = bad_usb_hid_get_interface(app->hid->interface);

    if(app->hid->api) {
        app->hid->hid_inst = app->hid->api->init(NULL);
        if(!app->hid->hid_inst) {
            FURI_LOG_W(TAG, "usb_worker: HID init returned NULL");
        }
    } else {
        FURI_LOG_W(TAG, "usb_worker: failed to get HID API");
    }

    FURI_LOG_I(TAG, "usb_worker %p ended", furi_thread_get_id(app->thread));
    return 0;
}

// ─────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────

void fire_string_scene_on_enter_loading_usb(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_loading_usb");
    furi_check(context);
    FireString* app = context;

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Loading);

    app->thread = furi_thread_alloc_ex("USBWorker", 1024, usb_worker, app);
    furi_thread_start(app->thread);
}

bool fire_string_scene_on_event_loading_usb(void* context, SceneManagerEvent event) {
    furi_check(context);
    FireString* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeTick:
        if(furi_thread_get_state(app->thread) == FuriThreadStateStopped) {
            scene_manager_next_scene(app->scene_manager, FireStringScene_USB);
            consumed = true;
        }
        break;
    default:
        break;
    }

    return consumed;
}

void fire_string_scene_on_exit_loading_usb(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_loading_usb");
    furi_check(context);
    FireString* app = context;

    if(app->thread) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }
}
