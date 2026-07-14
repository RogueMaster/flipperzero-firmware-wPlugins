#include "helpers/scheduler_popup.h"
#include "helpers/scheduler_settings_io.h"
#include "src/scheduler_app_i.h"

#define TAG "SubGHzSchedulerLoadSchedule"

static bool scheduler_load_schedule_dialog_and_apply(
    SchedulerApp* app,
    char* loaded_name,
    size_t loaded_name_sz) {
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".sch", &I_sub1_10px);
    browser_options.base_path = SCHEDULER_APP_FOLDER;

    FuriString* settings_path = furi_string_alloc_set_str(SCHEDULER_APP_FOLDER);

    bool ok =
        dialog_file_browser_show(app->dialogs, settings_path, settings_path, &browser_options);

    if(ok) {
        const char* full = furi_string_get_cstr(settings_path);
        snprintf(loaded_name, loaded_name_sz, "Loaded:\n%s", path_basename(full));

        ok = scheduler_settings_load_from_path(app, full);
        if(!ok) {
            dialog_message_show_storage_error(app->dialogs, "Wrong file format\nor corrupt file!");
        }
    }

    furi_string_free(settings_path);
    return ok;
}

void scheduler_scene_loadschedule_on_enter(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;

    if(scheduler_load_schedule_dialog_and_apply(app, app->popup_msg, sizeof(app->popup_msg))) {
        scheduler_popup_success_show(app, app->popup_msg);
        return;
    }

    scene_manager_previous_scene(app->scene_manager);
}

bool scheduler_scene_loadschedule_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SchedulerEventPopupDone) {
            scene_manager_set_scene_state(app->scene_manager, SchedulerSceneStart, 0);
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, SchedulerSceneStart);
            return true;
        }
    }
    return false;
}

void scheduler_scene_loadschedule_on_exit(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;
    scene_manager_set_scene_state(app->scene_manager, SchedulerSceneStart, 0);
}
