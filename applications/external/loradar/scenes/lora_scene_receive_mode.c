#include "lora_scene.h"
#include "lora_app.h"
#include "lora_config.h"
#include "lora_receiver_i.h"

void lora_scene_receive_mode_callback(LoraCustomEvent event, void* context) {
    furi_assert(context);
    LoraApp* app = context;
    FURI_LOG_D("LoraSceneReceiveMode", "Received event: %d", event);
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void lora_scene_receive_mode_on_enter(void* context) {
    FURI_LOG_D("LoraSceneReceiveMode", "Entering Lora Scene Receive Mode");
    LoraApp* app = context;
    lora_receiver_set_view_callback(app->receiver, lora_scene_receive_mode_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraAppReceiverView);
}

bool lora_scene_receive_mode_on_event(void* context, SceneManagerEvent event) {
    LoraApp* app = context;
    UNUSED(app);
    bool consumed = false;
    FURI_LOG_D("lora_scene_receive_mode_on_event", "Event type: %d", event.type);
    FURI_LOG_D("lora_scene_receive_mode_on_event", "Event: %ld", event.event);
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoraReceiverEventConfig) {
            scene_manager_next_scene(app->scene_manager, LoraSceneReceiveModeCfg);
            consumed = true;
        } else if(event.event == LoraReceiverEventCfgSet) {
            consumed = true;
            /* *INDENT-OFF* */
            with_view_model(
                app->receiver->view,
                LoraReceiverModel * model,
                {
                    // Send the new configuration to the receiver
                    LoraConfigModel config_copy;
                    config_copy = model->config;
                    lora_transmitter_set_rf_test_config(app->transmitter, &config_copy);
                },
                false);
            /* *INDENT-ON* */
        }
    }

    return consumed;
}

void lora_scene_receive_mode_on_exit(void* context) {
    LoraApp* app = context;
    UNUSED(app);
}
