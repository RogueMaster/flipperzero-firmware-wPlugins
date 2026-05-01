#include "flippass_scene_file_browser.h"

#include "../flippass.h"
#include "../kdbx/kdbx_constants.h"
#include "../plugins/flippass_file_plugin.h"
#include "flippass_db_browser_view.h"
#include "flippass_scene.h"
#include "flippass_scene_editor.h"
#include "flippass_scene_status.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <toolbox/path.h>

#define FLIPPASS_BROWSER_ROOT_PATH      EXT_PATH("apps_data/flippass")
#define FLIPPASS_FILE_BROWSER_NAME_SIZE 96U

enum {
    FlipPassSceneFileBrowserEventEnter = 1,
    FlipPassSceneFileBrowserEventUpDirectory,
    FlipPassSceneFileBrowserEventOpenMenu,
    FlipPassSceneFileBrowserEventModifySelected,
    FlipPassSceneFileBrowserEventMenuAction,
};

enum {
    FlipPassFileBrowserMenuActionModify = 0,
    FlipPassFileBrowserMenuActionConfig,
    FlipPassFileBrowserMenuActionRename,
    FlipPassFileBrowserMenuActionDelete,
};

enum {
    FlipPassFileBrowserCreateActionDatabase = 0,
    FlipPassFileBrowserCreateActionDirectory,
};

typedef enum {
    FlipPassFileBrowserItemUp = 0,
    FlipPassFileBrowserItemDirectory,
    FlipPassFileBrowserItemFile,
    FlipPassFileBrowserItemNewDatabase,
    FlipPassFileBrowserItemInfo,
} FlipPassFileBrowserItemType;

typedef enum {
    FlipPassFileBrowserStateBrowse = 0,
    FlipPassFileBrowserStateDeleteConfirm,
} FlipPassFileBrowserState;

typedef struct {
    FlipPassFileBrowserItemType type;
    char name[FLIPPASS_FILE_BROWSER_NAME_SIZE];
} FlipPassFileBrowserItem;

static FlipPassFileBrowserItem* flippass_file_browser_items = NULL;
static size_t flippass_file_browser_item_count = 0U;

static void flippass_file_browser_go_parent(App* app);
static void flippass_file_browser_open_selected_menu(App* app);

static bool flippass_file_browser_back_filter(void* context) {
    App* app = context;

    if(app != NULL) {
        flippass_request_exit(app);
    }

    return true;
}

static bool flippass_file_browser_items_ensure(void) {
    if(flippass_file_browser_items != NULL) {
        return true;
    }

    flippass_file_browser_items =
        malloc(sizeof(FlipPassFileBrowserItem) * FLIPPASS_DB_BROWSER_MAX_ITEMS);
    return flippass_file_browser_items != NULL;
}

static void flippass_file_browser_items_free(void) {
    free(flippass_file_browser_items);
    flippass_file_browser_items = NULL;
    flippass_file_browser_item_count = 0U;
}

static bool flippass_file_browser_has_parent_directory(const App* app) {
    if(app == NULL || app->browser_directory == NULL ||
       furi_string_empty(app->browser_directory)) {
        return false;
    }

    return strcmp(furi_string_get_cstr(app->browser_directory), STORAGE_EXT_PATH_PREFIX) != 0;
}

static void flippass_file_browser_add_item(
    App* app,
    FlipPassFileBrowserItemType type,
    FlipPassDbBrowserItemType view_type,
    const char* label,
    const char* name) {
    FlipPassFileBrowserItem* item = NULL;

    if(flippass_file_browser_items == NULL ||
       flippass_file_browser_item_count >= FLIPPASS_DB_BROWSER_MAX_ITEMS) {
        return;
    }

    item = &flippass_file_browser_items[flippass_file_browser_item_count++];
    item->type = type;
    snprintf(item->name, sizeof(item->name), "%s", name != NULL ? name : "");
    flippass_db_browser_view_add_item(app->db_browser, view_type, label);
}

