#include "../../app_user.h"

// To know the session
uint8_t reset = 0;

// Thread to send the message
static int32_t ecu_reset_thread(void* context);

// callback for the diagnostic sessions menu
void submenu_ecu_reset_callback(void* context, uint32_t index) {
    App* app = context;
    reset = index + 1;
    scene_manager_next_scene(app->scene_manager, app_scene_uds_ecu_reset_response_option);
}

// Scene on enter
void app_scene_uds_ecu_reset_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "Type of Reset");

    submenu_add_item(app->submenu, "Hard Reset", 0, submenu_ecu_reset_callback, app);
    submenu_add_item(app->submenu, "Key On-Off Reset", 1, submenu_ecu_reset_callback, app);
    submenu_add_item(app->submenu, "Soft Reset", 2, submenu_ecu_reset_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Scene on event
bool app_scene_uds_ecu_reset_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

// Scene on exit
void app_scene_uds_ecu_reset_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// Scene on enter for the response
void app_scene_uds_ecu_reset_response_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);

    app->thread = furi_thread_alloc_ex("SetSessionThread", 1024, ecu_reset_thread, app);
    furi_thread_start(app->thread);
}

// Scene on event for the response
bool app_scene_uds_ecu_reset_response_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    App* app = context;

    UNUSED(app);
    UNUSED(event);

    return consumed;
}

// Scene on exit for the response
void app_scene_uds_ecu_reset_response_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);

    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

// Thread to work
static int32_t ecu_reset_thread(void* context) {
    App* app = context;
    MCP2515* CAN = app->mcp_can;

    FuriString* text = app->text;

    furi_string_reset(text);

    UDS_SERVICE* uds_service = uds_service_alloc(
        app->uds_send_id, app->uds_received_id, CAN->mode, CAN->clck, CAN->bitRate);

    if(uds_init(uds_service)) {
        furi_delay_ms(500);

        if(uds_reset_ecu(uds_service, (type_ecu_reset)reset)) {
            widget_add_string_multiline_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "RESET OK");

        } else {
            draw_transmition_failure(app);
        }
    } else {
        draw_device_no_connected(app);
    }

    free_uds(uds_service);

    return 0;
}
