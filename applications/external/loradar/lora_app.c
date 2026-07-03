#include "lora_app.h"
#include "scenes/lora_scene.h"

#include <gui/modules/submenu.h>

static bool lora_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    LoraApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool lora_app_back_event_callback(void* context) {
    furi_assert(context);
    LoraApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static LoraApp* lora_app_alloc() {
    LoraApp* app = malloc(sizeof(LoraApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->scene_manager = scene_manager_alloc(&lora_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, lora_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, lora_app_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoraAppSubMenuView, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        LoraAppReceiverCfgView,
        variable_item_list_get_view(app->var_item_list));
    // Allocate a Bluetooth transmitter
    app->bt_transmitter = bt_transmitter_alloc();

    // Allocate a serial transmitter object
    UartHelper* uart_helper = uart_helper_alloc(DEVICE_BAUDRATE, app);
    LoraTransmitterMethod send_method = (LoraTransmitterMethod)uart_helper_send;

    LoraTransmitterContextDestructor context_destructor =
        (LoraTransmitterContextDestructor)uart_helper_free;

    app->transmitter = lora_transmitter_alloc(
        uart_helper, send_method, context_destructor, uart_helper_set_transmitter_thread_id);

    // Allocate a receiver object
    app->receiver = lora_receiver_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoraAppReceiverView, lora_receiver_get_view(app->receiver));

    app->state_manager = lora_state_manager_alloc(app->receiver);

    // Set State Manager
    lora_receiver_set_state_manager(app->receiver, app->state_manager);
    lora_transmitter_set_state_manager(app->transmitter, app->state_manager);
    bt_transmitter_set_state_manager(app->bt_transmitter, app->state_manager);

#ifdef DEMO_PROCESS_LINE
    uart_helper_set_delimiter(uart_helper, LINE_DELIMITER, INCLUDE_LINE_DELIMITER);
#else
    app->timer = furi_timer_alloc(uart_demo_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 1000);
#endif
    scene_manager_next_scene(app->scene_manager, LoraSceneStart);
    return app;
}

static void lora_app_free(LoraApp* app) {
    if(app->timer) {
        furi_timer_free(app->timer);
    }

    view_dispatcher_remove_view(app->view_dispatcher, LoraAppSubMenuView);
    view_dispatcher_remove_view(app->view_dispatcher, LoraAppReceiverView);
    view_dispatcher_remove_view(app->view_dispatcher, LoraAppReceiverCfgView);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);
    submenu_free(app->submenu);
    lora_state_manager_free(app->state_manager);

    // Free all objects
    bt_transmitter_free(app->bt_transmitter);
    lora_receiver_free(app->receiver);
    lora_transmitter_free(app->transmitter);

    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t lora_app_main(void* p) {
    UNUSED(p);

    LoraApp* app = lora_app_alloc();
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources
    lora_app_free(app);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    return 0;
}
