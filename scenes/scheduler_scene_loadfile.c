#include "scheduler_scene_loadfile.h"
#include "helpers/scheduler_popup.h"
#include "helpers/scheduler_playlist.h"
#include "src/scheduler_app_i.h"
#include "views/scheduler_start_view_settings.h"

#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <devices/cc1101_int/cc1101_int_interconnect.h>
#include <devices/devices.h>
#include <flipper_format/flipper_format_i.h>
#include <furi_hal.h>
#include <types.h>

#include <stdio.h>
#include <string.h>

#define TAG "SubGHzSchedulerLoadFile"

bool check_file_extension(const char* filename) {
    const char* extension = strrchr(filename, '.');
    return extension && (!strcmp(extension, ".txt") || !strcmp(extension, ".sub"));
}

static void scheduler_show_file_error(
    SchedulerApp* app,
    bool is_playlist,
    uint16_t line,
    const char* reason,
    uint32_t frequency,
    const char* file_path) {
    const char* name = strrchr(file_path, '/');
    name = name ? name + 1 : file_path;

    const char* type = is_playlist ? "Playlist" : "Sub-GHz File";
    const uint32_t mhz100 = frequency / 10000UL;

    char prefix[20] = "";
    if(is_playlist && line) {
        snprintf(prefix, sizeof(prefix), "Line %u: ", (unsigned)line);
    }

    snprintf(app->popup_heading, sizeof(app->popup_heading), "%s Error", type);

    snprintf(
        app->popup_msg,
        sizeof(app->popup_msg),
        "%s%s\n%lu.%02lu MHz\n%.24s",
        prefix,
        reason,
        (unsigned long)(mhz100 / 100),
        (unsigned long)(mhz100 % 100),
        name);

    FURI_LOG_E(
        TAG,
        "%s rejected (line %u): %s; frequency=%lu Hz; file=%s",
        type,
        (unsigned)line,
        reason,
        (unsigned long)frequency,
        file_path);
}

// Check one .sub file or every 'sub:' entry in a playlist for valid
//  and allowed frequencies at load.
static bool
    scheduler_validate_frequency_files(SchedulerApp* app, const char* file_path, int count) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* playlist = flipper_format_file_alloc(storage);
    FlipperFormat* sub_file = flipper_format_file_alloc(storage);
    FuriString* sub_path = furi_string_alloc();

    const char* error = NULL;
    uint16_t bad_line = 0;
    uint32_t bad_frequency = 0;
    bool playlist_open = false;

    if(count > 0) {
        playlist_open = flipper_format_file_open_existing(playlist, file_path);
        if(!playlist_open) {
            error = "Cannot open playlist";
        }
    }

    if(!error) {
        subghz_devices_init();

        const char* radio = scheduler_get_radio(app->scheduler) ? SUBGHZ_DEVICE_CC1101_EXT_NAME :
                                                                  SUBGHZ_DEVICE_CC1101_INT_NAME;
        const SubGhzDevice* device = subghz_devices_get_by_name(radio);

        for(int i = 0; i < (count > 0 ? count : 1); ++i) {
            bad_line = count > 0 ? (uint16_t)i + 1 : 0;
            bad_frequency = 0;

            if(count > 0) {
                furi_string_set_str(sub_path, "");
                if(!flipper_format_read_string(playlist, "sub", sub_path)) {
                    error = "Missing entry";
                    break;
                }
            } else {
                furi_string_set_str(sub_path, file_path);
            }

            if(!flipper_format_file_open_existing(sub_file, furi_string_get_cstr(sub_path))) {
                error = "Missing .sub";
                break;
            }

            flipper_format_read_uint32(sub_file, "Frequency", &bad_frequency, 1);
            flipper_format_file_close(sub_file);

            if(!subghz_devices_is_frequency_valid(device, bad_frequency)) {
                error = "Invalid Freq";
                break;
            }
            if(!furi_hal_region_is_frequency_allowed(bad_frequency)) {
                error = "TX Region Blocked";
                break;
            }
        }
    }

    if(error) {
        scheduler_show_file_error(
            app,
            count > 0,
            bad_line,
            error,
            bad_frequency,
            furi_string_empty(sub_path) ? file_path : furi_string_get_cstr(sub_path));
    }

    if(playlist_open) flipper_format_file_close(playlist);

    subghz_devices_deinit();
    flipper_format_free(sub_file);
    flipper_format_free(playlist);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(sub_path);
    return error == NULL;
}

static bool load_protocol_from_file(SchedulerApp* app) {
    furi_assert(app);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".sub|.txt", &I_sub1_10px);
    browser_options.skip_assets = true;
    browser_options.hide_ext = false;
    browser_options.base_path = SUBGHZ_APP_FOLDER;

    FuriString* path = furi_string_alloc_set(app->tx_file_path);
    if(furi_string_empty(path)) {
        furi_string_set(path, SUBGHZ_APP_FOLDER);
    }

    // Input events and views are managed by file_select
    bool ok = dialog_file_browser_show(app->dialogs, path, path, &browser_options);

    if(ok) {
        const char* filestr = furi_string_get_cstr(path);
        const char* ext = strrchr(filestr, '.');
        int list_count = 0;

        ok = false;

        // Count playlist entries first, then check every referenced .sub file.
        if(ext && strcmp(ext, ".txt") == 0) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            list_count = scheduler_playlist_count(storage, filestr);
            furi_record_close(RECORD_STORAGE);

            if(list_count <= 0) {
                dialog_message_show_storage_error(
                    app->dialogs, "Playlist is empty\nor unreadable!");
            } else if(list_count > SCHEDULER_MAX_LINES) {
                dialog_message_show_storage_error(
                    app->dialogs, "Playlist has more\nthan 127 lines!");
            } else {
                //  reject the whole playlist before changing app state
                ok = scheduler_validate_frequency_files(app, filestr, list_count);
            }
        } else if(ext && strcmp(ext, ".sub") == 0) {
            ok = scheduler_validate_frequency_files(app, filestr, 0);
        }

        if(ok) {
            furi_string_set(app->tx_file_path, path);
            scheduler_set_file(app->scheduler, filestr, (int8_t)list_count);

            // set  popup 'success!' message on load
            const char* filename = strrchr(filestr, '/');
            filename = filename ? filename + 1 : filestr;
            snprintf(app->popup_msg, sizeof(app->popup_msg), "Loaded:\n%s", filename);
        }
    }

    furi_string_free(path);
    return ok;
}

void scheduler_scene_load_on_enter(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;
    if(load_protocol_from_file(app)) {
        scheduler_popup_success_show(app, app->popup_msg);
    } else {
        app->should_reset = true;
        scheduler_popup_error_show(app, app->popup_heading, app->popup_msg);
    }
}

void scheduler_scene_load_on_exit(void* context) {
    furi_assert(context);
    UNUSED(context);
}

bool scheduler_scene_load_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    SchedulerApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == SchedulerEventPopupDone) {
        scene_manager_set_scene_state(
            app->scene_manager, SchedulerSceneStart, MenuIndexStartSchedule);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SchedulerSceneStart);
        return true;
    }

    return false;
}
