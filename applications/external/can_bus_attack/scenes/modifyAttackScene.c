#include "../app_user.h"

static void modify_attack_center_callback(GuiButtonType type, InputType input, void* context);
static void modify_attack_left_callback(GuiButtonType type, InputType input, void* context);
static void modify_attack_right_callback(GuiButtonType type, InputType input, void* context);
static void draw_modify_attack_scene(App* app);
static int32_t thread_modify_attack(void* context);

void app_scene_modify_attack_on_enter(void* context) {
    App* app = context;
    draw_modify_attack_scene(app);
}

bool app_scene_modify_attack_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        draw_modify_attack_scene(app);
        return true;
    }

    return false;
}

void app_scene_modify_attack_on_exit(void* context) {
    App* app = context;

    widget_reset(app->widget);
}

static void modify_attack_center_callback(GuiButtonType type, InputType input, void* context) {
    if(type != GuiButtonTypeCenter || input != InputTypeShort) return;

    App* app = context;

    FuriThread* temp_thread = furi_thread_alloc_ex("ModifyOnce", 1024, thread_modify_attack, app);
    furi_thread_start(temp_thread);
    furi_thread_join(temp_thread);
    furi_thread_free(temp_thread);

    draw_modify_attack_scene(app);
}

static void modify_attack_right_callback(GuiButtonType type, InputType input, void* context) {
    if(type == GuiButtonTypeRight && input == InputTypeShort) {
        App* app = context;
        scene_manager_next_scene(app->scene_manager, app_scene_modify_attack_edit_frame);
    }
}

static void modify_attack_left_callback(GuiButtonType type, InputType input, void* context) {
    if(type == GuiButtonTypeLeft && input == InputTypeShort) {
        App* app = context;
        scene_manager_next_scene(app->scene_manager, app_scene_modify_attack_edit_id);
    }
}

static void draw_modify_attack_scene(App* app) {
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Modify & Send");
    CANFRAME* frame = app->modify_frame;
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "ID: 0x%lX", frame->canId);
    widget_add_string_element(app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary, id_str);

    char data_str[64] = {0};
    for(uint8_t i = 0; i < frame->data_lenght; i++) {
        char byte[4];
        snprintf(byte, sizeof(byte), "%02X ", frame->buffer[i]);
        strcat(data_str, byte);
    }

    widget_add_string_element(app->widget, 64, 35, AlignCenter, AlignTop, FontSecondary, data_str);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "SEND", modify_attack_center_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "ID", modify_attack_left_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "DATA", modify_attack_right_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

static int32_t thread_modify_attack(void* context) {
    App* app = context;
    MCP2515* mcp_can = app->mcp_can;
    mcp_can->mode = MCP_NORMAL;

    if(mcp2515_init(mcp_can) != ERROR_OK) {
        draw_device_no_connected(app);
        return 0;
    }

    CANFRAME* frame = app->modify_frame;
    frame->ext = 0;
    frame->req = 0;
    frame->data_lenght = 8;

    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_blink_start(LightRed, 255, 20, 100);

    send_can_frame(mcp_can, frame);
    furi_delay_ms(300);

    furi_hal_light_blink_stop();
    furi_hal_light_set(LightGreen, 255);
    deinit_mcp2515(mcp_can);

    return 0;
}
