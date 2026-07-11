#include "../flipper_recon_i.h"

typedef enum {
    StartIndexNew,
    StartIndexOpen,
    StartIndexAbout,
} StartIndex;

static void flipper_recon_scene_start_submenu_callback(void* context, uint32_t index) {
    FlipperReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipper_recon_scene_start_on_enter(void* context) {
    FlipperReconApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Flipper Recon");
    submenu_add_item(
        submenu, "New engagement", StartIndexNew, flipper_recon_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu,
        "Open engagement",
        StartIndexOpen,
        flipper_recon_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu, "About", StartIndexAbout, flipper_recon_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, FlipperReconSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool flipper_recon_scene_start_on_event(void* context, SceneManagerEvent event) {
    FlipperReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, FlipperReconSceneStart, event.event);
        consumed = true;
        switch(event.event) {
        case StartIndexNew:
            session_reset(app->session, "Engagement");
            app->session_file[0] = '\0';
            scene_manager_next_scene(app->scene_manager, FlipperReconSceneSessionMenu);
            break;
        case StartIndexOpen:
            scene_manager_next_scene(app->scene_manager, FlipperReconSceneSessionList);
            break;
        case StartIndexAbout:
            app->message_mode = ReconMessageAbout;
            scene_manager_next_scene(app->scene_manager, FlipperReconSceneMessage);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void flipper_recon_scene_start_on_exit(void* context) {
    FlipperReconApp* app = context;
    submenu_reset(app->submenu);
}
