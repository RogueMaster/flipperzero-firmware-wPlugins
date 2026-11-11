#include "../ir_maker.h"

typedef enum {
    IrMakerDefaultsItemProtocol,
    IrMakerDefaultsItemAddress,
    IrMakerDefaultsItemStart,
} IrMakerDefaultsItem;

static void ir_maker_scene_defaults_protocol_changed(VariableItem* item) {
    IrMakerApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    app->def_protocol = ir_maker_valid_protocol_at(index);
    variable_item_set_current_value_text(item, infrared_get_protocol_name(app->def_protocol));

    /* Keep the default address within the new protocol's address width. */
    const uint8_t bits = infrared_get_protocol_address_length(app->def_protocol);
    const uint32_t mask = (bits >= 32U) ? 0xFFFFFFFFUL : ((1UL << bits) - 1UL);
    app->def_address &= mask;
}

static void ir_maker_scene_defaults_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ir_maker_scene_defaults_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    VariableItemList* list = app->var_item_list;

    variable_item_list_reset(list);

    VariableItem* item = variable_item_list_add(
        list,
        "Protocol",
        (uint8_t)ir_maker_valid_protocol_count(),
        ir_maker_scene_defaults_protocol_changed,
        app);
    variable_item_set_current_value_index(
        item, (uint8_t)ir_maker_valid_protocol_index(app->def_protocol));
    variable_item_set_current_value_text(item, infrared_get_protocol_name(app->def_protocol));

    const uint8_t addr_bits = infrared_get_protocol_address_length(app->def_protocol);
    char addr_str[12];
    snprintf(
        addr_str,
        sizeof(addr_str),
        "0x%0*lX",
        (int)ROUND_UP_TO(addr_bits, 4),
        (unsigned long)app->def_address);
    item = variable_item_list_add(list, "Address", 1, NULL, app);
    variable_item_set_current_value_text(item, addr_str);

    variable_item_list_add(list, "Start building", 1, NULL, app);

    variable_item_list_set_enter_callback(list, ir_maker_scene_defaults_enter_callback, app);
    variable_item_list_set_selected_item(
        list, (uint8_t)scene_manager_get_scene_state(app->scene_manager, IrMakerSceneDefaults));

    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewVarItemList);
}

bool ir_maker_scene_defaults_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, IrMakerSceneDefaults, event.event);
        if(event.event == IrMakerDefaultsItemAddress) {
            app->hex_target = IrMakerHexTargetDefaultAddress;
            scene_manager_next_scene(app->scene_manager, IrMakerSceneEditHex);
            consumed = true;
        } else if(event.event == IrMakerDefaultsItemStart) {
            /* Re-entered from ButtonList: return to it. First visit: push it. */
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, IrMakerSceneButtonList)) {
                scene_manager_next_scene(app->scene_manager, IrMakerSceneButtonList);
            }
            consumed = true;
        }
    }

    return consumed;
}

void ir_maker_scene_defaults_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
