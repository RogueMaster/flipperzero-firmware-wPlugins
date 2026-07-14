#include "../animation_switcher.h"
#include "../views/fas_list_view.h"
#include "fas_scene.h"

static void fas_delete_playlist_cb(void* context, int index, FasListEvent event) {
    FasApp* app = context;
    app->current_playlist_index = index;

    switch(event) {
    case FasListEvtOkShort:
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtDeleteSelect);
        break;
    case FasListEvtOkLong:
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtDeletePreview);
        break;
    default:
        break;
    }
}

void fas_scene_delete_playlist_on_enter(void* context) {
    FasApp* app = context;
    fas_load_playlists(app);

    fas_list_view_reset(app->list_view);
    fas_list_view_set_callback(app->list_view, fas_delete_playlist_cb, app);

    if(app->playlist_count == 0) {
        fas_list_view_add_item(app->list_view, "No playlists yet", false, false);
    } else {
        for(int i = 0; i < app->playlist_count; i++) {
            fas_list_view_add_item(app->list_view, app->playlists[i].name, false, false);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewList);
}

bool fas_scene_delete_playlist_on_event(void* context, SceneManagerEvent event) {
    FasApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FasEvtDeleteSelect:
            if(app->playlist_count > 0) {
                scene_manager_next_scene(app->scene_manager, FasSceneDeleteConfirm);
            }
            consumed = true;
            break;

        case FasEvtDeletePreview:
            if(app->playlist_count > 0) {
                scene_manager_next_scene(app->scene_manager, FasScenePlaylistPreview);
            }
            consumed = true;
            break;

        default:
            break;
        }
    }
    return consumed;
}

void fas_scene_delete_playlist_on_exit(void* context) {
    UNUSED(context);
}
