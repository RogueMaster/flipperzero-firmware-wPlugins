#include "../../rf_studio_i.h"

enum {
    MenuButton,
    MenuEdit,
    MenuAdd,
    MenuRename,
    MenuCategory,
    MenuFavorite,
    MenuRadio,
    MenuDuplicate,
    MenuExport,
    MenuDelete,
};

static void rfs_remote_menu_label(RfStudioApp* app, uint32_t item, FuriString* out) {
    switch(item) {
    case MenuCategory:
        furi_string_printf(out, "Category: %s", rfs_category_name(app->remote.category));
        break;
    case MenuFavorite:
        furi_string_printf(out, "Favorite: %s", app->remote.favorite ? "Yes" : "No");
        break;
    case MenuRadio:
        furi_string_printf(out, "Radio: %s", rfs_radio_mode_name(app->remote.preferred_radio));
        break;
    default:
        break;
    }
}

void rfs_scene_remote_menu_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, app->remote.name);

    FuriString* label = furi_string_alloc();
    if(app->remote.count > 0 && (size_t)app->button_index < app->remote.count) {
        furi_string_printf(label, "Button: %s", app->remote.buttons[app->button_index].name);
        submenu_add_item(
            submenu, furi_string_get_cstr(label), MenuButton, rfs_app_submenu_callback, app);
    }
    submenu_add_item(submenu, "Edit Buttons", MenuEdit, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Add Button", MenuAdd, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Rename Remote", MenuRename, rfs_app_submenu_callback, app);
    const uint32_t labelled[] = {MenuCategory, MenuFavorite, MenuRadio};
    for(size_t i = 0; i < COUNT_OF(labelled); i++) {
        rfs_remote_menu_label(app, labelled[i], label);
        submenu_add_item(
            submenu, furi_string_get_cstr(label), labelled[i], rfs_app_submenu_callback, app);
    }
    submenu_add_item(submenu, "Duplicate Remote", MenuDuplicate, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Export Remote", MenuExport, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Delete Remote", MenuDelete, rfs_app_submenu_callback, app);
    furi_string_free(label);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RfsSceneRemoteMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_remote_menu_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneRemoteMenu, index);
    FuriString* text = furi_string_alloc();

    switch(index) {
    case MenuButton:
        scene_manager_set_scene_state(app->scene_manager, RfsSceneButtonMenu, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneButtonMenu);
        break;
    case MenuEdit:
        scene_manager_set_scene_state(app->scene_manager, RfsSceneRemoteEditor, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneRemoteEditor);
        break;
    case MenuAdd:
        rfs_app_start_button_capture(app, RfsCaptureForNewButton);
        break;
    case MenuRename:
        app->text_purpose = RfsTextRemoteRename;
        strlcpy(app->text_buffer, app->remote.name, sizeof(app->text_buffer));
        scene_manager_next_scene(app->scene_manager, RfsSceneTextInput);
        break;
    case MenuCategory:
        scene_manager_next_scene(app->scene_manager, RfsSceneCategory);
        break;
    case MenuFavorite:
    case MenuRadio:
        if(index == MenuFavorite) {
            app->remote.favorite = !app->remote.favorite;
        } else {
            app->remote.preferred_radio = (app->remote.preferred_radio + 1) % RfsRadioModeCount;
        }
        if(rfs_app_save_remote(app)) {
            rfs_remote_menu_label(app, index, text);
            submenu_change_item_label(app->submenu, index, furi_string_get_cstr(text));
        } else {
            rfs_app_message(app, "Not Saved", "Could not save the remote.", RfsSceneNum);
        }
        break;
    case MenuDuplicate: {
        FuriString* name = furi_string_alloc();
        if(rfs_remote_duplicate(app->storage, &app->remote, name)) {
            furi_string_printf(text, "Saved as\n%s", furi_string_get_cstr(name));
            rfs_app_message(app, "Duplicated", furi_string_get_cstr(text), RfsSceneNum);
        } else {
            rfs_app_message(app, "Not Duplicated", "Could not save the copy.", RfsSceneNum);
        }
        furi_string_free(name);
        break;
    }
    case MenuExport: {
        FuriString* dir = furi_string_alloc();
        if(rfs_remote_export(app->storage, &app->remote, &app->signals, dir)) {
            furi_string_printf(
                text, "Saved to apps_data/\nrf_studio/exports/\n%s", app->remote.id);
            rfs_app_message(app, "Exported", furi_string_get_cstr(text), RfsSceneNum);
        } else {
            rfs_diag_error(app->diag, "Export failed: %s", app->remote.id);
            rfs_app_message(app, "Export Failed", "Could not write the export.", RfsSceneNum);
        }
        furi_string_free(dir);
        break;
    }
    case MenuDelete:
        app->confirm_purpose = RfsConfirmDeleteRemote;
        scene_manager_next_scene(app->scene_manager, RfsSceneConfirm);
        break;
    default:
        break;
    }
    furi_string_free(text);
    return true;
}

void rfs_scene_remote_menu_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
