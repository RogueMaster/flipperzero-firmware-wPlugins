#include "../breach_map_i.h"

typedef enum {
    StartIndexNew,
    StartIndexOpen,
    StartIndexSetPin,
    StartIndexRemovePin,
    StartIndexAbout,
} StartIndex;

static void breach_map_scene_start_submenu_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void breach_map_scene_start_on_enter(void* context) {
    BreachMapApp* app = context;

    /* screen lock: require the PIN once per launch */
    if(app->pin_set && !app->unlocked) {
        scene_manager_next_scene(app->scene_manager, BreachMapScenePin);
        return;
    }

    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "BreachMap");
    submenu_add_item(
        submenu, "New engagement", StartIndexNew, breach_map_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Open engagement", StartIndexOpen, breach_map_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu,
        app->pin_set ? "Change PIN" : "Set PIN",
        StartIndexSetPin,
        breach_map_scene_start_submenu_callback,
        app);
    if(app->pin_set) {
        submenu_add_item(
            submenu,
            "Remove PIN",
            StartIndexRemovePin,
            breach_map_scene_start_submenu_callback,
            app);
    }
    submenu_add_item(
        submenu, "About", StartIndexAbout, breach_map_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, BreachMapSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool breach_map_scene_start_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BreachMapSceneStart, event.event);
        consumed = true;
        switch(event.event) {
        case StartIndexNew:
            session_reset(app->session, "Engagement");
            app->session_file[0] = '\0';
            scene_manager_next_scene(app->scene_manager, BreachMapSceneSessionMenu);
            break;
        case StartIndexOpen:
            scene_manager_next_scene(app->scene_manager, BreachMapSceneSessionList);
            break;
        case StartIndexSetPin:
            app->text_target = ReconTextTargetPinSet;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneTextInput);
            break;
        case StartIndexRemovePin:
            app->pin_set = false;
            app->pin_hash = 0;
            breach_settings_save(app->storage, app->pin_hash, app->pin_set);
            break;
        case StartIndexAbout:
            app->message_mode = ReconMessageAbout;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void breach_map_scene_start_on_exit(void* context) {
    BreachMapApp* app = context;
    submenu_reset(app->submenu);
}