static const FlipPassFilePluginV1* flippass_file_browser_plugin_load(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotFileOps,
        NULL,
        FLIPPASS_FILE_PLUGIN_APP_ID,
        FLIPPASS_FILE_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        return NULL;
    }

    const FlipPassFilePluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_FILE_PLUGIN_API_VERSION || plugin->list_directory == NULL ||
       plugin->delete_path == NULL) {
        furi_string_set_str(error, "FlipPass file plugin has an incompatible API.");
        return NULL;
    }

    return plugin;
}

static void flippass_file_browser_add_plugin_item(
    void* context,
    FlipPassFilePluginItemType type,
    const char* label,
    const char* name) {
    App* app = context;
    FlipPassFileBrowserItemType item_type = FlipPassFileBrowserItemInfo;
    FlipPassDbBrowserItemType view_type = FlipPassDbBrowserItemTypeInfo;

    switch(type) {
    case FlipPassFilePluginItemUp:
        item_type = FlipPassFileBrowserItemUp;
        view_type = FlipPassDbBrowserItemTypeUp;
        break;
    case FlipPassFilePluginItemDirectory:
        item_type = FlipPassFileBrowserItemDirectory;
        view_type = FlipPassDbBrowserItemTypeGroup;
        break;
    case FlipPassFilePluginItemFile:
        item_type = FlipPassFileBrowserItemFile;
        view_type = FlipPassDbBrowserItemTypeFile;
        break;
    case FlipPassFilePluginItemNewObject:
        item_type = FlipPassFileBrowserItemNewDatabase;
        view_type = FlipPassDbBrowserItemTypeAdd;
        break;
    case FlipPassFilePluginItemInfo:
    default:
        item_type = FlipPassFileBrowserItemInfo;
        view_type = FlipPassDbBrowserItemTypeInfo;
        break;
    }

    flippass_file_browser_add_item(app, item_type, view_type, label, name);
}

static FlipPassFileBrowserItem* flippass_file_browser_selected_item(App* app) {
    furi_assert(app);

    app->browser_directory_selected_index =
        flippass_db_browser_view_get_selected_item(app->db_browser);
    if(app->browser_directory_selected_index >= flippass_file_browser_item_count) {
        return NULL;
    }

    return &flippass_file_browser_items[app->browser_directory_selected_index];
}

static void flippass_file_browser_build_selected_path(App* app, const char* name) {
    furi_assert(app);
    furi_assert(name);

    furi_string_reset(app->pending_path);
    path_concat(furi_string_get_cstr(app->browser_directory), name, app->pending_path);
}

static uint32_t flippass_file_browser_find_directory_index(const char* name) {
    if(name == NULL || name[0] == '\0') {
        return 0U;
    }

    for(size_t index = 0U; index < flippass_file_browser_item_count; index++) {
        if(flippass_file_browser_items[index].type == FlipPassFileBrowserItemDirectory &&
           strcmp(flippass_file_browser_items[index].name, name) == 0) {
            return (uint32_t)index;
        }
    }

    return 0U;
}

static void flippass_file_browser_prepare_new_database(App* app) {
    furi_assert(app);

    app->editor_mode = FlipPassEditorModeNewDatabase;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_selected_index = 0U;
    app->editor_return_scene = FlipPassScene_FileBrowser;
    app->editor_close_after_commit = false;
    app->database_cipher = FlipPassKdbxCipherAes256;
    app->database_compression = KDBX_COMPRESSION_NONE;
    app->database_kdf_rounds = FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
    app->editor_file_name[0] = '\0';
    app->editor_group_name[0] = '\0';
    app->editor_database_password[0] = '\0';
    flippass_editor_clear_custom_field_drafts(app);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
}

