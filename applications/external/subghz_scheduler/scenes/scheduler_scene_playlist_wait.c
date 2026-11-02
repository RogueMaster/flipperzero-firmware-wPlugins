#include "src/scheduler_app_i.h"
#include "helpers/scheduler_playlist.h"

#include <limits.h>

#define PLAYLIST_WAIT_DONE 100

static void playlist_wait_callback(void* context) {
    SchedulerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PLAYLIST_WAIT_DONE);
}

void scheduler_scene_playlist_wait_on_enter(void* context) {
    SchedulerApp* app = context;
    if(!app->playlist_wait_tmp[0])
        snprintf(app->playlist_wait_tmp, sizeof(app->playlist_wait_tmp), "10");
    text_input_set_header_text(app->text_input, "Wait before next signal (s)");
    text_input_set_result_callback(
        app->text_input,
        playlist_wait_callback,
        app,
        app->playlist_wait_tmp,
        sizeof(app->playlist_wait_tmp),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewTextInput);
}

bool scheduler_scene_playlist_wait_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != PLAYLIST_WAIT_DONE)
        return false;
    const char* digits = app->playlist_wait_tmp;
    if(!*digits) return true;
    uint32_t seconds = 0;
    for(; *digits; digits++) {
        if(*digits < '0' || *digits > '9' || seconds > (UINT32_MAX - (*digits - '0')) / 10) {
            dialog_message_show_storage_error(app->dialogs, "Enter seconds as digits");
            return true;
        }
        seconds = seconds * 10 + (*digits - '0');
    }
    if(seconds > SCHEDULER_PLAYLIST_MAX_WAIT_SECONDS) {
        dialog_message_show_storage_error(app->dialogs, "Maximum wait is 99:59:59");
        return true;
    }
    if(app->playlist_count < SCHEDULER_PLAYLIST_MAX_ITEMS) {
        app->playlist_items[app->playlist_count++] =
            furi_string_alloc_printf("time: %lu", (unsigned long)seconds);
    }
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void scheduler_scene_playlist_wait_on_exit(void* context) {
    SchedulerApp* app = context;
    text_input_reset(app->text_input);
}
