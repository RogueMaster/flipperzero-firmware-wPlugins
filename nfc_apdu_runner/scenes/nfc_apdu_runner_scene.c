#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// 生成场景进入处理函数数组
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const nfc_apdu_runner_on_enter_handlers[])(void*) = {
    nfc_apdu_runner_scene_start_on_enter,
    nfc_apdu_runner_scene_file_select_on_enter,
    nfc_apdu_runner_scene_card_placement_on_enter,
    nfc_apdu_runner_scene_running_on_enter,
    nfc_apdu_runner_scene_results_on_enter,
    nfc_apdu_runner_scene_save_file_on_enter,
    nfc_apdu_runner_scene_logs_on_enter,
    nfc_apdu_runner_scene_about_on_enter,
};
#undef ADD_SCENE

// 生成场景事件处理函数数组
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const nfc_apdu_runner_on_event_handlers[])(void* context, SceneManagerEvent event) = {
    nfc_apdu_runner_scene_start_on_event,
    nfc_apdu_runner_scene_file_select_on_event,
    nfc_apdu_runner_scene_card_placement_on_event,
    nfc_apdu_runner_scene_running_on_event,
    nfc_apdu_runner_scene_results_on_event,
    nfc_apdu_runner_scene_save_file_on_event,
    nfc_apdu_runner_scene_logs_on_event,
    nfc_apdu_runner_scene_about_on_event,
};
#undef ADD_SCENE

// 生成场景退出处理函数数组
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const nfc_apdu_runner_on_exit_handlers[])(void* context) = {
    nfc_apdu_runner_scene_start_on_exit,
    nfc_apdu_runner_scene_file_select_on_exit,
    nfc_apdu_runner_scene_card_placement_on_exit,
    nfc_apdu_runner_scene_running_on_exit,
    nfc_apdu_runner_scene_results_on_exit,
    nfc_apdu_runner_scene_save_file_on_exit,
    nfc_apdu_runner_scene_logs_on_exit,
    nfc_apdu_runner_scene_about_on_exit,
};
#undef ADD_SCENE

// 初始化场景处理器配置结构
const SceneManagerHandlers nfc_apdu_runner_scene_handlers = {
    .on_enter_handlers = nfc_apdu_runner_on_enter_handlers,
    .on_event_handlers = nfc_apdu_runner_on_event_handlers,
    .on_exit_handlers = nfc_apdu_runner_on_exit_handlers,
    .scene_num = NfcApduRunnerSceneCount,
};
