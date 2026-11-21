#include "../../rf_studio_i.h"

/* Confirmation before destructive operations (REM-003, SIG-010, AC-014). */

static void rfs_confirm_callback(DialogExResult result, void* context) {
    RfStudioApp* app = context;
    if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventDialogLeft);
    } else if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventDialogRight);
    }
}

void rfs_scene_confirm_on_enter(void* context) {
    RfStudioApp* app = context;
    DialogEx* dialog = app->dialog;
    dialog_ex_reset(dialog);
    furi_string_reset(app->text_store);
    const char* header = "Delete?";

    switch(app->confirm_purpose) {
    case RfsConfirmDeleteRemote:
        header = "Delete Remote?";
        furi_string_printf(app->text_store, "%s\nSignals stay in the library.", app->remote.name);
        break;
    case RfsConfirmDeleteButton:
        header = "Delete Button?";
        if(app->button_index >= 0 && (size_t)app->button_index < app->remote.count) {
            furi_string_printf(
                app->text_store,
                "%s\nIts signal stays in the library.",
                app->remote.buttons[app->button_index].name);
        }
        break;
    case RfsConfirmDeleteSignal: {
        header = "Delete Signal?";
        RfsSignalMeta* meta = rfs_signal_find(&app->signals, app->signal_id);
        size_t uses = rfs_remote_signal_usage(app->storage, app->signal_id, NULL);
        if(uses > 0) {
            furi_string_printf(
                app->text_store,
                "%s is used by %u button%s.\nThey will show as missing.",
                meta ? meta->name : "",
                (unsigned)uses,
                uses == 1 ? "" : "s");
        } else {
            furi_string_printf(
                app->text_store, "%s\nThis can't be undone.", meta ? meta->name : "");
        }
        break;
    }
    }

    dialog_ex_set_header(dialog, header, 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(
        dialog, furi_string_get_cstr(app->text_store), 64, 14, AlignCenter, AlignTop);
    dialog_ex_set_left_button_text(dialog, "Cancel");
    dialog_ex_set_right_button_text(dialog, "Delete");
    dialog_ex_set_context(dialog, app);
    dialog_ex_set_result_callback(dialog, rfs_confirm_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewDialog);
}

bool rfs_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == RfsEventDialogLeft) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(event.event != RfsEventDialogRight) return false;

    switch(app->confirm_purpose) {
    case RfsConfirmDeleteRemote: {
        if(rfs_remote_delete(app->storage, app->remote.id)) {
            rfs_remote_clear(&app->remote);
            const uint32_t targets[] = {RfsSceneRemoteList, RfsSceneStart};
            scene_manager_search_and_switch_to_previous_scene_one_of(
                app->scene_manager, targets, COUNT_OF(targets));
        } else {
            rfs_app_message(app, "Not Deleted", "Could not delete the remote.", RfsSceneNum);
        }
        break;
    }
    case RfsConfirmDeleteButton: {
        if(app->button_index >= 0 && (size_t)app->button_index < app->remote.count) {
            rfs_remote_remove_button(&app->remote, app->button_index);
            if(app->button_index > 0 && (size_t)app->button_index >= app->remote.count) {
                app->button_index = app->remote.count - 1;
            }
            if(!rfs_app_save_remote(app)) {
                rfs_app_message(app, "Not Saved", "Could not save the remote.", RfsSceneNum);
                break;
            }
        }
        const uint32_t targets[] = {RfsSceneRemoteEditor, RfsSceneRemoteMenu, RfsSceneRemoteRun};
        scene_manager_search_and_switch_to_previous_scene_one_of(
            app->scene_manager, targets, COUNT_OF(targets));
        break;
    }
    case RfsConfirmDeleteSignal: {
        if(rfs_signal_delete(app->storage, &app->signals, app->signal_id)) {
            const uint32_t targets[] = {RfsSceneSignalList, RfsSceneStart};
            scene_manager_search_and_switch_to_previous_scene_one_of(
                app->scene_manager, targets, COUNT_OF(targets));
        } else {
            rfs_app_message(app, "Not Deleted", "Could not delete the signal.", RfsSceneNum);
        }
        break;
    }
    }
    return true;
}

void rfs_scene_confirm_on_exit(void* context) {
    RfStudioApp* app = context;
    dialog_ex_reset(app->dialog);
}
