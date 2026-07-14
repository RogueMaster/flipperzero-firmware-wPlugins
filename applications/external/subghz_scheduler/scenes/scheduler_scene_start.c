#include "scheduler_scene_loadfile.h"
#include "src/scheduler_app_i.h"
#include "views/scheduler_start_view_settings.h"
#include "views/scheduler_start_view.h"

#define TAG "SubGHzSchedulerSceneStart"

void scheduler_scene_start_on_enter(void* context) {
    SchedulerApp* app = context;
    furi_assert(app);

    scheduler_time_reset(app->scheduler);
    if(app->should_reset) {
        scheduler_full_reset(app->scheduler);
        furi_string_reset(app->tx_file_path);
        app->should_reset = false;
    }

    if(!furi_string_empty(app->tx_file_path)) {
        if(check_file_extension(furi_string_get_cstr(app->tx_file_path))) {
            scene_manager_set_scene_state(
                app->scene_manager, SchedulerSceneStart, MenuIndexStartSchedule);
        }
    }

    scheduler_start_view_rebuild(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewVarItemList);
}

bool scheduler_scene_start_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    furi_assert(app);

    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case SchedulerStartRunEvent:
        if(furi_string_empty(app->tx_file_path)) {
            dialog_message_show_storage_error(
                app->dialogs, "Please select\nplaylist (*.txt) or\n *.sub file!");
        } else {
            scene_manager_next_scene(app->scene_manager, SchedulerSceneRunSchedule);
        }
        return true;

    case SchedulerStartEventSelectFile:
        scene_manager_next_scene(app->scene_manager, SchedulerSceneLoadFile);
        return true;

    case SchedulerStartEventSaveSchedule:
        scene_manager_next_scene(app->scene_manager, SchedulerSceneSaveName);
        return true;

    case SchedulerStartEventSetInterval:
        scene_manager_next_scene(app->scene_manager, SchedulerSceneInterval);
        return true;

    default:
        return false;
    }
}

void scheduler_scene_start_on_exit(void* context) {
    SchedulerApp* app = context;
    furi_assert(app);

    scheduler_start_view_reset(app);
}