static void flippass_file_browser_prepare_new_directory(App* app) {
    furi_assert(app);

    app->editor_mode = FlipPassEditorModeNewDirectory;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_selected_index = 0U;
    app->editor_return_scene = FlipPassScene_FileBrowser;
    app->editor_close_after_commit = false;
    app->editor_group_name[0] = '\0';
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
}

static void flippass_file_browser_open_selected_file(App* app) {
    FlipPassFileBrowserItem* item = flippass_file_browser_selected_item(app);

    if(item == NULL) {
        return;
    }

    switch(item->type) {
    case FlipPassFileBrowserItemUp:
        flippass_file_browser_go_parent(app);
        break;
    case FlipPassFileBrowserItemDirectory:
        flippass_file_browser_build_selected_path(app, item->name);
        furi_string_set(app->browser_directory, app->pending_path);
        app->browser_directory_selected_index = 0U;
        flippass_scene_file_browser_on_enter(app);
        break;
    case FlipPassFileBrowserItemFile:
        flippass_file_browser_build_selected_path(app, item->name);
        furi_string_set(app->file_path, app->pending_path);
        flippass_reset_database(app);
        flippass_clear_text_buffer(app);
        flippass_clear_master_password(app);
        if(app->editor_mode != FlipPassEditorModeModifyDatabase) {
            app->editor_mode = FlipPassEditorModeNone;
            app->editor_return_scene = FlipPassScene_FileBrowser;
            app->editor_close_after_commit = false;
        }
        scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
        break;
    case FlipPassFileBrowserItemNewDatabase:
        flippass_file_browser_open_selected_menu(app);
        break;
    case FlipPassFileBrowserItemInfo:
    default:
        break;
    }
}

static void flippass_file_browser_open_selected_menu(App* app) {
    FlipPassFileBrowserItem* item = flippass_file_browser_selected_item(app);

    if(item == NULL || (item->type != FlipPassFileBrowserItemFile &&
                        item->type != FlipPassFileBrowserItemNewDatabase)) {
        return;
    }

    if(item->type == FlipPassFileBrowserItemFile) {
        flippass_file_browser_build_selected_path(app, item->name);
    } else {
        furi_string_reset(app->pending_path);
    }
    app->browser_menu_selected_index = 0U;
    flippass_db_browser_view_set_action_selected(app->db_browser, 0U);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, true);
}

static void flippass_file_browser_prepare_rename(App* app) {
    FuriString* file_name = furi_string_alloc();

    path_extract_filename(app->pending_path, file_name, true);
    app->editor_mode = FlipPassEditorModeRenameFile;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_selected_index = 0U;
    app->editor_return_scene = FlipPassScene_FileBrowser;
    app->editor_close_after_commit = false;
    snprintf(
        app->editor_file_name,
        sizeof(app->editor_file_name),
        "%s",
        furi_string_empty(file_name) ? "" : furi_string_get_cstr(file_name));
    furi_string_free(file_name);
}

static void flippass_file_browser_prepare_modify(App* app) {
    app->editor_mode = FlipPassEditorModeModifyDatabase;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_selected_index = 4U;
    app->editor_return_scene = FlipPassScene_FileBrowser;
    app->editor_close_after_commit = true;
    app->editor_database_password[0] = '\0';
    furi_string_set(app->file_path, app->pending_path);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
}

static void flippass_file_browser_prepare_config(App* app) {
    app->editor_mode = FlipPassEditorModeGlobalConfig;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_selected_index = 0U;
    app->editor_return_scene = FlipPassScene_FileBrowser;
    app->editor_close_after_commit = false;
    app->editor_idle_lock_minutes = app->idle_lock_minutes;
    app->editor_idle_unlock_attempts = app->idle_unlock_attempts;
    app->editor_idle_exit_minutes = app->idle_exit_minutes;
    app->editor_otp_time_zone_minutes = app->otp_time_zone_minutes;
    app->editor_keyboard_layout_index = 0U;
    app->editor_keyboard_layout_use_alt = app->keyboard_layout_path == NULL ||
                                          furi_string_empty(app->keyboard_layout_path);
    app->editor_keyboard_layout_available = false;
    snprintf(
        app->editor_keyboard_layout_path,
        sizeof(app->editor_keyboard_layout_path),
        "%s",
        app->editor_keyboard_layout_use_alt ? "" :
                                              furi_string_get_cstr(app->keyboard_layout_path));
}

