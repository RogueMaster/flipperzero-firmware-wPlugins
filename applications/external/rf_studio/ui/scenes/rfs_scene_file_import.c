#include "../../rf_studio_i.h"

/*
 * Import a .sub file into the Signal Library (SIG-001, SIG-012) or an exported remote (REM-009).
 * The file browser runs on entry; the outcome is shown by the message scene.
 */

#define IMPORT_CANCELLED 0
#define IMPORT_DONE      1

void rfs_scene_file_import_on_enter(void* context) {
    RfStudioApp* app = context;
    popup_reset(app->popup);
    popup_set_header(app->popup, "Importing...", 64, 26, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewPopup);

    bool is_signal = app->import_kind == RfsImportSignal;
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(
        &options, is_signal ? RFS_SIGNAL_EXT : RFS_REMOTE_EXT, NULL);
    options.hide_dot_files = true;
    FuriString* path = furi_string_alloc_set(is_signal ? EXT_PATH("subghz") : RFS_EXPORTS_DIR);

    uint32_t outcome = IMPORT_CANCELLED;
    if(dialog_file_browser_show(app->dialogs, path, path, &options)) {
        outcome = IMPORT_DONE;
        FuriString* text = furi_string_alloc();
        if(is_signal) {
            RfsSignalMeta* meta = NULL;
            RfsSubResult result =
                rfs_signal_import(app->storage, &app->signals, furi_string_get_cstr(path), &meta);
            if(result == RfsSubOk && meta) {
                char mhz[16];
                rfs_format_mhz(meta->frequency, mhz, sizeof(mhz));
                furi_string_printf(
                    text,
                    "%s\n%s, %s MHz\n%s",
                    meta->name,
                    rfs_kind_name(meta->kind),
                    mhz,
                    rfs_replay_name(rfs_signal_replay(meta)));
                furi_string_set(app->message_title, "Signal Imported");
            } else {
                furi_string_printf(text, "%s", rfs_sub_result_text(result));
                furi_string_set(app->message_title, "Import Failed");
                rfs_diag_error(
                    app->diag,
                    "Import %s: %s",
                    furi_string_get_cstr(path),
                    rfs_sub_result_text(result));
            }
        } else {
            FuriString* name = furi_string_alloc();
            FuriString* error = furi_string_alloc();
            if(rfs_remote_import(
                   app->storage, furi_string_get_cstr(path), &app->signals, name, error)) {
                furi_string_printf(text, "%s\nis now in My Remotes.", furi_string_get_cstr(name));
                furi_string_set(app->message_title, "Remote Imported");
            } else {
                furi_string_set(
                    text,
                    furi_string_empty(error) ? "Could not read the remote." :
                                               furi_string_get_cstr(error));
                furi_string_set(app->message_title, "Import Failed");
                rfs_diag_error(app->diag, "Remote import: %s", furi_string_get_cstr(text));
            }
            furi_string_free(error);
            furi_string_free(name);
        }
        furi_string_set(app->message_text, text);
        furi_string_free(text);
    }
    furi_string_free(path);

    /* Leave this scene from on_event, not from inside on_enter. */
    view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventMenuBase + outcome);
}

bool rfs_scene_file_import_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) {
        return event.type == SceneManagerEventTypeBack;
    }
    if(event.event - RfsEventMenuBase == IMPORT_DONE) {
        /* Returning from the message skips this scene so the browser doesn't reopen. */
        app->message_return =
            scene_manager_has_previous_scene(app->scene_manager, RfsSceneSignalList) ?
                RfsSceneSignalList :
                RfsSceneStart;
        scene_manager_next_scene(app->scene_manager, RfsSceneMessage);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void rfs_scene_file_import_on_exit(void* context) {
    RfStudioApp* app = context;
    popup_reset(app->popup);
}
