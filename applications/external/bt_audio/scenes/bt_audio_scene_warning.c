#include "../bt_audio.h"

static void
    bt_audio_scene_warning_widget_callback(GuiButtonType result, InputType type, void* context) {
    BtAudio* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void bt_audio_scene_warning_on_enter(void* context) {
    BtAudio* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    widget_add_string_element(
        widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "ESP32 Not Detected");

    widget_add_text_scroll_element(
        widget,
        0,
        12,
        128,
        38,
        "BT Audio requires an ESP32\n"
        "board with compatible firmware.\n\n"
        "Setup guide:\n"
        "github.com/FatherDivine/\n"
        "flipperzero-firmware-wPlugins\n"
        "blob/dev/applications/\n"
        "external/bt_audio/README.md\n\n"
        "Press OK for QR code");

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "QR", bt_audio_scene_warning_widget_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Back", bt_audio_scene_warning_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
}

bool bt_audio_scene_warning_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(app->scene_manager, BtAudioSceneQRCode);
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_warning_on_exit(void* context) {
    BtAudio* app = context;
    widget_reset(app->widget);
}