static void flippass_file_browser_modify_selected(App* app) {
    FlipPassFileBrowserItem* item = flippass_file_browser_selected_item(app);

    flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
    if(item == NULL) {
        return;
    }

    if(item->type == FlipPassFileBrowserItemFile) {
        flippass_file_browser_build_selected_path(app, item->name);
        flippass_file_browser_prepare_modify(app);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
    } else if(item->type == FlipPassFileBrowserItemNewDatabase) {
        flippass_file_browser_open_selected_menu(app);
    }
}

static void flippass_file_browser_clear_last_open_if_matches(App* app, const char* path) {
    if(app->last_open_file_path != NULL && path != NULL &&
       strcmp(furi_string_get_cstr(app->last_open_file_path), path) == 0) {
        furi_string_reset(app->last_open_file_path);
        app->last_open_count = 0U;
        flippass_save_settings(app);
    }

    if(app->file_path != NULL && path != NULL &&
       strcmp(furi_string_get_cstr(app->file_path), path) == 0) {
        furi_string_reset(app->file_path);
    }
}

static bool flippass_file_browser_delete_selected(App* app) {
    FuriString* error = furi_string_alloc();
    const FlipPassFilePluginV1* plugin = flippass_file_browser_plugin_load(app, error);
    const bool deleted = plugin != NULL &&
                         plugin->delete_path(furi_string_get_cstr(app->pending_path), error);

    if(deleted) {
        flippass_file_browser_clear_last_open_if_matches(
            app, furi_string_get_cstr(app->pending_path));
    }

    flippass_module_unload(app, FlipPassModuleSlotFileOps);
    furi_string_free(error);
    return deleted;
}

static void flippass_file_browser_delete_dialog_callback(DialogExResult result, void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0x100U + result);
}

static void flippass_file_browser_show_delete_confirm(App* app) {
    FuriString* file_name = furi_string_alloc();

    path_extract_filename(app->pending_path, file_name, false);
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Delete Database?", 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex,
        furi_string_empty(file_name) ? "This action cannot be undone." :
                                       furi_string_get_cstr(file_name),
        64,
        20,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "Cancel");
    dialog_ex_set_right_button_text(app->dialog_ex, "Delete");
    dialog_ex_set_result_callback(app->dialog_ex, flippass_file_browser_delete_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_FileBrowser, FlipPassFileBrowserStateDeleteConfirm);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialogEx);
    furi_string_free(file_name);
}

static void flippass_file_browser_handle_menu_action(App* app) {
    FlipPassFileBrowserItem* item = flippass_file_browser_selected_item(app);

    app->browser_menu_selected_index =
        flippass_db_browser_view_get_action_selected(app->db_browser);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, false);

    if(item == NULL) {
        return;
    }

    if(item->type == FlipPassFileBrowserItemNewDatabase) {
        if(app->browser_menu_selected_index == FlipPassFileBrowserCreateActionDatabase) {
            flippass_file_browser_prepare_new_database(app);
        } else if(app->browser_menu_selected_index == FlipPassFileBrowserCreateActionDirectory) {
            flippass_file_browser_prepare_new_directory(app);
        }
        return;
    }

    if(item->type != FlipPassFileBrowserItemFile) {
        return;
    }

    if(app->browser_menu_selected_index == FlipPassFileBrowserMenuActionModify) {
        flippass_file_browser_prepare_modify(app);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
        return;
    }

    if(app->browser_menu_selected_index == FlipPassFileBrowserMenuActionConfig) {
        flippass_file_browser_prepare_config(app);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
        return;
    }

    if(app->browser_menu_selected_index == FlipPassFileBrowserMenuActionRename) {
        flippass_file_browser_prepare_rename(app);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
        return;
    }

    if(app->browser_menu_selected_index == FlipPassFileBrowserMenuActionDelete) {
        flippass_file_browser_show_delete_confirm(app);
    }
}

