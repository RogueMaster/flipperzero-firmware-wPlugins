#include "../rosetta_i.h"

/* Per-protocol landing: choose the animated walkthrough or the live capture. */
typedef enum {
    ProtoIndexWalkthrough,
    ProtoIndexCapture,
} ProtoIndex;

static void rosetta_scene_protocol_submenu_cb(void* context, uint32_t index) {
    RosettaApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void rosetta_scene_protocol_on_enter(void* context) {
    RosettaApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, protocol_name(app->protocol));
    submenu_add_item(
        submenu, "Walkthrough", ProtoIndexWalkthrough, rosetta_scene_protocol_submenu_cb, app);

    /* Label the live option with what the hardware actually does. */
    const char* live;
    switch(app->protocol) {
    case ProtocolMifare:
        live = "Live Capture (NFC)";
        break;
    case ProtocolModulation:
        live = "Live Capture (RF)";
        break;
    case ProtocolOneWire:
    default:
        live = "Live Capture (iButton)";
        break;
    }
    submenu_add_item(submenu, live, ProtoIndexCapture, rosetta_scene_protocol_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RosettaSceneProtocol));

    view_dispatcher_switch_to_view(app->view_dispatcher, RosettaViewSubmenu);
}

bool rosetta_scene_protocol_on_event(void* context, SceneManagerEvent event) {
    RosettaApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, RosettaSceneProtocol, event.event);
        switch(event.event) {
        case ProtoIndexWalkthrough:
            scene_manager_next_scene(app->scene_manager, RosettaSceneLesson);
            consumed = true;
            break;
        case ProtoIndexCapture:
            scene_manager_next_scene(app->scene_manager, RosettaSceneCapture);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void rosetta_scene_protocol_on_exit(void* context) {
    RosettaApp* app = context;
    submenu_reset(app->submenu);
}
