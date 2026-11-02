#include "helpers/scheduler_popup.h"
#include "src/scheduler_app_i.h"
#include "helpers/scheduler_playlist.h"

#include <gui/modules/validators.h>

#define PLAYLIST_SAVE_EVENT 100

static void playlist_name_callback(void* context) {
    SchedulerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PLAYLIST_SAVE_EVENT);
}

static bool playlist_save(SchedulerApp* app, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_simply_mkdir(storage, SCHEDULER_PLAYLIST_FOLDER)) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_NEW)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool ok = true;
    FuriString* line = furi_string_alloc();

    for(uint8_t i = 0; i < app->playlist_count; i++) {
        const char* item = furi_string_get_cstr(app->playlist_items[i]);
        if(!strncmp(item, "time:", 5))
            furi_string_printf(line, "%s\n", item);
        else
            furi_string_printf(line, "sub: %s\n", item);
        if(storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line)) !=
           furi_string_size(line)) {
            ok = false;
            break;
        }
    }

    if(!storage_file_close(file)) ok = false;
    storage_file_free(file);
    furi_string_free(line);
    if(!ok) storage_common_remove(storage, path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void playlist_activate_saved_file(SchedulerApp* app, FuriString* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    SchedulerPlaylistTiming timing;
    const bool valid =
        scheduler_playlist_read_timing(storage, furi_string_get_cstr(path), &timing);
    furi_record_close(RECORD_STORAGE);
    if(!valid) {
        dialog_message_show_storage_error(app->dialogs, "Invalid playlist");
        return;
    }

    furi_string_set(app->tx_file_path, path);
    scheduler_set_file(app->scheduler, furi_string_get_cstr(path), timing.count);
    scheduler_set_embedded_timing(app->scheduler, timing.has_timing);
    if(timing.has_timing) {
        scheduler_set_step_mode(app->scheduler, SchedulerStepModeOne);
        scheduler_set_schedule_type(app->scheduler, SchedulerSchedulePerLine);
        for(uint8_t i = 0; i < timing.count; ++i)
            scheduler_set_line_interval(app->scheduler, i, timing.wait_seconds[i]);
    }
    app->should_reset = false;
    snprintf(app->popup_msg, sizeof(app->popup_msg), "Saved %u signals", timing.count);
    scheduler_popup_success_show(app, app->popup_msg);
}

static void playlist_save_named(SchedulerApp* app) {
    if(!app->playlist_name_tmp[0] || !app->playlist_count) return;

    FuriString* path =
        furi_string_alloc_printf("%s/%s.txt", SCHEDULER_PLAYLIST_FOLDER, app->playlist_name_tmp);
    if(playlist_save(app, furi_string_get_cstr(path))) {
        playlist_activate_saved_file(app, path);
    } else {
        dialog_message_show_storage_error(app->dialogs, "Failed to save playlist");
    }
    furi_string_free(path);
}

void scheduler_scene_playlist_name_on_enter(void* context) {
    SchedulerApp* app = context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SCHEDULER_PLAYLIST_FOLDER);
    furi_record_close(RECORD_STORAGE);

    if(!app->playlist_name_tmp[0]) {
        snprintf(app->playlist_name_tmp, sizeof(app->playlist_name_tmp), "lights");
    }

    text_input_set_header_text(app->text_input, "Save playlist as");
    text_input_set_result_callback(
        app->text_input,
        playlist_name_callback,
        app,
        app->playlist_name_tmp,
        sizeof(app->playlist_name_tmp),
        false);

    ValidatorIsFile* validator =
        validator_is_file_alloc_init(SCHEDULER_PLAYLIST_FOLDER, ".txt", "");
    text_input_set_validator(app->text_input, validator_is_file_callback, validator);
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewTextInput);
}

bool scheduler_scene_playlist_name_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == PLAYLIST_SAVE_EVENT) {
        playlist_save_named(app);
        return true;
    }

    if(event.event == SchedulerEventPopupDone) {
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, SchedulerSceneStart);
        return true;
    }
    return false;
}

void scheduler_scene_playlist_name_on_exit(void* context) {
    SchedulerApp* app = context;
    void* validator = text_input_get_validator_callback_context(app->text_input);
    text_input_set_validator(app->text_input, NULL, NULL);
    if(validator) validator_is_file_free(validator);
    text_input_reset(app->text_input);
}
