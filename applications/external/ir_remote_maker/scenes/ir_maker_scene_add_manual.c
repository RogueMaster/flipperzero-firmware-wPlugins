#include "../ir_maker.h"

typedef enum {
    IrMakerAddItemProtocol,
    IrMakerAddItemAddress,
    IrMakerAddItemCommand,
    IrMakerAddItemTest,
    IrMakerAddItemAdd,
} IrMakerAddItem;

/* Custom event sent by the transient error popup to return to the editor. */
#define IR_MAKER_ADD_EVENT_ERROR_DISMISS 100U

static bool ir_maker_message_in_range(const InfraredMessage* msg) {
    if(!infrared_is_protocol_valid(msg->protocol)) return false;
    const uint8_t addr_bits = infrared_get_protocol_address_length(msg->protocol);
    const uint8_t cmd_bits = infrared_get_protocol_command_length(msg->protocol);
    const uint32_t addr_mask = (addr_bits >= 32U) ? 0xFFFFFFFFUL : ((1UL << addr_bits) - 1UL);
    const uint32_t cmd_mask = (cmd_bits >= 32U) ? 0xFFFFFFFFUL : ((1UL << cmd_bits) - 1UL);
    return (msg->address == (msg->address & addr_mask)) &&
           (msg->command == (msg->command & cmd_mask));
}

static void ir_maker_scene_add_manual_protocol_changed(VariableItem* item) {
    IrMakerApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    app->edit_msg.protocol = ir_maker_valid_protocol_at(index);
    variable_item_set_current_value_text(item, infrared_get_protocol_name(app->edit_msg.protocol));
    /* New protocol, new masks: keep address/command inside them. */
    ir_maker_clamp_message(&app->edit_msg);
}

static void ir_maker_scene_add_manual_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void ir_maker_scene_add_manual_popup_callback(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, IR_MAKER_ADD_EVENT_ERROR_DISMISS);
}

static void ir_maker_scene_add_manual_populate(IrMakerApp* app) {
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    VariableItem* item = variable_item_list_add(
        list,
        "Protocol",
        (uint8_t)ir_maker_valid_protocol_count(),
        ir_maker_scene_add_manual_protocol_changed,
        app);
    variable_item_set_current_value_index(
        item, (uint8_t)ir_maker_valid_protocol_index(app->edit_msg.protocol));
    variable_item_set_current_value_text(item, infrared_get_protocol_name(app->edit_msg.protocol));

    char buf[12];
    const uint8_t addr_bits = infrared_get_protocol_address_length(app->edit_msg.protocol);
    snprintf(
        buf,
        sizeof(buf),
        "0x%0*lX",
        (int)ROUND_UP_TO(addr_bits, 4),
        (unsigned long)app->edit_msg.address);
    item = variable_item_list_add(list, "Address", 1, NULL, app);
    variable_item_set_current_value_text(item, buf);

    const uint8_t cmd_bits = infrared_get_protocol_command_length(app->edit_msg.protocol);
    snprintf(
        buf,
        sizeof(buf),
        "0x%0*lX",
        (int)ROUND_UP_TO(cmd_bits, 4),
        (unsigned long)app->edit_msg.command);
    item = variable_item_list_add(list, "Command", 1, NULL, app);
    variable_item_set_current_value_text(item, buf);

    variable_item_list_add(list, "Test", 1, NULL, app);
    variable_item_list_add(list, "Add", 1, NULL, app);

    variable_item_list_set_enter_callback(list, ir_maker_scene_add_manual_enter_callback, app);
}

static void ir_maker_scene_add_manual_show_error(IrMakerApp* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "Out of range", 64, 12, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Value too big\nfor this protocol", 64, 34, AlignCenter, AlignTop);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, ir_maker_scene_add_manual_popup_callback);
    popup_set_timeout(app->popup, 1500);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewPopup);
}

void ir_maker_scene_add_manual_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    /* When editing an existing button's signal, prefill from it. */
    if(app->edit_mode == IrMakerEditModeSignal) {
        const IrMakerButton* button = &app->draft->items[app->selected_button];
        if(!infrared_signal_is_raw(button->signal)) {
            app->edit_msg = *infrared_signal_get_message(button->signal);
        }
    }

    ir_maker_scene_add_manual_populate(app);
    variable_item_list_set_selected_item(app->var_item_list, IrMakerAddItemProtocol);
    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewVarItemList);
}

bool ir_maker_scene_add_manual_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case IrMakerAddItemAddress:
            app->hex_target = IrMakerHexTargetButtonAddress;
            scene_manager_next_scene(app->scene_manager, IrMakerSceneEditHex);
            consumed = true;
            break;
        case IrMakerAddItemCommand:
            app->hex_target = IrMakerHexTargetButtonCommand;
            scene_manager_next_scene(app->scene_manager, IrMakerSceneEditHex);
            consumed = true;
            break;
        case IrMakerAddItemTest:
            if(ir_maker_message_in_range(&app->edit_msg)) {
                infrared_signal_set_message(app->pending, &app->edit_msg);
                ir_maker_send_test(app->pending);
                notification_message(app->notifications, &sequence_blink_cyan_10);
            } else {
                ir_maker_scene_add_manual_show_error(app);
            }
            consumed = true;
            break;
        case IrMakerAddItemAdd:
            if(ir_maker_message_in_range(&app->edit_msg)) {
                infrared_signal_set_message(app->pending, &app->edit_msg);
                scene_manager_next_scene(app->scene_manager, IrMakerSceneButtonName);
            } else {
                ir_maker_scene_add_manual_show_error(app);
            }
            consumed = true;
            break;
        case IR_MAKER_ADD_EVENT_ERROR_DISMISS:
            /* Error popup timed out: rebuild the editor and return to it. */
            popup_reset(app->popup);
            ir_maker_scene_add_manual_populate(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewVarItemList);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void ir_maker_scene_add_manual_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    variable_item_list_reset(app->var_item_list);
    popup_reset(app->popup);
}
