#include "../mizip_balance_editor_i.h"

void mizip_balance_editor_app_submenu_callback(void* context, uint32_t index) {
    MiZipBalanceEditorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void mizip_balance_editor_scene_main_menu_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "MiZip Balance Editor");
    submenu_add_item(
        app->submenu,
        "Direct to tag",
        SubmenuIndexDirectToTag,
        mizip_balance_editor_app_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Edit MiZip file",
        SubmenuIndexEditMiZipFile,
        mizip_balance_editor_app_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "About", SubmenuIndexAbout, mizip_balance_editor_app_submenu_callback, app);

    submenu_set_selected_item(app->submenu, app->last_selected_submenu_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdMainMenu);
}

bool mizip_balance_editor_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        app->last_selected_submenu_index = event.event;
        switch(event.event) {
        case SubmenuIndexDirectToTag:
            app->currentDataSource = DataSourceNfc;
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdScanner);
            break;
        case SubmenuIndexEditMiZipFile:
            app->currentDataSource = DataSourceFile;
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdFileSelect);
            break;
        case SubmenuIndexAbout:
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdAbout);
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view_dispatcher);
    }

    return consumed;
}

void mizip_balance_editor_scene_main_menu_on_exit(void* context) {
    UNUSED(context);
}
