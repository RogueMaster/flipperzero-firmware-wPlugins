#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"
#include <gui/elements.h>

// 卡片放置场景按钮回调
static void nfc_apdu_runner_scene_card_placement_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcApduRunner* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

// 卡片放置场景进入回调
void nfc_apdu_runner_scene_card_placement_on_enter(void* context) {
    NfcApduRunner* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    // 添加图标 - 使用内置元素
    widget_add_string_element(widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Place card on");
    widget_add_string_element(
        widget, 64, 20, AlignCenter, AlignTop, FontPrimary, "the Flipper's back");

    // 添加按钮
    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Back",
        nfc_apdu_runner_scene_card_placement_widget_callback,
        app);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "Run",
        nfc_apdu_runner_scene_card_placement_widget_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewWidget);
}

// 卡片放置场景事件回调
bool nfc_apdu_runner_scene_card_placement_on_event(void* context, SceneManagerEvent event) {
    NfcApduRunner* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            // 返回上一个场景
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == GuiButtonTypeRight) {
            // 进入运行场景
            scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneRunning);
            consumed = true;
        }
    }

    return consumed;
}

// 卡片放置场景退出回调
void nfc_apdu_runner_scene_card_placement_on_exit(void* context) {
    NfcApduRunner* app = context;
    widget_reset(app->widget);
}
