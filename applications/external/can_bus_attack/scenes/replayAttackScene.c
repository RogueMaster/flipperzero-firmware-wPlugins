#include "../app_user.h"

static bool using_default = false;

static int32_t thread_replay_attack(void* context);
static void draw_replay_attack_scene(App* app, bool using_default);

static void replay_attack_button_callback(GuiButtonType type, InputType input, void* context) {
    if(type != GuiButtonTypeCenter || input != InputTypeShort) return;
    App* app = context;

    FuriThread* replay_thread =
        furi_thread_alloc_ex("ReplayAttack", 1024, thread_replay_attack, app);
    furi_thread_start(replay_thread);
    furi_thread_join(replay_thread);
    furi_thread_free(replay_thread);

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 34, AlignCenter, AlignCenter, FontPrimary, "Replay sent!");
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
    furi_delay_ms(350);

    draw_replay_attack_scene(app, using_default);
}

static void draw_replay_attack_scene(App* app, bool using_default) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 18, AlignCenter, AlignCenter, FontPrimary, "Replay Attack");

    if(using_default) {
        widget_add_string_multiline_element(
            app->widget,
            64,
            38,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "No frame selected.\n One sample will be used.");
    } else {
        widget_add_string_element(
            app->widget,
            64,
            36,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "Press OK to repeat the message.");
    }

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "SEND", replay_attack_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

void app_scene_replay_attack_on_enter(void* context) {
    App* app = context;
    using_default = false;

    // If there's no valid frame, set a sample one
    if(app->replay_frame.canId == 0 || app->replay_frame.data_lenght == 0) {
        app->replay_frame.canId = 0x123;
        app->replay_frame.ext = 0;
        app->replay_frame.req = 0;
        app->replay_frame.data_lenght = 8;
        for(int i = 0; i < 8; i++)
            app->replay_frame.buffer[i] = i;
        using_default = true;
    }

    draw_replay_attack_scene(app, using_default);
}

bool app_scene_replay_attack_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_replay_attack_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

static int32_t thread_replay_attack(void* context) {
    App* app = context;
    MCP2515* mcp_can = app->mcp_can;
    mcp_can->mode = MCP_NORMAL;

    if(mcp2515_init(mcp_can) != ERROR_OK) {
        draw_device_no_connected(app);
        return 0;
    }

    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_blink_start(LightRed, 255, 20, 60);
    furi_delay_ms(50);
    send_can_frame(mcp_can, &app->replay_frame);
    furi_delay_ms(50);
    furi_hal_light_blink_stop();
    furi_hal_light_set(LightGreen, 255);

    deinit_mcp2515(mcp_can);
    return 0;
}
