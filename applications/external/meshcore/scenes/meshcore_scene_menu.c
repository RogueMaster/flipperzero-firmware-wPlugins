#include "../meshcore_cfg.h"

/* Item order == submenu order == custom event id. */
typedef enum {
    MeshCoreMenuIndexConnect,
    MeshCoreMenuIndexMessenger,
    MeshCoreMenuIndexLogger,
    MeshCoreMenuIndexRadio,
    MeshCoreMenuIndexIdentity,
    MeshCoreMenuIndexRole,
    MeshCoreMenuIndexProfiles,
    MeshCoreMenuIndexSendAdvert,
    MeshCoreMenuIndexSerialLog,
} MeshCoreMenuIndex;

static void meshcore_scene_menu_submenu_callback(void* context, uint32_t index) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void meshcore_scene_menu_on_enter(void* context) {
    MeshCoreApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "MeshCore Config");

#define ITEM(label, idx) \
    submenu_add_item(submenu, label, idx, meshcore_scene_menu_submenu_callback, app)

    ITEM(app->node.valid ? "Reconnect" : "Connect", MeshCoreMenuIndexConnect);
    ITEM("Messenger", MeshCoreMenuIndexMessenger);
    ITEM("Logger", MeshCoreMenuIndexLogger);
    ITEM("Radio", MeshCoreMenuIndexRadio);
    ITEM("Identity", MeshCoreMenuIndexIdentity);
    ITEM("Role", MeshCoreMenuIndexRole);
    ITEM("Profiles", MeshCoreMenuIndexProfiles);
    ITEM("Send advert", MeshCoreMenuIndexSendAdvert);
    ITEM("Serial log", MeshCoreMenuIndexSerialLog);

#undef ITEM

    /* Restore the cursor where the user left it. */
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewSubmenu);
}

bool meshcore_scene_menu_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    /* A scene named on the command line opens here, on the first tick, rather
     * than before view_dispatcher_run(). By now the dispatcher is serving its
     * queue, so a scene that starts a worker has somewhere for that worker's
     * completion event to go. Consumed, so Back returns to the menu instead of
     * reopening it. */
    if(event.type == SceneManagerEventTypeTick && app->launch_scene != MeshCoreSceneNum) {
        MeshCoreSceneId scene = app->launch_scene;
        app->launch_scene = MeshCoreSceneNum;
        scene_manager_next_scene(app->scene_manager, scene);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneMenu, event.event);

        switch(event.event) {
        case MeshCoreMenuIndexConnect:
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneConnect);
            break;
        case MeshCoreMenuIndexMessenger:
            /* Contacts is the messenger's entry point; chat, compose and
             * channels hang off it in later stages. */
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneContacts);
            break;
        case MeshCoreMenuIndexLogger:
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneLogger);
            break;
        case MeshCoreMenuIndexRadio:
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneRadio);
            break;
        case MeshCoreMenuIndexIdentity:
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneIdentity);
            break;
        case MeshCoreMenuIndexRole:
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneRole);
            break;
        case MeshCoreMenuIndexProfiles:
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneProfiles);
            break;
        case MeshCoreMenuIndexSendAdvert:
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneAdvert);
            break;
        case MeshCoreMenuIndexSerialLog:
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneLog);
            break;
        default:
            /* Every menu item now routes somewhere. */
            break;
        }

        return true;
    }

    return false;
}

void meshcore_scene_menu_on_exit(void* context) {
    MeshCoreApp* app = context;
    submenu_reset(app->submenu);
}
