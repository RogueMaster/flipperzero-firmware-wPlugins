#include "../rosetta_i.h"

/* Main menu: the three protocols, then settings + about. The first three menu
 * indices map 1:1 onto RosettaProtocol so we can stash the choice directly. */
typedef enum {
    StartIndexMifare = ProtocolMifare,
    StartIndexModulation = ProtocolModulation,
    StartIndexOneWire = ProtocolOneWire,
    StartIndexSettings = ProtocolCount,
    StartIndexAbout,
} StartIndex;

static void rosetta_scene_start_submenu_cb(void* context, uint32_t index) {
    RosettaApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void rosetta_scene_start_on_enter(void* context) {
    RosettaApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Rosetta");
    submenu_add_item(
        submenu, "Mifare Auth", StartIndexMifare, rosetta_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "OOK & PSK", StartIndexModulation, rosetta_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "1-Wire", StartIndexOneWire, rosetta_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, rosetta_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, rosetta_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RosettaSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, RosettaViewSubmenu);
}

bool rosetta_scene_start_on_event(void* context, SceneManagerEvent event) {
    RosettaApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, RosettaSceneStart, event.event);
        switch(event.event) {
        case StartIndexMifare:
        case StartIndexModulation:
        case StartIndexOneWire:
            app->protocol = (RosettaProtocol)event.event;
            scene_manager_next_scene(app->scene_manager, RosettaSceneProtocol);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, RosettaSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, RosettaSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void rosetta_scene_start_on_exit(void* context) {
    RosettaApp* app = context;
    submenu_reset(app->submenu);
}