static void flippass_file_browser_go_parent(App* app) {
    FuriString* parent = NULL;
    FuriString* child_name = NULL;

    furi_assert(app);
    if(!flippass_file_browser_has_parent_directory(app)) {
        return;
    }

    parent = furi_string_alloc();
    child_name = furi_string_alloc();
    path_extract_basename(furi_string_get_cstr(app->browser_directory), child_name);
    path_extract_dirname(furi_string_get_cstr(app->browser_directory), parent);
    if(furi_string_empty(parent)) {
        furi_string_set_str(app->browser_directory, STORAGE_EXT_PATH_PREFIX);
    } else {
        furi_string_set(app->browser_directory, parent);
    }
    app->browser_directory_selected_index = 0U;
    furi_string_free(parent);
    flippass_scene_file_browser_on_enter(app);
    if(!furi_string_empty(child_name)) {
        app->browser_directory_selected_index =
            flippass_file_browser_find_directory_index(furi_string_get_cstr(child_name));
        flippass_db_browser_view_set_selected_item(
            app->db_browser, app->browser_directory_selected_index);
    }
    furi_string_free(child_name);
}

static void flippass_file_browser_view_callback(FlipPassDbBrowserEvent event, void* context) {
    App* app = context;
    uint32_t custom_event = 0U;

    switch(event) {
    case FlipPassDbBrowserEventEnter:
        custom_event = FlipPassSceneFileBrowserEventEnter;
        break;
    case FlipPassDbBrowserEventBack:
        custom_event = FlipPassSceneFileBrowserEventUpDirectory;
        break;
    case FlipPassDbBrowserEventOpenActionMenu:
        custom_event = FlipPassSceneFileBrowserEventOpenMenu;
        break;
    case FlipPassDbBrowserEventLongOk:
        custom_event = FlipPassSceneFileBrowserEventModifySelected;
        break;
    case FlipPassDbBrowserEventSelectAction:
        custom_event = FlipPassSceneFileBrowserEventMenuAction;
        break;
    default:
        break;
    }

    if(custom_event != 0U) {
        view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
    }
}

static void flippass_file_browser_render(App* app) {
    FuriString* error = furi_string_alloc();
    FuriString* resolved_directory = furi_string_alloc();
    const FlipPassFilePluginV1* plugin = NULL;
    bool has_parent = false;
    const FlipPassFileHostApiV1 host_api = {
        .api_version = FLIPPASS_FILE_HOST_API_VERSION,
        .context = app,
        .add_item = flippass_file_browser_add_plugin_item,
    };

    furi_assert(app);

    if(!flippass_file_browser_items_ensure()) {
        flippass_scene_status_show(
            app,
            "Open Failed",
            "Not enough RAM is available to list files.",
            FlipPassScene_FileBrowser);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        goto cleanup;
    }

    flippass_file_browser_item_count = 0U;
    flippass_db_browser_view_reset(app->db_browser);
    flippass_db_browser_view_set_mode(app->db_browser, FlipPassDbBrowserModeBrowse);

    plugin = flippass_file_browser_plugin_load(app, error);
    if(plugin == NULL) {
        flippass_scene_status_show(
            app, "Open Failed", furi_string_get_cstr(error), FlipPassScene_FileBrowser);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        goto cleanup;
    }

    const FlipPassFileListRequestV1 request = {
        .api_version = FLIPPASS_FILE_PLUGIN_API_VERSION,
        .root_path = FLIPPASS_BROWSER_ROOT_PATH,
        .requested_directory = furi_string_get_cstr(app->browser_directory),
        .fallback_file_path = furi_string_get_cstr(app->file_path),
        .max_items = FLIPPASS_DB_BROWSER_MAX_ITEMS,
        .resolved_directory = resolved_directory,
        .has_parent = &has_parent,
    };

    if(!plugin->list_directory(&request, &host_api, error)) {
        flippass_scene_status_show(
            app, "Open Failed", furi_string_get_cstr(error), FlipPassScene_FileBrowser);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        goto cleanup;
    }

    furi_string_set(app->browser_directory, resolved_directory);
    flippass_db_browser_view_set_has_parent(app->db_browser, has_parent);
    flippass_db_browser_view_set_header(
        app->db_browser, furi_string_get_cstr(app->browser_directory));

    if(app->browser_directory_selected_index >= flippass_file_browser_item_count) {
        app->browser_directory_selected_index = 0U;
    }

    flippass_db_browser_view_set_selected_item(
        app->db_browser, app->browser_directory_selected_index);
    flippass_db_browser_view_set_show_other_action(app->db_browser, false);
    flippass_db_browser_view_set_action_selected(app->db_browser, 0U);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_FileBrowser, FlipPassFileBrowserStateBrowse);

