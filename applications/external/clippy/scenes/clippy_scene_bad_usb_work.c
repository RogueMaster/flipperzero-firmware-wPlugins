#include "../helpers/ducky_script.h"
#include "../clippy_app_i.h"
#include "../views/clippy_bad_usb_view.h"
#include <furi_hal.h>
#include "toolbox/path.h"

void clippy_scene_bad_usb_work_button_callback(InputKey key, void* context) {
    furi_assert(context);
    ClippyApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, key);
}

bool clippy_scene_bad_usb_work_on_event(void* context, SceneManagerEvent event) {
    ClippyApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == InputKeyLeft) {
            if(clippy_bad_usb_view_is_idle_state(app->bad_usb_view)) {
                scene_manager_next_scene(app->scene_manager, ClippySceneConfig);
            }
            consumed = true;
        } else if(event.event == InputKeyOk) {
            clippy_bad_usb_payload_start_stop(app->bad_usb_payload);
            consumed = true;
        } else if(event.event == InputKeyRight) {
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        clippy_bad_usb_view_set_state(
            app->bad_usb_view, clippy_bad_usb_payload_get_state(app->bad_usb_payload));
    }
    return consumed;
}

void clippy_scene_bad_usb_work_on_enter(void* context) {
    ClippyApp* app = context;

    app->bad_usb_payload =
        clippy_bad_usb_payload_setup(app->string_to_print, app->interface, app->delay);
    clippy_bad_usb_payload_set_keyboard_layout(app->bad_usb_payload, app->keyboard_layout);

    FuriString* layout;
    layout = furi_string_alloc();
    path_extract_filename(app->keyboard_layout, layout, true);
    clippy_bad_usb_view_set_layout(app->bad_usb_view, furi_string_get_cstr(layout));
    furi_string_free(layout);

    clippy_bad_usb_view_set_state(
        app->bad_usb_view, clippy_bad_usb_payload_get_state(app->bad_usb_payload));

    clippy_bad_usb_view_set_button_callback(
        app->bad_usb_view, clippy_scene_bad_usb_work_button_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ClippyAppViewBadUsbWork);
}

void clippy_scene_bad_usb_work_on_exit(void* context) {
    ClippyApp* app = context;

    clippy_bad_usb_payload_teardown(app->bad_usb_payload);
    app->bad_usb_payload = NULL;
}
