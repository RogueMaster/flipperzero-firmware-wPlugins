#include "wol_flipper.h"

#include <furi_hal_rtc.h>
#include <storage/storage.h>

static const char* const wol_flasher_stage_text[] = {
    [WolFlasherStageConnect] = "Connecting",
    [WolFlasherStageErase] = "Erasing",
    [WolFlasherStageWrite] = "Writing",
    [WolFlasherStageRead] = "Reading",
    [WolFlasherStageVerify] = "Verifying",
};

static char flasher_header[24];

static void wol_flasher_progress(void* context, WolFlasherStage stage, uint8_t percent) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WOL_EVENT_FLASH(stage, percent));
}

/** /ext/apps_data/wol_flipper/backup/esp-YYYYMMDD-HHMMSS.bin */
static void wol_flasher_build_backup_path(WolApp* app) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    furi_string_printf(
        app->flasher_path,
        "%s/esp-%04u%02u%02u-%02u%02u%02u.bin",
        WOL_BACKUP_DIR,
        now.year,
        now.month,
        now.day,
        now.hour,
        now.minute,
        now.second);
}

static const struct {
    const char* name;
    uint32_t address;
} wol_firmware_parts[] = {
    {"bootloader.bin", WOL_FW_BOOTLOADER_ADDR},
    {"partitions.bin", WOL_FW_PARTITIONS_ADDR},
    {"firmware.bin", WOL_FW_APP_ADDR},
};

static WolFlasherResult wol_flasher_do_flash_firmware(WolApp* app, WolFlasher* flasher) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* paths[COUNT_OF(wol_firmware_parts)];
    WolFlasherImage images[COUNT_OF(wol_firmware_parts)];
    WolFlasherResult result;
    bool complete = true;

    for(size_t i = 0; i < COUNT_OF(wol_firmware_parts); i++) {
        paths[i] = furi_string_alloc();

        furi_string_printf(paths[i], WOL_FW_DATA_PATH, wol_firmware_parts[i].name);
        if(!storage_file_exists(storage, furi_string_get_cstr(paths[i]))) {
            furi_string_printf(paths[i], WOL_FW_ASSETS_PATH, wol_firmware_parts[i].name);
            if(!storage_file_exists(storage, furi_string_get_cstr(paths[i]))) complete = false;
        }

        images[i].path = furi_string_get_cstr(paths[i]);
        images[i].address = wol_firmware_parts[i].address;
    }
    furi_record_close(RECORD_STORAGE);

    if(complete) {
        result = wol_flasher_write_images(flasher, images, COUNT_OF(wol_firmware_parts));
    } else {
        snprintf(
            app->worker_info,
            sizeof(app->worker_info),
            "Firmware images missing.\nReinstall the app");
        result = WolFlasherErrFile;
    }

    for(size_t i = 0; i < COUNT_OF(wol_firmware_parts); i++) {
        furi_string_free(paths[i]);
    }
    return result;
}

