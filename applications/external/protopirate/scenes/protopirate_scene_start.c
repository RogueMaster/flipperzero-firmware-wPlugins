// scenes/protopirate_scene_start.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"

#include "proto_pirate_icons.h"

#define TAG "ProtoPirateSceneStart"

typedef enum {
    SubmenuIndexProtoPirateReceiver,
    SubmenuIndexProtoPirateSaved,
    SubmenuIndexProtoPirateReceiverConfig,
#ifdef ENABLE_SUB_DECODE_SCENE
    SubmenuIndexProtoPirateSubDecode,
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
    SubmenuIndexProtoPirateTimingTuner,
#endif
    SubmenuIndexProtoPirateAbout,
} SubmenuIndex;

static void protopirate_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_check(context);
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void protopirate_scene_start_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    protopirate_release_shared_radio_state(app);

    submenu_add_item(
        app->submenu,
        "Receive",
        SubmenuIndexProtoPirateReceiver,
        protopirate_scene_start_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Saved Captures",
        SubmenuIndexProtoPirateSaved,
        protopirate_scene_start_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Configuration",
        SubmenuIndexProtoPirateReceiverConfig,
        protopirate_scene_start_submenu_callback,
        app);
#ifdef ENABLE_SUB_DECODE_SCENE
    submenu_add_item(
        app->submenu,
        "Sub Decode",
        SubmenuIndexProtoPirateSubDecode,
        protopirate_scene_start_submenu_callback,
        app);
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
    submenu_add_item(
        app->submenu,
        "Timing Tuner",
        SubmenuIndexProtoPirateTimingTuner,
        protopirate_scene_start_submenu_callback,
        app);
#endif

    submenu_add_item(
        app->submenu,
        "About",
        SubmenuIndexProtoPirateAbout,
        protopirate_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, ProtoPirateSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewSubmenu);
}

bool protopirate_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexProtoPirateAbout) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneAbout);
            consumed = true;
        } else if(event.event == SubmenuIndexProtoPirateReceiver) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiver);
            consumed = true;
        } else if(event.event == SubmenuIndexProtoPirateSaved) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSaved);
            consumed = true;
        } else if(event.event == SubmenuIndexProtoPirateReceiverConfig) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverConfig);
            consumed = true;
        }
#ifdef ENABLE_SUB_DECODE_SCENE
        else if(event.event == SubmenuIndexProtoPirateSubDecode) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSubDecode);
            consumed = true;
        }
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
        else if(event.event == SubmenuIndexProtoPirateTimingTuner) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneTimingTuner);
            consumed = true;
        }
#endif
        scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneStart, event.event);
    }

    return consumed;
}

void protopirate_scene_start_on_exit(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    submenu_reset(app->submenu);
}
