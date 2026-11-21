#include "../../rf_studio_i.h"

enum {
    SignalDetails,
    SignalTest,
    SignalRename,
    SignalVerify,
    SignalExport,
    SignalDelete,
};

static const char* rfs_signal_verify_label(const RfsSignalMeta* meta) {
    return meta->verified ? "Mark as Untested" : "Mark as Working";
}

void rfs_scene_signal_menu_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    RfsSignalMeta* meta = rfs_signal_find(&app->signals, app->signal_id);
    if(!meta) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    submenu_set_header(submenu, meta->name);
    submenu_add_item(submenu, "Details", SignalDetails, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Test Send", SignalTest, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Rename", SignalRename, rfs_app_submenu_callback, app);
    if(meta->kind == RfsKindRaw || meta->kind == RfsKindBinRaw) {
        submenu_add_item(
            submenu, rfs_signal_verify_label(meta), SignalVerify, rfs_app_submenu_callback, app);
    }
    submenu_add_item(submenu, "Export to Sub-GHz", SignalExport, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Delete", SignalDelete, rfs_app_submenu_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RfsSceneSignalMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_signal_menu_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    RfsSignalMeta* meta = rfs_signal_find(&app->signals, app->signal_id);
    if(!meta) return false;

    if(event.event == RfsEventTxDone) {
        char text[32];
        rfs_tx_outcome_text(app->tx, text, sizeof(text));
        submenu_set_header(app->submenu, text);
        return true;
    }
    if(event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneSignalMenu, index);
    FuriString* text = furi_string_alloc();

    switch(index) {
    case SignalDetails:
        app->info_kind = RfsInfoSignal;
        scene_manager_next_scene(app->scene_manager, RfsSceneInfo);
        break;
    case SignalTest:
        rfs_app_send_signal(app, meta->id, app->settings.radio_mode, text);
        submenu_set_header(app->submenu, furi_string_get_cstr(text));
        break;
    case SignalRename:
        app->text_purpose = RfsTextSignalRename;
        strlcpy(app->text_buffer, meta->name, sizeof(app->text_buffer));
        scene_manager_next_scene(app->scene_manager, RfsSceneTextInput);
        break;
    case SignalVerify:
        meta->verified = !meta->verified;
        if(rfs_app_save_signals(app)) {
            submenu_change_item_label(app->submenu, SignalVerify, rfs_signal_verify_label(meta));
        } else {
            meta->verified = !meta->verified;
            submenu_set_header(app->submenu, "Not saved");
        }
        break;
    case SignalExport: {
        FuriString* path = furi_string_alloc();
        if(rfs_signal_export(app->storage, meta, path)) {
            furi_string_printf(
                text, "Copied to\n%s", furi_string_get_cstr(path) + strlen("/ext/"));
            rfs_app_message(app, "Exported", furi_string_get_cstr(text), RfsSceneNum);
        } else {
            rfs_app_message(app, "Export Failed", "Could not copy the file.", RfsSceneNum);
        }
        furi_string_free(path);
        break;
    }
    case SignalDelete:
        app->confirm_purpose = RfsConfirmDeleteSignal;
        scene_manager_next_scene(app->scene_manager, RfsSceneConfirm);
        break;
    default:
        break;
    }
    furi_string_free(text);
    return true;
}

void rfs_scene_signal_menu_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
