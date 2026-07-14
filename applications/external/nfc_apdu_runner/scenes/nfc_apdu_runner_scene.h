#pragma once

#include <gui/scene_manager.h>

// 场景枚举
typedef enum {
    NfcApduRunnerSceneStart,
    NfcApduRunnerSceneFileSelect,
    NfcApduRunnerSceneCardPlacement,
    NfcApduRunnerSceneRunning,
    NfcApduRunnerSceneResults,
    NfcApduRunnerSceneSaveFile,
    NfcApduRunnerSceneLogs,
    NfcApduRunnerSceneAbout,
    NfcApduRunnerSceneCount,
} NfcApduRunnerScene;

extern const SceneManagerHandlers nfc_apdu_runner_scene_handlers;

// 场景回调
void nfc_apdu_runner_scene_start_on_enter(void* context);
bool nfc_apdu_runner_scene_start_on_event(void* context, SceneManagerEvent event);
void nfc_apdu_runner_scene_start_on_exit(void* context);

void nfc_apdu_runner_scene_file_select_on_enter(void* context);
bool nfc_apdu_runner_scene_file_select_on_event(void* context, SceneManagerEvent event);
void nfc_apdu_runner_scene_file_select_on_exit(void* context);

void nfc_apdu_runner_scene_card_placement_on_enter(void* context);
bool nfc_apdu_runner_scene_card_placement_on_event(void* context, SceneManagerEvent event);
void nfc_apdu_runner_scene_card_placement_on_exit(void* context);

void nfc_apdu_runner_scene_running_on_enter(void* context);
bool nfc_apdu_runner_scene_running_on_event(void* context, SceneManagerEvent event);
void nfc_apdu_runner_scene_running_on_exit(void* context);

void nfc_apdu_runner_scene_results_on_enter(void* context);
bool nfc_apdu_runner_scene_results_on_event(void* context, SceneManagerEvent event);
void nfc_apdu_runner_scene_results_on_exit(void* context);

void nfc_apdu_runner_scene_logs_on_enter(void* context);
bool nfc_apdu_runner_scene_logs_on_event(void* context, SceneManagerEvent event);
void nfc_apdu_runner_scene_logs_on_exit(void* context);

void nfc_apdu_runner_scene_about_on_enter(void* context);
bool nfc_apdu_runner_scene_about_on_event(void* context, SceneManagerEvent event);
void nfc_apdu_runner_scene_about_on_exit(void* context);

void nfc_apdu_runner_scene_save_file_on_enter(void* context);
bool nfc_apdu_runner_scene_save_file_on_event(void* context, SceneManagerEvent event);
void nfc_apdu_runner_scene_save_file_on_exit(void* context);
