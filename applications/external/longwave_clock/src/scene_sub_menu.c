#include "flipper.h"

#include "app_state.h"
#include "scene_sub_menu.h"
#include "scenes.h"
#include "module_lights.h"

#define START_ITEM     0
#define RUN_MODE_ITEM  1
#define DATA_MODE_ITEM 2
#define DATA_PIN_ITEM  3

static char* run_mode_names[] = {"demo", "GPIO"};
static char* data_mode_names[] = {"normal", "inverted"};
static char* data_pin_names[] = {"A7", "A4", "B2", "C1", "C0"};

#ifdef FW_ORIGIN_Momentum
static char* GPIO_ONLY = "GPIO mode\nonly!";
static char* run_mode_start_text[] = {"Start the simulation", "Start the receiver"};
#else
static char* start_mode_text = "Start in selected mode";
#endif

void lwc_run_mode_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    ProtoConfig* config = lwc_get_protocol_config(app->state);

    uint8_t index = variable_item_get_current_value_index(item);
    config->run_mode = (LWCRunMode)(index);
    variable_item_set_current_value_text(item, run_mode_names[index]);

#ifdef FW_ORIGIN_Momentum
    VariableItem* start = variable_item_list_get(app->sub_menu, START_ITEM);
    VariableItem* data_mode = variable_item_list_get(app->sub_menu, DATA_MODE_ITEM);
    VariableItem* data_pin = variable_item_list_get(app->sub_menu, DATA_PIN_ITEM);

    variable_item_set_locked(data_mode, (LWCRunMode)(index) == Demo, GPIO_ONLY);
    variable_item_set_locked(data_pin, (LWCRunMode)(index) == Demo, GPIO_ONLY);
    variable_item_set_item_label(start, run_mode_start_text[index]);
#endif
}

void lwc_data_mode_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    ProtoConfig* config = lwc_get_protocol_config(app->state);

    uint8_t index = variable_item_get_current_value_index(item);
    config->data_mode = (LWCDataMode)(index);
    variable_item_set_current_value_text(item, data_mode_names[index]);
}

void lwc_data_pin_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    ProtoConfig* config = lwc_get_protocol_config(app->state);

    uint8_t index = variable_item_get_current_value_index(item);
    config->data_pin = (LWCDataPin)(index);
    variable_item_set_current_value_text(item, data_pin_names[index]);
}

void lwc_enter_item_callback(void* context, uint32_t index) {
    App* app = context;

    if(index == START_ITEM) {
        store_proto_config(app->state);
        lwc_app_backlight_on_persist(app);
        scene_manager_next_scene(app->scene_manager, lwc_get_start_scene_for_protocol(app->state));
    }
}

void lwc_sub_menu_scene_on_enter(void* context) {
    App* app = context;

    lwc_app_backlight_on_reset(app);

    ProtoConfig* config = lwc_get_protocol_config(app->state);

#ifdef FW_ORIGIN_Momentum
    variable_item_list_add(app->sub_menu, run_mode_start_text[config->run_mode], 0, NULL, app);
#else
    variable_item_list_add(app->sub_menu, start_mode_text, 0, NULL, app);
#endif

    variable_item_list_set_enter_callback(app->sub_menu, lwc_enter_item_callback, app);

    VariableItem* run_mode = variable_item_list_add(
        app->sub_menu, "Run mode", __lwc_number_of_run_modes, lwc_run_mode_change_callback, app);

    variable_item_set_current_value_index(run_mode, config->run_mode);
    variable_item_set_current_value_text(run_mode, run_mode_names[config->run_mode]);

    VariableItem* data_mode = variable_item_list_add(
        app->sub_menu, "GPIO data", __lwc_number_of_data_modes, lwc_data_mode_change_callback, app);

    variable_item_set_current_value_index(data_mode, config->data_mode);
    variable_item_set_current_value_text(data_mode, data_mode_names[config->data_mode]);

    VariableItem* data_pin = variable_item_list_add(
        app->sub_menu, "Data pin", __lwc_number_of_data_pins, lwc_data_pin_change_callback, app);

    variable_item_set_current_value_index(data_pin, config->data_pin);
    variable_item_set_current_value_text(data_pin, data_pin_names[config->data_pin]);

#ifdef FW_ORIGIN_Momentum
    variable_item_set_locked(data_mode, config->run_mode == Demo, GPIO_ONLY);
    variable_item_set_locked(data_pin, config->run_mode == Demo, GPIO_ONLY);

    variable_item_list_set_header(app->sub_menu, get_protocol_name(app->state->lwc_type));
#endif

    view_dispatcher_switch_to_view(app->view_dispatcher, LWCSubMenuView);
}

/** main menu event handler - switches scene based on the event */
bool lwc_sub_menu_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void lwc_sub_menu_scene_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->sub_menu);
}
