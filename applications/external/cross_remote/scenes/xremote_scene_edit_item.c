#include "../xremote.h"
#include "../models/cross/xremote_cross_remote.h"

enum SubmenuIndexEdit {
    SubmenuIndexRename = 10,
    SubmenuIndexTiming,
    SubmenuIndexDelete,
    SubmenuIndexMoveUp,
    SubmenuIndexMoveDown,
};

void xremote_scene_edit_item_submenu_callback(void* context, uint32_t index) {
    XRemote* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void xremote_scene_edit_item_on_enter(void* context) {
    XRemote* app = context;
    submenu_add_item(
        app->editmenu, "Rename", SubmenuIndexRename, xremote_scene_edit_item_submenu_callback, app);

    int16_t item_type = xremote_cross_remote_get_item_type(app->cross_remote, app->edit_item);
    if(item_type == XRemoteRemoteItemTypeInfrared || item_type == XRemoteRemoteItemTypePause) {
        submenu_add_item(
            app->editmenu,
            "Set Timing",
            SubmenuIndexTiming,
            xremote_scene_edit_item_submenu_callback,
            app);
    }

    submenu_add_item(
        app->editmenu, "Delete", SubmenuIndexDelete, xremote_scene_edit_item_submenu_callback, app);

    size_t item_count = xremote_cross_remote_get_item_count(app->cross_remote);
    if(app->edit_item > 0) {
        submenu_add_item(
            app->editmenu,
            "Move Up",
            SubmenuIndexMoveUp,
            xremote_scene_edit_item_submenu_callback,
            app);
    }
    if(app->edit_item + 1 < item_count) {
        submenu_add_item(
            app->editmenu,
            "Move Down",
            SubmenuIndexMoveDown,
            xremote_scene_edit_item_submenu_callback,
            app);
    }

    submenu_set_selected_item(
        app->editmenu, scene_manager_get_scene_state(app->scene_manager, XRemoteSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, XRemoteViewIdEditItem);
}

bool xremote_scene_edit_item_on_event(void* context, SceneManagerEvent event) {
    XRemote* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        //exit app
        scene_manager_previous_scene(app->scene_manager);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexDelete) {
            xremote_cross_remote_remove_item(app->cross_remote, app->edit_item);
        } else if(event.event == SubmenuIndexMoveUp) {
            xremote_cross_remote_move_item_up(app->cross_remote, app->edit_item);
        } else if(event.event == SubmenuIndexMoveDown) {
            xremote_cross_remote_move_item_down(app->cross_remote, app->edit_item);
        } else if(event.event == SubmenuIndexRename) {
            scene_manager_next_scene(app->scene_manager, XRemoteSceneSaveRemoteItem);
            //scene_manager_next_scene(app->scene_manager, XRemoteSceneWip);
            return 0;
        } else if(event.event == SubmenuIndexTiming) {
            if(xremote_cross_remote_get_item_type(app->cross_remote, app->edit_item) ==
               XRemoteRemoteItemTypePause) {
                app->pause_edit_mode = true;
                scene_manager_next_scene(app->scene_manager, XRemoteScenePauseSet);
            } else {
                scene_manager_next_scene(app->scene_manager, XRemoteSceneIrTimer);
            }
            return 0;
        }
        scene_manager_next_scene(app->scene_manager, XRemoteSceneCreate);
    }
    return 0;
}

void xremote_scene_edit_item_on_exit(void* context) {
    XRemote* app = context;
    submenu_reset(app->editmenu);
}
