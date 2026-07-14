/*
 * @Author: SpenserCai
 * @Date: 2025-03-07 16:30:29
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-11 09:50:54
 * @Description: file content
 */
#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// 开始场景菜单项枚举
enum {
    NfcApduRunnerStartSubmenuIndexLoadFile,
    NfcApduRunnerStartSubmenuIndexAbout,
};

// 开始场景菜单回调
void nfc_apdu_runner_start_submenu_callback(void* context, uint32_t index) {
    NfcApduRunner* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// 开始场景进入回调
void nfc_apdu_runner_scene_start_on_enter(void* context) {
    NfcApduRunner* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);

    submenu_add_item(
        submenu,
        "Load Script",
        NfcApduRunnerStartSubmenuIndexLoadFile,
        nfc_apdu_runner_start_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "About",
        NfcApduRunnerStartSubmenuIndexAbout,
        nfc_apdu_runner_start_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, NfcApduRunnerSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewSubmenu);
}

// 开始场景事件回调
bool nfc_apdu_runner_scene_start_on_event(void* context, SceneManagerEvent event) {
    NfcApduRunner* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, NfcApduRunnerSceneStart, event.event);

        switch(event.event) {
        case NfcApduRunnerStartSubmenuIndexLoadFile:
            scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneFileSelect);
            consumed = true;
            break;
        case NfcApduRunnerStartSubmenuIndexAbout:
            scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneAbout);
            consumed = true;
            break;
        }
    }

    return consumed;
}

// 开始场景退出回调
void nfc_apdu_runner_scene_start_on_exit(void* context) {
    NfcApduRunner* app = context;
    submenu_reset(app->submenu);
}
