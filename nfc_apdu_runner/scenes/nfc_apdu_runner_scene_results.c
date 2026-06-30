/*
 * @Author: SpenserCai
 * @Date: 2025-03-07 16:31:44
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-11 09:59:00
 * @Description: file content
 */
#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// 结果场景按钮回调
static void nfc_apdu_runner_scene_results_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcApduRunner* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

// 结果场景进入回调
void nfc_apdu_runner_scene_results_on_enter(void* context) {
    NfcApduRunner* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "执行结果");

    FuriString* text = furi_string_alloc();

    for(uint32_t i = 0; i < app->response_count; i++) {
        furi_string_cat_printf(text, "Command: %s\n", app->responses[i].command);
        furi_string_cat_str(text, "Response: ");

        for(uint16_t j = 0; j < app->responses[i].response_length; j++) {
            furi_string_cat_printf(text, "%02X", app->responses[i].response[j]);
        }

        furi_string_cat_str(text, "\n\n");
    }

    widget_add_text_scroll_element(widget, 0, 16, 128, 35, furi_string_get_cstr(text));
    furi_string_free(text);

    // 添加按钮
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Cancel", nfc_apdu_runner_scene_results_button_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Save", nfc_apdu_runner_scene_results_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewWidget);
}

// 结果场景事件回调
bool nfc_apdu_runner_scene_results_on_event(void* context, SceneManagerEvent event) {
    NfcApduRunner* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            // 取消按钮 - 返回到开始场景
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, NfcApduRunnerSceneStart);
            consumed = true;
        } else if(event.event == GuiButtonTypeRight) {
            // 保存按钮 - 进入保存文件场景
            // 设置场景状态，标记为进入保存文件场景
            scene_manager_set_scene_state(
                app->scene_manager, NfcApduRunnerSceneResults, NfcApduRunnerSceneSaveFile);
            scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneSaveFile);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // 返回键 - 返回到开始场景
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, NfcApduRunnerSceneStart);
        consumed = true;
    }

    return consumed;
}

// 结果场景退出回调
void nfc_apdu_runner_scene_results_on_exit(void* context) {
    NfcApduRunner* app = context;
    widget_reset(app->widget);

    // 只有在不是进入保存文件场景时才释放响应资源
    if(app->responses != NULL &&
       scene_manager_get_scene_state(app->scene_manager, NfcApduRunnerSceneResults) !=
           NfcApduRunnerSceneSaveFile) {
        nfc_apdu_responses_free(app->responses, app->response_count);
        app->responses = NULL;
        app->response_count = 0;
    }
}
