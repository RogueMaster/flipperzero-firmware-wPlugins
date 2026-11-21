#include "../../rf_studio_i.h"

static void rfs_text_input_callback(void* context) {
    RfStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventTextDone);
}

static const char* rfs_text_header(RfsTextPurpose purpose) {
    switch(purpose) {
    case RfsTextRemoteNew:
        return "Name the new remote";
    case RfsTextRemoteRename:
        return "Rename remote";
    case RfsTextButtonNew:
        return "Name this button";
    case RfsTextButtonRename:
        return "Rename button";
    case RfsTextSignalNew:
        return "Name this signal";
    case RfsTextSignalRename:
        return "Rename signal";
    default:
        return "Search remotes (empty = all)";
    }
}

void rfs_scene_text_input_on_enter(void* context) {
    RfStudioApp* app = context;
    TextInput* text_input = app->text_input;
    text_input_reset(text_input);
    text_input_set_header_text(text_input, rfs_text_header(app->text_purpose));
    text_input_set_minimum_length(text_input, app->text_purpose == RfsTextSearch ? 0 : 1);
    text_input_set_result_callback(
        text_input,
        rfs_text_input_callback,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewTextInput);
}

static void rfs_text_open_new_remote(RfStudioApp* app) {
    rfs_remote_new(app->storage, &app->remote, app->text_buffer);
    if(!rfs_app_save_remote(app)) {
        rfs_app_message(
            app, "Not Saved", "Could not create the remote.\nCheck the SD card.", RfsSceneNum);
        return;
    }
    /* Stack becomes Start > RemoteRun > RemoteEditor so Done lands on the new remote. */
    app->button_index = 0;
    scene_manager_search_and_switch_to_another_scene(app->scene_manager, RfsSceneRemoteRun);
    scene_manager_set_scene_state(app->scene_manager, RfsSceneRemoteEditor, 0);
    scene_manager_next_scene(app->scene_manager, RfsSceneRemoteEditor);
}

bool rfs_scene_text_input_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != RfsEventTextDone) return false;

    switch(app->text_purpose) {
    case RfsTextRemoteNew:
        rfs_text_open_new_remote(app);
        break;
    case RfsTextRemoteRename:
        strlcpy(app->remote.name, app->text_buffer, sizeof(app->remote.name));
        if(rfs_app_save_remote(app)) {
            scene_manager_previous_scene(app->scene_manager);
        } else {
            rfs_app_message(app, "Not Saved", "Could not save the remote.", RfsSceneNum);
        }
        break;
    case RfsTextButtonNew:
        rfs_app_finish_button_capture(app, app->text_buffer);
        break;
    case RfsTextButtonRename:
        if(app->button_index >= 0 && (size_t)app->button_index < app->remote.count) {
            RfsButton* button = &app->remote.buttons[app->button_index];
            strlcpy(button->name, app->text_buffer, sizeof(button->name));
            button->updated = rfs_storage_now();
            if(!rfs_app_save_remote(app)) {
                rfs_app_message(app, "Not Saved", "Could not save the remote.", RfsSceneNum);
                break;
            }
        }
        scene_manager_previous_scene(app->scene_manager);
        break;
    case RfsTextSignalNew: {
        char id[RFS_ID_LEN];
        if(rfs_app_store_capture(app, app->text_buffer, id)) {
            notification_message(app->notifications, &sequence_success);
            rfs_app_exit_capture(app);
        } else {
            rfs_app_message(
                app, "Not Saved", "Could not save the signal.\nCheck the SD card.", RfsSceneNum);
        }
        break;
    }
    case RfsTextSignalRename: {
        RfsSignalMeta* meta = rfs_signal_find(&app->signals, app->signal_id);
        if(meta) {
            strlcpy(meta->name, app->text_buffer, sizeof(meta->name));
            if(!rfs_app_save_signals(app)) {
                rfs_app_message(app, "Not Saved", "Could not save the library.", RfsSceneNum);
                break;
            }
        }
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
    case RfsTextSearch:
        strlcpy(app->search, app->text_buffer, sizeof(app->search));
        scene_manager_set_scene_state(app->scene_manager, RfsSceneRemoteList, 0);
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
    return true;
}

void rfs_scene_text_input_on_exit(void* context) {
    RfStudioApp* app = context;
    text_input_reset(app->text_input);
}
