// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <gui/scene_manager.h>

typedef enum {
    ReconSceneStart,
    ReconSceneFlock,
    ReconSceneFlockDetail,
    ReconSceneNfc,
    ReconSceneReports,
    ReconSceneSettings,
    ReconSceneAbout,
    ReconSceneWifi,
    ReconSceneWifiDetail,
    ReconSceneBle,
    ReconSceneBleDetail,
    ReconSceneFirmware,
    ReconSceneFirmwareRun,
    ReconSceneFlockMap,
    ReconSceneDeflockHandoff,
    ReconSceneGuardian,
    ReconSceneGuardianSus,
    ReconSceneLocator,
    ReconSceneLocatorHome,
    ReconSceneNum,
} ReconScene;

extern const SceneManagerHandlers recon_scene_handlers;

void recon_scene_start_on_enter(void* context);
bool recon_scene_start_on_event(void* context, SceneManagerEvent event);
void recon_scene_start_on_exit(void* context);

void recon_scene_flock_on_enter(void* context);
bool recon_scene_flock_on_event(void* context, SceneManagerEvent event);
void recon_scene_flock_on_exit(void* context);

void recon_scene_flock_detail_on_enter(void* context);
bool recon_scene_flock_detail_on_event(void* context, SceneManagerEvent event);
void recon_scene_flock_detail_on_exit(void* context);

void recon_scene_nfc_on_enter(void* context);
bool recon_scene_nfc_on_event(void* context, SceneManagerEvent event);
void recon_scene_nfc_on_exit(void* context);

void recon_scene_reports_on_enter(void* context);
bool recon_scene_reports_on_event(void* context, SceneManagerEvent event);
void recon_scene_reports_on_exit(void* context);

void recon_scene_settings_on_enter(void* context);
bool recon_scene_settings_on_event(void* context, SceneManagerEvent event);
void recon_scene_settings_on_exit(void* context);

void recon_scene_about_on_enter(void* context);
bool recon_scene_about_on_event(void* context, SceneManagerEvent event);
void recon_scene_about_on_exit(void* context);

void recon_scene_wifi_on_enter(void* context);
bool recon_scene_wifi_on_event(void* context, SceneManagerEvent event);
void recon_scene_wifi_on_exit(void* context);

void recon_scene_wifi_detail_on_enter(void* context);
bool recon_scene_wifi_detail_on_event(void* context, SceneManagerEvent event);
void recon_scene_wifi_detail_on_exit(void* context);

void recon_scene_ble_on_enter(void* context);
bool recon_scene_ble_on_event(void* context, SceneManagerEvent event);
void recon_scene_ble_on_exit(void* context);

void recon_scene_ble_detail_on_enter(void* context);
bool recon_scene_ble_detail_on_event(void* context, SceneManagerEvent event);
void recon_scene_ble_detail_on_exit(void* context);

void recon_scene_firmware_on_enter(void* context);
bool recon_scene_firmware_on_event(void* context, SceneManagerEvent event);
void recon_scene_firmware_on_exit(void* context);

void recon_scene_firmware_run_on_enter(void* context);
bool recon_scene_firmware_run_on_event(void* context, SceneManagerEvent event);
void recon_scene_firmware_run_on_exit(void* context);

void recon_scene_flock_map_on_enter(void* context);
bool recon_scene_flock_map_on_event(void* context, SceneManagerEvent event);
void recon_scene_flock_map_on_exit(void* context);

void recon_scene_deflock_handoff_on_enter(void* context);
bool recon_scene_deflock_handoff_on_event(void* context, SceneManagerEvent event);
void recon_scene_deflock_handoff_on_exit(void* context);

void recon_scene_guardian_on_enter(void* context);
bool recon_scene_guardian_on_event(void* context, SceneManagerEvent event);
void recon_scene_guardian_on_exit(void* context);

void recon_scene_guardian_sus_on_enter(void* context);
bool recon_scene_guardian_sus_on_event(void* context, SceneManagerEvent event);
void recon_scene_guardian_sus_on_exit(void* context);

void recon_scene_locator_on_enter(void* context);
bool recon_scene_locator_on_event(void* context, SceneManagerEvent event);
void recon_scene_locator_on_exit(void* context);

void recon_scene_locator_home_on_enter(void* context);
bool recon_scene_locator_home_on_event(void* context, SceneManagerEvent event);
void recon_scene_locator_home_on_exit(void* context);
