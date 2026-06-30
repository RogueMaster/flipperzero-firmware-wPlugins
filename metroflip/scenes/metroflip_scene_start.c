#include "../metroflip_i.h"
#include <nfc/protocols/mf_classic/mf_classic.h>

void metroflip_scene_start_submenu_callback(void* context, uint32_t index) {
    Metroflip* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void metroflip_scene_start_on_enter(void* context) {
    Metroflip* app = context;
    Submenu* submenu = app->submenu;

    // Clean up any previously loaded MFC data when returning to start
    if(app->mfc_data) {
        mf_classic_free(app->mfc_data);
        app->mfc_data = NULL;
    }

    submenu_set_header(submenu, "Metroflip");

    submenu_add_item(
        submenu, "Scan Card", MetroflipSceneAuto, metroflip_scene_start_submenu_callback, app);

    submenu_add_item(
        submenu,
        "OV-Chipkaart (unstable)",
        MetroflipSceneOVC,
        metroflip_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu, "Saved", MetroflipSceneLoad, metroflip_scene_start_submenu_callback, app);

    submenu_add_item(
        submenu,
        "Supported Cards",
        MetroflipSceneSupported,
        metroflip_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu, "About", MetroflipSceneAbout, metroflip_scene_start_submenu_callback, app);

    submenu_add_item(
        submenu, "Credits", MetroflipSceneCredits, metroflip_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, MetroflipSceneStart));

    notification_message(app->notifications, &sequence_display_backlight_on);
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewSubmenu);
}

bool metroflip_scene_start_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MetroflipSceneStart, event.event);
        consumed = true;
        scene_manager_next_scene(app->scene_manager, event.event);
    }

    return consumed;
}

void metroflip_scene_start_on_exit(void* context) {
    Metroflip* app = context;
    submenu_reset(app->submenu);
}
