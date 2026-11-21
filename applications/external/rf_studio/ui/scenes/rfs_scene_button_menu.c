#include "../../rf_studio_i.h"

enum {
    ButtonTest,
    ButtonRename,
    ButtonRerecord,
    ButtonAssign,
    ButtonMoveUp,
    ButtonMoveDown,
    ButtonDuplicate,
    ButtonDelete,
    ButtonDetails,
    ButtonVerify,
};

static RfsButton* rfs_current_button(RfStudioApp* app) {
    if(app->button_index < 0 || (size_t)app->button_index >= app->remote.count) return NULL;
    return &app->remote.buttons[app->button_index];
}

static void rfs_button_menu_header(RfStudioApp* app, const char* status) {
    RfsButton* button = rfs_current_button(app);
    FuriString* header = furi_string_alloc();
    furi_string_printf(
        header,
        "%ld/%u %s",
        (long)(app->button_index + 1),
        (unsigned)app->remote.count,
        button ? button->name : "");
    if(status && status[0]) furi_string_printf(header, "%s", status);
    submenu_set_header(app->submenu, furi_string_get_cstr(header));
    furi_string_free(header);
}

static const char* rfs_verify_label(RfsSignalMeta* meta) {
    return meta && meta->verified ? "Mark as Untested" : "Mark as Working";
}

void rfs_scene_button_menu_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    RfsButton* button = rfs_current_button(app);
    if(!button) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    rfs_button_menu_header(app, NULL);
    submenu_add_item(submenu, "Test Send", ButtonTest, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Rename", ButtonRename, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Re-record Signal", ButtonRerecord, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Use Existing Signal", ButtonAssign, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Move Up", ButtonMoveUp, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Move Down", ButtonMoveDown, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Duplicate", ButtonDuplicate, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Delete", ButtonDelete, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Signal Details", ButtonDetails, rfs_app_submenu_callback, app);
    RfsSignalMeta* meta = rfs_signal_find(&app->signals, button->signal_id);
    if(meta && (meta->kind == RfsKindRaw || meta->kind == RfsKindBinRaw)) {
        submenu_add_item(
            submenu, rfs_verify_label(meta), ButtonVerify, rfs_app_submenu_callback, app);
    }
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RfsSceneButtonMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_button_menu_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    RfsButton* button = rfs_current_button(app);
    if(!button) return false;

    if(event.event == RfsEventTxDone) {
        char text[32];
        rfs_tx_outcome_text(app->tx, text, sizeof(text));
        rfs_button_menu_header(app, text);
        return true;
    }
    if(event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneButtonMenu, index);

    switch(index) {
    case ButtonTest: {
        FuriString* status = furi_string_alloc();
        rfs_app_send_signal(app, button->signal_id, rfs_app_remote_radio_mode(app), status);
        rfs_button_menu_header(app, furi_string_get_cstr(status));
        furi_string_free(status);
        break;
    }
    case ButtonRename:
        app->text_purpose = RfsTextButtonRename;
        strlcpy(app->text_buffer, button->name, sizeof(app->text_buffer));
        scene_manager_next_scene(app->scene_manager, RfsSceneTextInput);
        break;
    case ButtonRerecord:
        rfs_app_start_button_capture(app, RfsCaptureForRerecord);
        break;
    case ButtonAssign:
        app->signal_list_mode = RfsSignalListPickForButton;
        scene_manager_set_scene_state(app->scene_manager, RfsSceneSignalList, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneSignalList);
        break;
    case ButtonMoveUp:
    case ButtonMoveDown:
        if(rfs_remote_move_button(&app->remote, app->button_index, index == ButtonMoveUp)) {
            app->button_index += index == ButtonMoveUp ? -1 : 1;
            if(rfs_app_save_remote(app)) {
                rfs_button_menu_header(app, NULL);
            } else {
                rfs_button_menu_header(app, "Not saved");
            }
        } else {
            rfs_button_menu_header(app, index == ButtonMoveUp ? "Already first" : "Already last");
        }
        break;
    case ButtonDuplicate:
        if(rfs_remote_duplicate_button(app->storage, &app->remote, app->button_index)) {
            app->button_index++;
            rfs_button_menu_header(app, rfs_app_save_remote(app) ? "Duplicated" : "Not saved");
        } else {
            rfs_button_menu_header(app, "Not enough memory");
        }
        break;
    case ButtonDelete:
        app->confirm_purpose = RfsConfirmDeleteButton;
        scene_manager_next_scene(app->scene_manager, RfsSceneConfirm);
        break;
    case ButtonDetails:
        if(!rfs_signal_find(&app->signals, button->signal_id)) {
            rfs_app_message(
                app,
                "Signal Missing",
                "This button has no signal\nin the library. Re-record\nor pick an existing one.",
                RfsSceneNum);
        } else {
            strlcpy(app->signal_id, button->signal_id, sizeof(app->signal_id));
            app->info_kind = RfsInfoSignal;
            scene_manager_next_scene(app->scene_manager, RfsSceneInfo);
        }
        break;
    case ButtonVerify: {
        RfsSignalMeta* meta = rfs_signal_find(&app->signals, button->signal_id);
        if(meta) {
            meta->verified = !meta->verified;
            if(rfs_app_save_signals(app)) {
                submenu_change_item_label(app->submenu, ButtonVerify, rfs_verify_label(meta));
                rfs_button_menu_header(app, meta->verified ? "Marked working" : "Marked untested");
            }
        }
        break;
    }
    default:
        break;
    }
    return true;
}

void rfs_scene_button_menu_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
