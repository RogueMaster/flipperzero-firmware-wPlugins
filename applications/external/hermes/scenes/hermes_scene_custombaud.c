#include "../hermes_i.h"

/* v1.0 could only name rates from its table, so a board running something
 * unusual (100000, 250000 variants, a divider that landed oddly) had no way in.
 * This is that way in. */

static void hermes_scene_custombaud_callback(void* context, int32_t number) {
    HermesApp* app = context;
    app->custom_baud = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventBaudEntered);
}

void hermes_scene_custombaud_on_enter(void* context) {
    HermesApp* app = context;

    number_input_set_header_text(app->number_input, "Baud rate");
    number_input_set_result_callback(
        app->number_input,
        hermes_scene_custombaud_callback,
        app,
        app->custom_baud,
        HERMES_BAUD_MIN,
        HERMES_BAUD_MAX);

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewNumberInput);
}

bool hermes_scene_custombaud_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == HermesCustomEventBaudEntered) {
        /* The hardware has the final say on whether a divider exists for this
         * rate, so ask it rather than opening a link that cannot work. */
        FuriHalSerialHandle* handle =
            furi_hal_serial_control_acquire(hermes_port_serial_id(app->port));
        bool supported = false;
        if(handle) {
            supported = furi_hal_serial_is_baud_rate_supported(handle, (uint32_t)app->custom_baud);
            furi_hal_serial_control_release(handle);
        }

        if(!supported) {
            hermes_notify_found(app, false);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }

        app->link.baud = (uint32_t)app->custom_baud;
        app->link.verified = false; // typed in, not measured
        scene_manager_next_scene(app->scene_manager, HermesSceneConsole);
        return true;
    }

    return false;
}

void hermes_scene_custombaud_on_exit(void* context) {
    UNUSED(context);
}
