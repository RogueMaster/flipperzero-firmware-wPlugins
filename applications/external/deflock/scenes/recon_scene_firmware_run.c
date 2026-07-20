// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_flasher.h"

#include <string.h>

static void fw_log_cb(void* ctx, const char* line) {
    ReconApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    furi_string_cat_printf(app->fw_log, "%s\n", line);
    app->fw_log_dirty = true;
    furi_mutex_release(app->mutex);
}

static int32_t fw_worker(void* context) {
    ReconApp* app = context;
    EspFlasher* fl = esp_flasher_alloc((FuriHalSerialId)app->settings.esp_uart, fw_log_cb, app);
    bool ok = false;
    if(!fl) {
        fw_log_cb(app, "UART busy.");
    } else {
        // Both flash (write) and backup (read) talk to the raw ROM loader -- no
        // stub is ever uploaded (the 0xchocolate approach), so the "loader
        // resident / overlapping address" error can't occur. Backup forces Safe
        // baud (ROM reads are slow + integrity matters); flash allows the user's
        // Fast (230400) and verifies the write afterwards.
        uint32_t fast = (app->fw_op == 0 || !app->settings.flash_fast) ? 0 : 230400;
        if(esp_flasher_connect(fl, fast)) {
            if(app->fw_op == 0) {
                ok = esp_flasher_backup(fl, app->storage, app->fw_path);
            } else {
                ok = esp_flasher_flash_file(fl, app->storage, app->fw_path, 0);
            }
        }
        esp_flasher_free(fl);
    }
    fw_log_cb(app, ok ? "== DONE ==" : "== FAILED ==");
    app->fw_ok = ok;
    app->fw_running = false;
    return 0;
}

// Render the tail (~last 7 lines) of the log so live progress stays visible.
static void fw_render(ReconApp* app) {
    widget_reset(app->widget);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const char* full = furi_string_get_cstr(app->fw_log);
    size_t len = strlen(full);
    const char* start = full;
    int nl = 0;
    for(int i = (int)len - 1; i >= 0; i--) {
        if(full[i] == '\n') {
            if(++nl >= 8) {
                start = full + i + 1;
                break;
            }
        }
    }
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, start);
    furi_mutex_release(app->mutex);
}

void recon_scene_firmware_run_on_enter(void* context) {
    ReconApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    furi_string_reset(app->fw_log);
    furi_mutex_release(app->mutex);

    fw_log_cb(app, app->fw_op == 0 ? "BACKUP firmware" : "FLASH firmware");
    fw_log_cb(app, "Put ESP in bootloader:");
    fw_log_cb(app, "hold BOOT, tap RESET.");
    fw_log_cb(app, "Working...");

    // The worker needs a chunk of heap (4 KB thread stack + UART buffer + the
    // esp-serial-flasher stub upload). A FAP shares the Flipper's ~256 KB RAM
    // with the firmware, so on a busy system this can come up short. Check up
    // front and fail with a message instead of letting an allocation abort the
    // whole app (the "out of memory" crash).
    if(memmgr_get_free_heap() < 10 * 1024) {
        fw_log_cb(app, "Not enough free RAM.");
        fw_log_cb(app, "Reboot the Flipper, open");
        fw_log_cb(app, "only FlipDeFlock, retry.");
        fw_log_cb(app, "== FAILED ==");
        app->fw_running = false;
        app->fw_ok = false;
        fw_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
        return;
    }

    app->fw_running = true;
    app->fw_ok = false;
    app->fw_log_dirty = false;
    app->fw_thread = furi_thread_alloc_ex("FlipDeFlockFlash", 4096, fw_worker, app);
    furi_thread_start(app->fw_thread);

    fw_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_firmware_run_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type == SceneManagerEventTypeTick) {
        // Only rebuild the widget when the log actually changed.
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool dirty = app->fw_log_dirty;
        app->fw_log_dirty = false;
        furi_mutex_release(app->mutex);
        if(dirty) fw_render(app);
        return true;
    }
    return false;
}

void recon_scene_firmware_run_on_exit(void* context) {
    ReconApp* app = context;
    if(app->fw_thread) {
        esp_flasher_abort(); // stop a long flash/backup so we don't block on join
        furi_thread_join(app->fw_thread);
        furi_thread_free(app->fw_thread);
        app->fw_thread = NULL;
    }
    widget_reset(app->widget);
}
