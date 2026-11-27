#include "../bc_scanner_app_i.h"

/**
 * RPC scene: shows the reception/transmission state while the app runs in RPC mode.
 *
 * The app is driven by the phone over BLE; this screen only reflects what happened to the last
 * data exchange (waiting / received / sent / error).
 */
static void bc_scanner_scene_rpc_render(BarCodeApp* app) {
    furi_assert(app);
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "RPC mode");

    char buffer[40];
    switch(app->rpc_status) {
    case BarCodeRpcStatusWaiting:
        widget_add_string_multiline_element(
            app->widget,
            64,
            22,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "Waiting for\ndata from phone...");
        break;
    case BarCodeRpcStatusReceived:
        snprintf(buffer, sizeof(buffer), "Received %u bytes", (unsigned)app->rpc_last_size);
        widget_add_string_element(
            app->widget, 64, 30, AlignCenter, AlignTop, FontSecondary, buffer);
        break;
    case BarCodeRpcStatusSent:
        snprintf(buffer, sizeof(buffer), "Sent %u bytes", (unsigned)app->rpc_last_size);
        widget_add_string_element(
            app->widget, 64, 30, AlignCenter, AlignTop, FontSecondary, buffer);
        break;
    case BarCodeRpcStatusError:
        widget_add_string_multiline_element(
            app->widget,
            64,
            24,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "USB not ready\nor send error");
        break;
    }

    widget_add_string_element(
        app->widget, 64, 55, AlignCenter, AlignTop, FontSecondary, "Disconnect to exit");
}

void bc_scanner_scene_rpc_on_enter(void* context) {
    BarCodeApp* app = context;
    app->rpc_status = BarCodeRpcStatusWaiting;
    bc_scanner_scene_rpc_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BarCodeAppViewError);
}

bool bc_scanner_scene_rpc_on_event(void* context, SceneManagerEvent event) {
    BarCodeApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BarCodeCustomEventRpcStatusUpdate) {
            bc_scanner_scene_rpc_render(app);
            consumed = true;
        }
    }
    return consumed;
}

void bc_scanner_scene_rpc_on_exit(void* context) {
    BarCodeApp* app = context;
    widget_reset(app->widget);
}
