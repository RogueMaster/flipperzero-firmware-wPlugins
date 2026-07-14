#include "../animation_switcher.h"
#include "../views/fas_list_view.h"
#include "fas_scene.h"

/* ── List-view callback (runs on the input thread) ────────────────────── */
static void fas_anim_list_cb(void* context, int index, FasListEvent event) {
    FasApp* app = context;

    switch(event) {
    case FasListEvtOkShort:
        /* Checkbox was toggled inside the view; sync the value back to app */
        app->animations[index].selected = fas_list_view_get_checked(app->list_view, index);
        break;

    case FasListEvtOkLong:
        /* Open per-animation settings.  Set flag so on_enter preserves state. */
        app->current_anim_index = index;
        app->returning_from_settings = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtAnimListOpenSettings);
        break;

    case FasListEvtRight:
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtAnimListDone);
        break;

    default:
        break;
    }
}

/* ── Scene handlers ───────────────────────────────────────────────────── */
void fas_scene_anim_list_on_enter(void* context) {
    FasApp* app = context;

    /* Fresh entry (first time or after completing/aborting a playlist) */
    if(!app->returning_from_settings) {
        fas_load_animations(app);
        app->current_anim_index = 0;
    }
    app->returning_from_settings = false;

    fas_list_view_reset(app->list_view);
    fas_list_view_set_callback(app->list_view, fas_anim_list_cb, app);

    if(app->animation_count == 0) {
        fas_list_view_add_item(app->list_view, "No animations found", false, false);
    } else {
        for(int i = 0; i < app->animation_count; i++) {
            fas_list_view_add_item(
                app->list_view,
                app->animations[i].name,
                /*has_checkbox=*/true,
                app->animations[i].selected);
        }
        /* Restore cursor so it's still on the last-edited animation */
        fas_list_view_set_cursor(app->list_view, app->current_anim_index);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewList);
}

bool fas_scene_anim_list_on_event(void* context, SceneManagerEvent event) {
    FasApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FasEvtAnimListOpenSettings:
            scene_manager_next_scene(app->scene_manager, FasSceneAnimSettings);
            consumed = true;
            break;

        case FasEvtAnimListDone: {
            /* Only proceed if at least one animation is selected */
            bool any = false;
            for(int i = 0; i < app->animation_count; i++) {
                if(app->animations[i].selected) {
                    any = true;
                    break;
                }
            }
            if(any) {
                scene_manager_next_scene(app->scene_manager, FasScenePlaylistName);
            }
            consumed = true;
            break;
        }

        default:
            break;
        }
    }
    return consumed;
}

void fas_scene_anim_list_on_exit(void* context) {
    FasApp* app = context;
    /* Sync checkbox state and cursor from the view back to app data */
    for(int i = 0; i < app->animation_count; i++) {
        app->animations[i].selected = fas_list_view_get_checked(app->list_view, i);
    }
    app->current_anim_index = fas_list_view_get_cursor(app->list_view);
}
