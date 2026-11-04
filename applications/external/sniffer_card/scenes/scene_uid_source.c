#include "../sniffer_card_app.h"

// Where the UID to write comes from. Three sources rather than one, because the card whose keys
// are wanted is not always the card in reach: its UID may be written down, or sitting in a dump
// saved earlier, or on a tag somebody else is holding.
static const struct {
    const char* label;
    SnifferCardAppScene scene;
} sniffer_card_uid_sources[] = {
    {"From Tag", SnifferCardAppSceneReadOriginal},
    {"From Dump", SnifferCardAppSceneLoadDump},
    {"Enter UID", SnifferCardAppSceneUidLength},
};

static void sniffer_card_app_scene_uid_source_callback(void* context, uint32_t index) {
    SnifferCardApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sniffer_card_app_scene_uid_source_on_enter(void* context) {
    SnifferCardApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "UID to write");

    for(size_t i = 0; i < COUNT_OF(sniffer_card_uid_sources); i++) {
        submenu_add_item(
            app->submenu,
            sniffer_card_uid_sources[i].label,
            i,
            sniffer_card_app_scene_uid_source_callback,
            app);
    }

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, SnifferCardAppSceneUidSource));

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewSubmenu);
}

bool sniffer_card_app_scene_uid_source_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= COUNT_OF(sniffer_card_uid_sources)) return false;

    scene_manager_set_scene_state(app->scene_manager, SnifferCardAppSceneUidSource, event.event);
    scene_manager_next_scene(app->scene_manager, sniffer_card_uid_sources[event.event].scene);

    return true;
}

void sniffer_card_app_scene_uid_source_on_exit(void* context) {
    SnifferCardApp* app = context;
    submenu_reset(app->submenu);
}
