#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"
#include <storage/storage.h>
#include <dialogs/dialogs.h>

// 文件选择场景进入回调
void nfc_apdu_runner_scene_file_select_on_enter(void* context) {
    NfcApduRunner* app = context;
    DialogsFileBrowserOptions browser_options;

    dialog_file_browser_set_basic_options(&browser_options, FILE_EXTENSION, NULL);
    browser_options.base_path = APP_DIRECTORY_PATH;
    browser_options.hide_ext = false;

    // 确保目录存在
    Storage* storage = app->storage;
    if(!storage_dir_exists(storage, APP_DIRECTORY_PATH)) {
        if(!storage_simply_mkdir(storage, APP_DIRECTORY_PATH)) {
            dialog_message_show_storage_error(app->dialogs, "Cannot create\napp folder");
            scene_manager_previous_scene(app->scene_manager);
            return;
        }
    }

    // 设置初始路径为APP_DIRECTORY_PATH
    furi_string_set(app->file_path, APP_DIRECTORY_PATH);

    // 显示文件浏览器
    bool success =
        dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options);

    if(success) {
        scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneCardPlacement);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

// 文件选择场景事件回调
bool nfc_apdu_runner_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

// 文件选择场景退出回调
void nfc_apdu_runner_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
