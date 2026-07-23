// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

#include <dialogs/dialogs.h>

typedef enum {
    FwItemBackup,
    FwItemFlash,
} FwItem;

static void fw_submenu_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void recon_scene_firmware_on_enter(void* context) {
    ReconApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "ESP32 Firmware");
    submenu_add_item(submenu, "Backup current FW -> SD", FwItemBackup, fw_submenu_cb, app);
    submenu_add_item(submenu, "Flash a .bin", FwItemFlash, fw_submenu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool recon_scene_firmware_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == FwItemBackup) {
        app->fw_op = 0;
        storage_common_mkdir(app->storage, RECON_APP_FOLDER);
        storage_common_mkdir(app->storage, RECON_APP_FOLDER "/firmware");
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        snprintf(
            app->fw_path,
            sizeof(app->fw_path),
            RECON_APP_FOLDER "/firmware/backup_%04u%02u%02u_%02u%02u%02u.bin",
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second);
        scene_manager_next_scene(app->scene_manager, ReconSceneFirmwareRun);
        return true;
    }

    if(event.event == FwItemFlash) {
        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        FuriString* path = furi_string_alloc_set(EXT_PATH("apps_data"));
        DialogsFileBrowserOptions opts;
        dialog_file_browser_set_basic_options(&opts, ".bin", NULL);
        opts.base_path = EXT_PATH("");
        bool ok = dialog_file_browser_show(dialogs, path, path, &opts);
        furi_record_close(RECORD_DIALOGS);
        if(ok) {
            strncpy(app->fw_path, furi_string_get_cstr(path), sizeof(app->fw_path) - 1);
            app->fw_path[sizeof(app->fw_path) - 1] = '\0';
            app->fw_op = 1;
            scene_manager_next_scene(app->scene_manager, ReconSceneFirmwareRun);
        }
        furi_string_free(path);
        return true;
    }
    return false;
}

void recon_scene_firmware_on_exit(void* context) {
    ReconApp* app = context;
    submenu_reset(app->submenu);
}
