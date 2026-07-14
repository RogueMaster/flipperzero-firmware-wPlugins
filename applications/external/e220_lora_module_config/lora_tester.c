#include "lora_tester_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#define TAG "LoRaTester"

const char* lora_mode_names[] = {"Normal", "WOR Tx", "WOR Rx", "Config"};
const uint32_t baud_rates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
const uint8_t baud_rate_count = sizeof(baud_rates) / sizeof(baud_rates[0]);

void lora_tester_app_free(LoraTesterApp* app);

static bool lora_tester_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    LoraTesterApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool lora_tester_app_back_event_callback(void* context) {
    furi_assert(context);
    LoraTesterApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void lora_tester_app_tick_event_callback(void* context) {
    furi_assert(context);
    LoraTesterApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

void lora_tester_set_mode(LoraTesterApp* app, LoRaMode mode) {
    app->current_mode = mode;

    // Set GPIO pins A6 (M0) and A7 (M1)
    switch(mode) {
    case LoRaMode_Normal:
        furi_hal_gpio_write(&gpio_ext_pa6, false);
        furi_hal_gpio_write(&gpio_ext_pa7, false);
        break;
    case LoRaMode_WOR_Transmit:
        furi_hal_gpio_write(&gpio_ext_pa6, true);
        furi_hal_gpio_write(&gpio_ext_pa7, false);
        break;
    case LoRaMode_WOR_Receive:
        furi_hal_gpio_write(&gpio_ext_pa6, false);
        furi_hal_gpio_write(&gpio_ext_pa7, true);
        break;
    case LoRaMode_Config:
        furi_hal_gpio_write(&gpio_ext_pa6, true);
        furi_hal_gpio_write(&gpio_ext_pa7, true);
        break;
    }

    FURI_LOG_I(
        "LoRaTester",
        "Mode set to: %s, A6 (M0): %s, A7 (M1): %s",
        lora_mode_names[mode],
        furi_hal_gpio_read(&gpio_ext_pa6) ? "HIGH" : "LOW",
        furi_hal_gpio_read(&gpio_ext_pa7) ? "HIGH" : "LOW");
}

static void lora_tester_mode_changed(VariableItem* item) {
    LoraTesterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    lora_tester_set_mode(app, (LoRaMode)index);

    const char* mode_names[] = {"Normal", "WOR Tx", "WOR Rx", "Config"};
    variable_item_set_current_value_text(item, mode_names[index]);
}

static void lora_tester_baud_rate_changed(VariableItem* item) {
    LoraTesterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->baud_rate = baud_rates[index];
    char baud_str[16];
    snprintf(baud_str, sizeof(baud_str), "%lu bps", app->baud_rate);
    variable_item_set_current_value_text(item, baud_str);
}

LoraTesterApp* lora_tester_app_alloc() {
    LoraTesterApp* app = malloc(sizeof(LoraTesterApp));

    if(app == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to allocate memory for app");
        return NULL;
    }

    // Initialize all pointers to NULL
    memset(app, 0, sizeof(LoraTesterApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);

    if(app->gui == NULL || app->dialogs == NULL || app->storage == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to open required records");
        lora_tester_app_free(app);
        return NULL;
    }

    app->view_dispatcher = view_dispatcher_alloc();
    if(app->view_dispatcher == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to allocate view dispatcher");
        lora_tester_app_free(app);
        return NULL;
    }

    app->scene_manager = scene_manager_alloc(&lora_tester_scene_handlers, app);
    if(app->scene_manager == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to allocate scene manager");
        lora_tester_app_free(app);
        return NULL;
    }

    // Allocate popup
    app->popup = popup_alloc();
    if(app->popup == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to allocate popup");
        lora_tester_app_free(app);
        return NULL;
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, lora_tester_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, lora_tester_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, lora_tester_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        LoraTesterAppViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoraTesterAppViewTextBox, text_box_get_view(app->text_box));
    app->text_box_store = furi_string_alloc();
    furi_string_reserve(app->text_box_store, LORA_TESTER_TEXT_BOX_STORE_SIZE);

    app->text_input = uart_text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        LoraTesterAppViewTextInput,
        uart_text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoraTesterAppViewWidget, widget_get_view(app->widget));

    view_dispatcher_add_view(
        app->view_dispatcher, LoraTesterAppViewPopup, popup_get_view(app->popup));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoraTesterAppViewByteInput, byte_input_get_view(app->byte_input));

    app->file_path = furi_string_alloc();
    app->receive_context = NULL;
    app->worker_thread = NULL;

    // Initialize GPIO pins and serial
    furi_hal_gpio_init_simple(&gpio_ext_pa6, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_ext_pa7, GpioModeOutputPushPull);
    lora_tester_set_mode(app, LoRaMode_Normal);
    app->baud_rate = 9600;

    VariableItem* mode_item =
        variable_item_list_add(app->var_item_list, "LoRa Mode", 4, lora_tester_mode_changed, app);
    variable_item_set_current_value_index(mode_item, 0);
    variable_item_set_current_value_text(mode_item, "Normal");

    VariableItem* baud_item = variable_item_list_add(
        app->var_item_list, "Baud Rate", baud_rate_count, lora_tester_baud_rate_changed, app);

    uint8_t default_baud_index = 0;
    for(uint8_t i = 0; i < baud_rate_count; i++) {
        if(baud_rates[i] == 9600) {
            default_baud_index = i;
            break;
        }
    }

    variable_item_set_current_value_index(baud_item, default_baud_index);
    char baud_str[16];
    snprintf(baud_str, sizeof(baud_str), "%lu bps", app->baud_rate);
    variable_item_set_current_value_text(baud_item, baud_str);

    app->address = 0;

    scene_manager_next_scene(app->scene_manager, LoraTesterSceneStart);

    FURI_LOG_I("LoRaTester", "App allocation completed successfully");
    return app;
}

void lora_tester_app_free(LoraTesterApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, LoraTesterAppViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, LoraTesterAppViewTextBox);

    view_dispatcher_remove_view(app->view_dispatcher, LoraTesterAppViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, LoraTesterAppViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, LoraTesterAppViewPopup);

    variable_item_list_free(app->var_item_list);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    uart_text_input_free(app->text_input);
    widget_free(app->widget);
    popup_free(app->popup);
    furi_string_free(app->file_path);
    view_dispatcher_remove_view(app->view_dispatcher, LoraTesterAppViewByteInput);
    byte_input_free(app->byte_input);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    free(app);
}

int32_t lora_tester_app(void* p) {
    FURI_LOG_D(TAG, "Enter: lora_tester_app");
    UNUSED(p);
    LoraTesterApp* lora_tester_app = lora_tester_app_alloc();
    furi_assert(lora_tester_app);

    FURI_LOG_D(TAG, "Starting view dispatcher");
    view_dispatcher_run(lora_tester_app->view_dispatcher);
    FURI_LOG_D(TAG, "View dispatcher finished");

    lora_tester_app_free(lora_tester_app);
    FURI_LOG_D(TAG, "Exit: lora_tester_app");
    return 0;
}
