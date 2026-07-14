#include "lora_scene.h"
#include "lora_app.h"

/**
 * This callback function is called when a submenu item is clicked.
 * 
 * @param context The context passed to the submenu.
 * @param index   The index of the submenu item that was clicked.
*/
static void lora_submenu_item_callback(void* context, uint32_t index) {
    LoraApp* app = context;
    switch(index) {
    case 0:
        lora_transmitter_setup_lorawan(app->transmitter);
        lora_transmitter_otaa_join_procedure(app->transmitter);
        // otaa_join_procedure(app);
        // scene_manager_next_scene(app->scene_manager, LoraSceneLorawan);
        break;
    case 1:
        scene_manager_next_scene(app->scene_manager, LoraSceneReceiveMode);
        lora_transmitter_enter_receive_mode(app->transmitter);
        break;
    case 2:
        lora_transmitter_send_cmsg(app->transmitter, "Hello World");
        // scene_manager_next_scene(app->scene_manager, LoraSceneLorawan);
        break;

    default:
        break;
    }
}

/**
 * Adds the default submenu entries.
 * 
 * @param submenu The submenu.
 * @param context The context to pass to the submenu item callback function.
*/
static void lora_submenu_add_default_entries(Submenu* submenu, void* context) {
    LoraApp* app = context;
    submenu_add_item(submenu, "OTAA join", 0, lora_submenu_item_callback, context);
    submenu_add_item(submenu, "Reiceive Mode", 1, lora_submenu_item_callback, context);
    submenu_add_item(submenu, "Send Msg", 2, lora_submenu_item_callback, context);

    app->index = 3;
}

void lora_scene_start_on_enter(void* context) {
    LoraApp* app = context;
    bt_transmitter_start(app->bt_transmitter); // Start Bluetooth transmitter
    lora_submenu_add_default_entries(app->submenu, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraAppSubMenuView);
}

void lora_scene_start_on_exit(void* context) {
    FURI_LOG_D("LoraSceneStart", "Exiting Lora Scene Start");
    LoraApp* app = context;
    submenu_reset(app->submenu);
}

bool lora_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
