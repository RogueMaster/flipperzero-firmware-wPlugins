#include "../sniffer_card_app.h"

// Label and destination in one row, so a reordered menu cannot quietly open the wrong scene.
//
// Two jobs rather than one guided run, and in the order they are first done. The run cannot be
// guided: between setting the UID and reading the captures the card goes to a reader, which is
// minutes away from the Flipper with the app closed, so a wizard spanning that is a wizard whose
// middle step is "leave". Each half stands alone and either can be repeated without the other.
static const struct {
    const char* label;
    SnifferCardAppScene scene;
} sniffer_card_start_items[] = {
    {"Set Sniffer UID", SnifferCardAppSceneUidSource},
    {"Read Sniffer Card", SnifferCardAppSceneRead},
    {"How to Use", SnifferCardAppSceneInfo},
};

static void sniffer_card_app_scene_start_callback(void* context, uint32_t index) {
    SnifferCardApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sniffer_card_app_scene_start_on_enter(void* context) {
    SnifferCardApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Sniffer Card");

    for(size_t i = 0; i < COUNT_OF(sniffer_card_start_items); i++) {
        submenu_add_item(
            app->submenu,
            sniffer_card_start_items[i].label,
            i,
            sniffer_card_app_scene_start_callback,
            app);
    }

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, SnifferCardAppSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewSubmenu);
}

bool sniffer_card_app_scene_start_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= COUNT_OF(sniffer_card_start_items)) return false;

    scene_manager_set_scene_state(app->scene_manager, SnifferCardAppSceneStart, event.event);
    scene_manager_next_scene(app->scene_manager, sniffer_card_start_items[event.event].scene);

    return true;
}

void sniffer_card_app_scene_start_on_exit(void* context) {
    SnifferCardApp* app = context;
    submenu_reset(app->submenu);
}
