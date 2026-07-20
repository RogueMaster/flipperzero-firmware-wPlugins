// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "recon_scene.h"

static void (*const recon_scene_on_enter_handlers[])(void*) = {
    recon_scene_start_on_enter,
    recon_scene_flock_on_enter,
    recon_scene_flock_detail_on_enter,
    recon_scene_nfc_on_enter,
    recon_scene_reports_on_enter,
    recon_scene_settings_on_enter,
    recon_scene_about_on_enter,
    recon_scene_wifi_on_enter,
    recon_scene_wifi_detail_on_enter,
    recon_scene_ble_on_enter,
    recon_scene_ble_detail_on_enter,
    recon_scene_firmware_on_enter,
    recon_scene_firmware_run_on_enter,
    recon_scene_flock_map_on_enter,
    recon_scene_deflock_handoff_on_enter,
    recon_scene_guardian_on_enter,
    recon_scene_guardian_sus_on_enter,
    recon_scene_locator_on_enter,
    recon_scene_locator_home_on_enter,
};

static bool (*const recon_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    recon_scene_start_on_event,
    recon_scene_flock_on_event,
    recon_scene_flock_detail_on_event,
    recon_scene_nfc_on_event,
    recon_scene_reports_on_event,
    recon_scene_settings_on_event,
    recon_scene_about_on_event,
    recon_scene_wifi_on_event,
    recon_scene_wifi_detail_on_event,
    recon_scene_ble_on_event,
    recon_scene_ble_detail_on_event,
    recon_scene_firmware_on_event,
    recon_scene_firmware_run_on_event,
    recon_scene_flock_map_on_event,
    recon_scene_deflock_handoff_on_event,
    recon_scene_guardian_on_event,
    recon_scene_guardian_sus_on_event,
    recon_scene_locator_on_event,
    recon_scene_locator_home_on_event,
};

static void (*const recon_scene_on_exit_handlers[])(void*) = {
    recon_scene_start_on_exit,
    recon_scene_flock_on_exit,
    recon_scene_flock_detail_on_exit,
    recon_scene_nfc_on_exit,
    recon_scene_reports_on_exit,
    recon_scene_settings_on_exit,
    recon_scene_about_on_exit,
    recon_scene_wifi_on_exit,
    recon_scene_wifi_detail_on_exit,
    recon_scene_ble_on_exit,
    recon_scene_ble_detail_on_exit,
    recon_scene_firmware_on_exit,
    recon_scene_firmware_run_on_exit,
    recon_scene_flock_map_on_exit,
    recon_scene_deflock_handoff_on_exit,
    recon_scene_guardian_on_exit,
    recon_scene_guardian_sus_on_exit,
    recon_scene_locator_on_exit,
    recon_scene_locator_home_on_exit,
};

const SceneManagerHandlers recon_scene_handlers = {
    .on_enter_handlers = recon_scene_on_enter_handlers,
    .on_event_handlers = recon_scene_on_event_handlers,
    .on_exit_handlers = recon_scene_on_exit_handlers,
    .scene_num = ReconSceneNum,
};
