/*
 * Scene definitions.
 */

#pragma once

#include <gui/scene_manager.h>

typedef enum {
    TagTinkerSceneWarning,
    TagTinkerSceneMainMenu,
    TagTinkerSceneSettings,
    TagTinkerSceneBroadcastMenu,
    TagTinkerSceneBroadcast,
    TagTinkerSceneTargetMenu,
    TagTinkerSceneTargetActions,
    TagTinkerSceneBarcodeInput,
    TagTinkerSceneTextInput,
    TagTinkerScenePresetList,
    TagTinkerSceneSyncedImageList,
    TagTinkerSceneSizePicker,
    TagTinkerSceneImageOptions,
    TagTinkerSceneTransmit,
    TagTinkerSceneAbout,
    TagTinkerSceneTextBox,
    TagTinkerSceneNfcScan,
    TagTinkerSceneCount,
} TagTinkerScene;

void tagtinker_scene_warning_on_enter(void* ctx);
bool tagtinker_scene_warning_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_warning_on_exit(void* ctx);

void tagtinker_scene_main_menu_on_enter(void* ctx);
bool tagtinker_scene_main_menu_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_main_menu_on_exit(void* ctx);

void tagtinker_scene_settings_on_enter(void* ctx);
bool tagtinker_scene_settings_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_settings_on_exit(void* ctx);

void tagtinker_scene_broadcast_menu_on_enter(void* ctx);
bool tagtinker_scene_broadcast_menu_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_broadcast_menu_on_exit(void* ctx);

void tagtinker_scene_broadcast_on_enter(void* ctx);
bool tagtinker_scene_broadcast_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_broadcast_on_exit(void* ctx);

void tagtinker_scene_target_menu_on_enter(void* ctx);
bool tagtinker_scene_target_menu_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_target_menu_on_exit(void* ctx);

void tagtinker_scene_target_actions_on_enter(void* ctx);
bool tagtinker_scene_target_actions_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_target_actions_on_exit(void* ctx);

void tagtinker_scene_barcode_input_on_enter(void* ctx);
bool tagtinker_scene_barcode_input_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_barcode_input_on_exit(void* ctx);

void tagtinker_scene_text_input_on_enter(void* ctx);
bool tagtinker_scene_text_input_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_text_input_on_exit(void* ctx);

void tagtinker_scene_size_picker_on_enter(void* ctx);
bool tagtinker_scene_size_picker_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_size_picker_on_exit(void* ctx);

void tagtinker_scene_preset_list_on_enter(void* ctx);
bool tagtinker_scene_preset_list_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_preset_list_on_exit(void* ctx);

void tagtinker_scene_synced_image_list_on_enter(void* ctx);
bool tagtinker_scene_synced_image_list_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_synced_image_list_on_exit(void* ctx);

void tagtinker_scene_image_options_on_enter(void* ctx);
bool tagtinker_scene_image_options_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_image_options_on_exit(void* ctx);

void tagtinker_scene_transmit_on_enter(void* ctx);
bool tagtinker_scene_transmit_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_transmit_on_exit(void* ctx);

void tagtinker_scene_about_on_enter(void* ctx);
bool tagtinker_scene_about_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_about_on_exit(void* ctx);

void tagtinker_scene_text_box_on_enter(void* ctx);
bool tagtinker_scene_text_box_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_text_box_on_exit(void* ctx);

void tagtinker_scene_nfc_scan_on_enter(void* ctx);
bool tagtinker_scene_nfc_scan_on_event(void* ctx, SceneManagerEvent event);
void tagtinker_scene_nfc_scan_on_exit(void* ctx);