cleanup:
    flippass_module_unload(app, FlipPassModuleSlotFileOps);
    furi_string_free(resolved_directory);
    furi_string_free(error);
}

void flippass_scene_file_browser_on_enter(void* context) {
    App* app = context;

    flippass_db_browser_view_set_callback(
        app->db_browser, flippass_file_browser_view_callback, app);
    flippass_db_browser_view_set_back_filter(app->db_browser, flippass_file_browser_back_filter);
    flippass_file_browser_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
}

bool flippass_scene_file_browser_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FlipPassSceneFileBrowserEventEnter) {
            flippass_file_browser_open_selected_file(app);
            return true;
        }

        if(event.event == FlipPassSceneFileBrowserEventUpDirectory) {
            flippass_file_browser_go_parent(app);
            return true;
        }

        if(event.event == FlipPassSceneFileBrowserEventOpenMenu) {
            flippass_file_browser_open_selected_menu(app);
            return true;
        }

        if(event.event == FlipPassSceneFileBrowserEventModifySelected) {
            flippass_file_browser_modify_selected(app);
            return true;
        }

        if(event.event == FlipPassSceneFileBrowserEventMenuAction) {
            flippass_file_browser_handle_menu_action(app);
            return true;
        }

        if(event.event == (0x100U + DialogExResultRight)) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipPassScene_FileBrowser, FlipPassFileBrowserStateBrowse);
            if(flippass_file_browser_delete_selected(app)) {
                flippass_file_browser_render(app);
                view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
            } else {
                flippass_scene_status_show(
                    app,
                    "Delete Failed",
                    "The selected database could not be deleted.",
                    FlipPassScene_FileBrowser);
                scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            }
            return true;
        }

        if(event.event == (0x100U + DialogExResultLeft)) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipPassScene_FileBrowser, FlipPassFileBrowserStateBrowse);
            flippass_db_browser_view_set_action_menu_open(app->db_browser, true);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        if(scene_manager_get_scene_state(app->scene_manager, FlipPassScene_FileBrowser) ==
           FlipPassFileBrowserStateDeleteConfirm) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipPassScene_FileBrowser, FlipPassFileBrowserStateBrowse);
            flippass_db_browser_view_set_action_menu_open(app->db_browser, true);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
            return true;
        }

        flippass_request_exit(app);
        return true;
    }

    return false;
}

void flippass_scene_file_browser_on_exit(void* context) {
    App* app = context;

    app->browser_directory_selected_index =
        flippass_db_browser_view_get_selected_item(app->db_browser);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
    flippass_db_browser_view_set_back_filter(app->db_browser, NULL);
    flippass_file_browser_items_free();
    dialog_ex_reset(app->dialog_ex);
}
