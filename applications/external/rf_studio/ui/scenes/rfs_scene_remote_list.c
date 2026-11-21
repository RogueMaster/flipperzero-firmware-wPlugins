#include "../../rf_studio_i.h"

#define ITEM_SEARCH 0
#define ITEM_NEW    1
#define ITEM_EMPTY  99
#define ITEM_REMOTE 100

void rfs_scene_remote_list_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    rfs_remote_list_load(app->storage, &app->remote_list);
    rfs_remote_list_sort(&app->remote_list, app->settings.sort);

    FuriString* label = furi_string_alloc();
    size_t shown = 0;

    if(!app->favorites_only) {
        if(app->search[0]) {
            furi_string_printf(label, "Search: %s", app->search);
        } else {
            furi_string_set(label, "Search...");
        }
        submenu_add_item(
            submenu, furi_string_get_cstr(label), ITEM_SEARCH, rfs_app_submenu_callback, app);
        submenu_add_item(submenu, "+ New Remote", ITEM_NEW, rfs_app_submenu_callback, app);
    }

    for(size_t i = 0; i < app->remote_list.count; i++) {
        const RfsRemoteSummary* item = &app->remote_list.items[i];
        if(app->favorites_only && !item->favorite) continue;
        if(!rfs_strcasestr(item->name, app->search)) continue;
        furi_string_reset(label);
        if(item->favorite) furi_string_cat_str(label, "* ");
        if(app->settings.sort == RfsSortCategory) {
            furi_string_cat_printf(label, "%s: ", rfs_category_short(item->category));
        }
        furi_string_cat_str(label, item->name);
        submenu_add_item(
            submenu, furi_string_get_cstr(label), ITEM_REMOTE + i, rfs_app_submenu_callback, app);
        shown++;
    }
    if(shown == 0) {
        submenu_add_item(
            submenu,
            app->favorites_only ? "(no favorites yet)" : "(no remotes found)",
            ITEM_EMPTY,
            rfs_app_submenu_callback,
            app);
    }

    furi_string_printf(
        label, "%s (%u)", app->favorites_only ? "Favorites" : "My Remotes", (unsigned)shown);
    submenu_set_header(submenu, furi_string_get_cstr(label));
    furi_string_free(label);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RfsSceneRemoteList));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_remote_list_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneRemoteList, index);

    if(index == ITEM_SEARCH) {
        app->text_purpose = RfsTextSearch;
        strlcpy(app->text_buffer, app->search, sizeof(app->text_buffer));
        scene_manager_next_scene(app->scene_manager, RfsSceneTextInput);
    } else if(index == ITEM_NEW) {
        app->text_purpose = RfsTextRemoteNew;
        app->text_buffer[0] = '\0';
        scene_manager_next_scene(app->scene_manager, RfsSceneTextInput);
    } else if(index >= ITEM_REMOTE && index - ITEM_REMOTE < app->remote_list.count) {
        const RfsRemoteSummary* item = &app->remote_list.items[index - ITEM_REMOTE];
        FuriString* error = furi_string_alloc();
        bool loaded = rfs_remote_load(app->storage, item->id, &app->remote, error);
        if(!loaded) {
            rfs_diag_error(app->diag, "Open %s: %s", item->id, furi_string_get_cstr(error));
            rfs_app_message(app, "Can't Open Remote", furi_string_get_cstr(error), RfsSceneNum);
        } else {
            app->remote.last_used = rfs_storage_now();
            /* Last-used only affects sorting; a failed write is not worth interrupting for. */
            rfs_remote_save(app->storage, &app->remote);
            app->button_index = 0;
            if(!furi_string_empty(error)) {
                /* Partly readable: show what loaded and say so (Section 12.6). */
                rfs_diag_error(app->diag, "Open %s: %s", item->id, furi_string_get_cstr(error));
                scene_manager_next_scene(app->scene_manager, RfsSceneRemoteRun);
                rfs_app_message(app, "Remote Damaged", furi_string_get_cstr(error), RfsSceneNum);
            } else {
                scene_manager_next_scene(app->scene_manager, RfsSceneRemoteRun);
            }
        }
        furi_string_free(error);
    }
    return true;
}

void rfs_scene_remote_list_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