static int32_t wol_flasher_worker(void* context) {
    WolApp* app = context;
    WolFlasher* flasher = wol_flasher_alloc(&app->worker_cancel);
    WolFlasherResult result;

    app->worker_info[0] = '\0';
    wol_flasher_set_progress_callback(flasher, wol_flasher_progress, app);

    wol_app_ensure_power(app);
    result = wol_flasher_connect(flasher);

    if(result == WolFlasherErrNoBoard) {
        // the auto reset lines are only on the official board
        snprintf(
            app->worker_info, sizeof(app->worker_info), "Hold BOOT, tap RESET,\nthen try again");
    }

    if(result == WolFlasherOk) {
        switch(app->flasher_op) {
        case WolFlasherOpInfo:
            snprintf(
                app->worker_info,
                sizeof(app->worker_info),
                "%s, %lu MB flash\n%s, %lu baud",
                wol_flasher_get_chip_name(flasher),
                (unsigned long)(wol_flasher_get_flash_size(flasher) / (1024 * 1024)),
                wol_flasher_is_stub_running(flasher) ? "stub" : "rom",
                (unsigned long)wol_flasher_get_transmission_rate(flasher));
            break;

        case WolFlasherOpFlashFirmware:
            result = wol_flasher_do_flash_firmware(app, flasher);
            if(result == WolFlasherOk) {
                wol_flasher_reset_target(flasher);
                snprintf(app->worker_info, sizeof(app->worker_info), "Board reset");
            }
            break;

        case WolFlasherOpBackup: {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_common_mkdir(storage, WOL_APP_DIR);
            storage_common_mkdir(storage, WOL_BACKUP_DIR);
            furi_record_close(RECORD_STORAGE);

            wol_flasher_build_backup_path(app);
            result = wol_flasher_backup(flasher, furi_string_get_cstr(app->flasher_path));
            if(result == WolFlasherOk) {
                snprintf(
                    app->worker_info,
                    sizeof(app->worker_info),
                    "%s",
                    strrchr(furi_string_get_cstr(app->flasher_path), '/') + 1);
            }
            break;
        }

        case WolFlasherOpRestore: {
            const WolFlasherImage image = {furi_string_get_cstr(app->flasher_path), 0};
            result = wol_flasher_write_images(flasher, &image, 1);
            if(result == WolFlasherOk) {
                wol_flasher_reset_target(flasher);
                snprintf(app->worker_info, sizeof(app->worker_info), "Board reset");
            }
            break;
        }
        }
    }

    wol_flasher_disconnect(flasher);
    wol_flasher_free(flasher);

    view_dispatcher_send_custom_event(app->view_dispatcher, WOL_EVENT_FLASH_DONE(result));
    return 0;
}

void wol_scene_flasher_on_enter(void* context) {
    WolApp* app = context;

    switch(app->flasher_op) {
    case WolFlasherOpInfo:
        wol_strcpy(flasher_header, sizeof(flasher_header), "Board info");
        break;
    case WolFlasherOpFlashFirmware:
        wol_strcpy(flasher_header, sizeof(flasher_header), "Flashing WoL fw");
        break;
    case WolFlasherOpBackup:
        wol_strcpy(flasher_header, sizeof(flasher_header), "Backing up");
        break;
    case WolFlasherOpRestore:
        wol_strcpy(flasher_header, sizeof(flasher_header), "Restoring");
        break;
    }

    wol_strcpy(app->status_text, sizeof(app->status_text), "Connecting...");

    popup_reset(app->popup);
    popup_set_header(app->popup, flasher_header, 64, 8, AlignCenter, AlignTop);
    popup_set_text(app->popup, app->status_text, 64, 28, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewPopup);

    app->worker_cancel = false;
    app->worker = furi_thread_alloc_ex("WolFlashWorker", 4096, wol_flasher_worker, app);
    furi_thread_start(app->worker);
}

bool wol_scene_flasher_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event >= WOL_EVENT_FLASH_DONE_BASE) {
        WolFlasherResult result = event.event - WOL_EVENT_FLASH_DONE_BASE;

        if(app->worker_info[0] != '\0') {
            snprintf(
                app->status_text,
                sizeof(app->status_text),
                "%s\n%s",
                wol_flasher_result_text(result),
                app->worker_info);
        } else {
            wol_strcpy(
                app->status_text, sizeof(app->status_text), wol_flasher_result_text(result));
        }

        popup_set_text(app->popup, app->status_text, 64, 26, AlignCenter, AlignTop);
        notification_message(
            app->notifications, result == WolFlasherOk ? &sequence_success : &sequence_error);
        return true;
    }

    if(event.event >= WOL_EVENT_FLASH_BASE) {
        uint32_t payload = event.event - WOL_EVENT_FLASH_BASE;
        uint32_t stage = payload >> 8;
        uint32_t percent = payload & 0xFF;

        if(stage >= COUNT_OF(wol_flasher_stage_text)) return false;

        snprintf(
            app->status_text,
            sizeof(app->status_text),
            "%s %lu%%",
            wol_flasher_stage_text[stage],
            (unsigned long)percent);
        popup_set_text(app->popup, app->status_text, 64, 28, AlignCenter, AlignTop);
        return true;
    }

    return false;
}

void wol_scene_flasher_on_exit(void* context) {
    WolApp* app = context;

    app->worker_cancel = true;
    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    popup_reset(app->popup);
}
