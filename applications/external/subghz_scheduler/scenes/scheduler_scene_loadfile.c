#include "scheduler_scene_loadfile.h"
#include "helpers/scheduler_popup.h"
#include "helpers/scheduler_playlist.h"
#include "src/scheduler_app_i.h"
#include "views/scheduler_start_view_settings.h"

#include <flipper_format/flipper_format_i.h>
#include <types.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <devices/cc1101_int/cc1101_int_interconnect.h>
#include <devices/devices.h>
#include <furi_hal.h>
#include <stdio.h>

#define TAG "SubGHzSchedulerLoadFile"

bool check_file_extension(const char* filename) {
    const char* extension = strrchr(filename, '.');
    return extension && (!strcmp(extension, ".txt") || !strcmp(extension, ".sub"));
}

// Validate before committing the selected path or embedded playlist timing.
static bool
    scheduler_validate_frequency_files(SchedulerApp* app, const char* file_path, int count) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* playlist = flipper_format_file_alloc(storage);
    FlipperFormat* sub_file = flipper_format_file_alloc(storage);
    FuriString* sub_path = furi_string_alloc();
    const char* error = NULL;
    uint32_t frequency = 0;
    unsigned line = 0;
    bool playlist_open = false;
    bool devices_initialized = false;

    if(count > 0) {
        playlist_open = flipper_format_file_open_existing(playlist, file_path);
        if(!playlist_open) error = "Cannot open playlist";
    }
    if(!error) {
        subghz_devices_init();
        devices_initialized = true;
        const char* radio = scheduler_get_radio(app->scheduler) ? SUBGHZ_DEVICE_CC1101_EXT_NAME :
                                                                  SUBGHZ_DEVICE_CC1101_INT_NAME;
        const SubGhzDevice* device = subghz_devices_get_by_name(radio);
        if(!device) error = "Radio unavailable";

        for(int i = 0; !error && i < (count > 0 ? count : 1); ++i) {
            line = count > 0 ? (unsigned)i + 1 : 0;
            frequency = 0;
            if(count > 0) {
                furi_string_reset(sub_path);
                if(!flipper_format_read_string(playlist, "sub", sub_path)) {
                    error = "Missing playlist entry";
                    break;
                }
            } else {
                furi_string_set_str(sub_path, file_path);
            }
            if(!flipper_format_file_open_existing(sub_file, furi_string_get_cstr(sub_path))) {
                error = "Cannot open .sub file";
                break;
            }
            bool has_frequency = flipper_format_read_uint32(sub_file, "Frequency", &frequency, 1);
            flipper_format_file_close(sub_file);
            if(!has_frequency) {
                error = "Missing Frequency";
            } else if(!subghz_devices_is_frequency_valid(device, frequency)) {
                error = "Invalid frequency";
            } else if(!furi_hal_region_is_frequency_allowed(frequency)) {
                error = "TX region blocked";
            }
        }
    }
    if(error) {
        const char* file = furi_string_empty(sub_path) ? file_path :
                                                         furi_string_get_cstr(sub_path);
        const char* name = strrchr(file, '/');
        name = name ? name + 1 : file;
        char prefix[20] = "";
        if(count > 0 && line) snprintf(prefix, sizeof(prefix), "Line %u: ", line);
        const uint32_t mhz100 = frequency / 10000UL;
        snprintf(
            app->popup_heading,
            sizeof(app->popup_heading),
            "%s Error",
            count > 0 ? "Playlist" : "Sub-GHz File");
        snprintf(
            app->popup_msg,
            sizeof(app->popup_msg),
            "%s%s\n%lu.%02lu MHz\n%.24s",
            prefix,
            error,
            (unsigned long)(mhz100 / 100),
            (unsigned long)(mhz100 % 100),
            name);
        FURI_LOG_E(
            TAG,
            "Load rejected (line %u): %s; frequency=%lu Hz; file=%s",
            line,
            error,
            (unsigned long)frequency,
            file);
    }
    if(devices_initialized) subghz_devices_deinit();
    if(playlist_open) flipper_format_file_close(playlist);
    flipper_format_free(sub_file);
    flipper_format_free(playlist);
    furi_string_free(sub_path);
    furi_record_close(RECORD_STORAGE);

    return error == NULL;
}

static bool load_protocol_from_file(SchedulerApp* app) {
    furi_assert(app);
    app->popup_heading[0] = '\0';
    app->popup_msg[0] = '\0';

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
        const bool is_playlist = ext && strcmp(ext, ".txt") == 0;
        SchedulerPlaylistTiming timing = {0};
        int list_count = 0;

        // Only attempt count of TXT playlist files
        if(is_playlist) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            const bool valid = scheduler_playlist_read_timing(storage, filestr, &timing);
            list_count = valid ? timing.count : -1;
            furi_record_close(RECORD_STORAGE);

            if(list_count <= 0) {
                snprintf(app->popup_heading, sizeof(app->popup_heading), "Playlist Error");
                snprintf(
                    app->popup_msg, sizeof(app->popup_msg), "Playlist is empty\nor unreadable!");
                ok = false;
            } else if(list_count > SCHEDULER_MAX_LINES) {
                snprintf(app->popup_heading, sizeof(app->popup_heading), "Playlist Error");
                snprintf(
                    app->popup_msg, sizeof(app->popup_msg), "Playlist has more\nthan 127 lines!");
                ok = false;
            }
        }

        if(ok) ok = scheduler_validate_frequency_files(app, filestr, list_count);

        if(ok) {
            const char* filename = strrchr(filestr, '/');
            filename = filename ? filename + 1 : filestr;
            snprintf(app->popup_msg, sizeof(app->popup_msg), "Loaded:\n%.70s", filename);
            furi_string_set(app->tx_file_path, path);
            scheduler_set_file(app->scheduler, filestr, (int8_t)list_count);
            scheduler_set_embedded_timing(app->scheduler, is_playlist && timing.has_timing);
            if(is_playlist && timing.has_timing) {
                scheduler_set_step_mode(app->scheduler, SchedulerStepModeOne);
                scheduler_set_schedule_type(app->scheduler, SchedulerSchedulePerLine);
                for(uint8_t i = 0; i < timing.count; ++i)
                    scheduler_set_line_interval(app->scheduler, i, timing.wait_seconds[i]);
            }
        }
    }

    furi_string_free(path);
    return ok;
}

void scheduler_scene_load_on_enter(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;
    if(load_protocol_from_file(app)) {
        scene_manager_set_scene_state(
            app->scene_manager, SchedulerSceneStart, MenuIndexStartSchedule);
        scheduler_popup_success_show(app, app->popup_msg);
    } else if(app->popup_heading[0]) {
        scheduler_popup_error_show(app, app->popup_heading, app->popup_msg);
    } else {
        // A canceled browser is not an error and must not reset the current schedule.
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SchedulerSceneStart);
    }
}

void scheduler_scene_load_on_exit(void* context) {
    furi_assert(context);
    UNUSED(context);
}

bool scheduler_scene_load_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    SchedulerApp* app = context;
    if((event.type == SceneManagerEventTypeCustom && event.event == SchedulerEventPopupDone) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SchedulerSceneStart);
        return true;
    }
    return false;
}
