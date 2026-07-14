#include "../../app_user.h"

// Thread to get the vin
static int32_t uds_get_vin_thread(void* context);

/**
 * UDS get Vin Scene
 */
// Scene on enter
void app_scene_uds_request_vin_on_enter(void* context) {
    App* app = context;

    widget_reset(app->widget);

    app->thread = furi_thread_alloc_ex("ManualUDS", 1024, uds_get_vin_thread, app);
    furi_thread_start(app->thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

// Scene on event
bool app_scene_uds_request_vin_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

// Scene on exit
void app_scene_uds_request_vin_on_exit(void* context) {
    App* app = context;

    furi_thread_join(app->thread);
    furi_thread_free(app->thread);

    widget_reset(app->widget);
}

/**
 * Thread to work with
 */

static int32_t uds_get_vin_thread(void* context) {
    App* app = context;
    MCP2515* CAN = app->mcp_can;

    FuriString* text = app->text;

    furi_string_reset(text);

    UDS_SERVICE* uds_service = uds_service_alloc(
        app->uds_send_id, app->uds_received_id, CAN->mode, CAN->clck, CAN->bitRate);

    if(uds_init(uds_service)) {
        furi_delay_ms(500);
        if(uds_get_vin(uds_service, text)) {
            widget_add_string_element(
                app->widget, 64, 25, AlignCenter, AlignCenter, FontPrimary, "VIN:");
            widget_add_string_element(
                app->widget,
                64,
                35,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                furi_string_get_cstr(text));
        } else {
            draw_transmition_failure(app);
        }
    } else {
        draw_device_no_connected(app);
    }

    free_uds(uds_service);

    return 0;
}
