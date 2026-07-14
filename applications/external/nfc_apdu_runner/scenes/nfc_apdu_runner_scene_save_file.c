#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"
#include <toolbox/path.h>

// 文本输入回调
static void nfc_apdu_runner_scene_save_file_text_input_callback(void* context) {
    NfcApduRunner* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcApduRunnerCustomEventViewExit);
}

// 保存文件场景进入回调
void nfc_apdu_runner_scene_save_file_on_enter(void* context) {
    NfcApduRunner* app = context;

    // 分配文本输入视图
    TextInput* text_input = app->text_input;

    // 提取原始文件名作为默认值
    FuriString* filename_str = furi_string_alloc();
    FuriString* file_path_str = furi_string_alloc_set(app->file_path);
    path_extract_filename(file_path_str, filename_str, true);

    // 移除扩展名
    size_t ext_pos = furi_string_search_str(filename_str, FILE_EXTENSION);
    if(ext_pos != FURI_STRING_FAILURE) {
        furi_string_left(filename_str, ext_pos);
    }

    // 添加响应扩展名
    furi_string_cat_str(filename_str, RESPONSE_EXTENSION);

    // 设置文本输入
    text_input_reset(text_input);
    text_input_set_header_text(text_input, "Input save file name");
    text_input_set_result_callback(
        text_input,
        nfc_apdu_runner_scene_save_file_text_input_callback,
        app,
        app->file_name_buf,
        MAX_FILENAME_LEN,
        true);

    // 设置默认文件名
    strncpy(app->file_name_buf, furi_string_get_cstr(filename_str), MAX_FILENAME_LEN);

    // 释放临时字符串
    furi_string_free(filename_str);
    furi_string_free(file_path_str);

    // 切换到文本输入视图
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewTextInput);
}

// 保存文件场景事件回调
bool nfc_apdu_runner_scene_save_file_on_event(void* context, SceneManagerEvent event) {
    NfcApduRunner* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcApduRunnerCustomEventViewExit) {
            // 构建完整的保存路径
            FuriString* save_path = furi_string_alloc();
            furi_string_printf(save_path, "%s/%s", APP_DIRECTORY_PATH, app->file_name_buf);

            // 保存响应结果
            bool success = nfc_apdu_save_responses(
                app->storage,
                furi_string_get_cstr(app->file_path),
                app->responses,
                app->response_count,
                furi_string_get_cstr(save_path));

            furi_string_free(save_path);

            if(!success) {
                // 显示错误消息
                dialog_message_show_storage_error(app->dialogs, "Save failed");
            } else {
                // 显示成功消息
                DialogMessage* message = dialog_message_alloc();
                dialog_message_set_header(message, "Save success", 64, 0, AlignCenter, AlignTop);
                dialog_message_set_text(
                    message, "Responses saved", 64, 32, AlignCenter, AlignCenter);
                dialog_message_set_buttons(message, "OK", NULL, NULL);
                dialog_message_show(app->dialogs, message);
                dialog_message_free(message);
            }

            // 返回到开始场景
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, NfcApduRunnerSceneStart);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // 返回到结果场景
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

// 保存文件场景退出回调
void nfc_apdu_runner_scene_save_file_on_exit(void* context) {
    NfcApduRunner* app = context;
    text_input_reset(app->text_input);

    // 释放响应资源
    if(app->responses != NULL) {
        nfc_apdu_responses_free(app->responses, app->response_count);
        app->responses = NULL;
        app->response_count = 0;
    }
}
