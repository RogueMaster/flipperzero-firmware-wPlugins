#include "../clippy_app_i.h"

enum SubmenuIndex {
    ConfigIndexKeyboardLayout,
    ConfigIndexInterface,
    ConfigIndexDelay,
    ConfigIndexBleUnpair,
};

const char* const interface_mode_text[2] = {
    "USB",
    "BLE",
};

const char* const delay_text[4] = {
    "25ms",
    "50ms",
    "100ms",
    "200ms",
};

void clippy_scene_config_select_callback(void* context, uint32_t index) {
    ClippyApp* clippy = context;
    if(index != ConfigIndexInterface) {
        view_dispatcher_send_custom_event(clippy->view_dispatcher, index);
    }
}

void clippy_scene_config_interface_callback(VariableItem* item) {
    ClippyApp* clippy = variable_item_get_context(item);
    furi_assert(clippy);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, interface_mode_text[index]);
    clippy->interface = index;

    view_dispatcher_send_custom_event(clippy->view_dispatcher, ConfigIndexInterface);
}

void clippy_scene_config_delay_callback(VariableItem* item) {
    ClippyApp* clippy = variable_item_get_context(item);
    furi_assert(clippy);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, delay_text[index]);
    clippy->delay = index;

    view_dispatcher_send_custom_event(clippy->view_dispatcher, ConfigIndexInterface);
}

static void draw_menu(ClippyApp* clippy) {
    VariableItemList* var_item_list = clippy->variable_item_list;

    variable_item_list_reset(var_item_list);

    variable_item_list_add(var_item_list, "Keyboard Layout (global)", 0, NULL, NULL);

    VariableItem* item = variable_item_list_add(
        var_item_list, "Delay", 4, clippy_scene_config_delay_callback, clippy);
    variable_item_set_current_value_index(item, clippy->delay);
    variable_item_set_current_value_text(item, delay_text[clippy->delay]);

    item = variable_item_list_add(
        var_item_list, "Interface", 2, clippy_scene_config_interface_callback, clippy);
    if(clippy->interface == BadUsbHidInterfaceUsb) {
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, interface_mode_text[0]);
    } else {
        variable_item_set_current_value_index(item, 1);
        variable_item_set_current_value_text(item, interface_mode_text[1]);
        variable_item_list_add(var_item_list, "Remove Pairing", 0, NULL, NULL);
    }
}

void clippy_scene_config_on_enter(void* context) {
    ClippyApp* clippy = context;
    VariableItemList* var_item_list = clippy->variable_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, clippy_scene_config_select_callback, clippy);
    draw_menu(clippy);
    variable_item_list_set_selected_item(var_item_list, 0);

    view_dispatcher_switch_to_view(clippy->view_dispatcher, ClippyAppViewBadUsbConfig);
}

bool clippy_scene_config_on_event(void* context, SceneManagerEvent event) {
    ClippyApp* clippy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == ConfigIndexKeyboardLayout) {
            scene_manager_next_scene(clippy->scene_manager, ClippySceneConfigLayout);
        } else if(event.event == ConfigIndexInterface) {
            draw_menu(clippy);
        } else if(event.event == ConfigIndexBleUnpair) {
            bad_usb_hid_ble_remove_pairing();
        } else {
            furi_crash("Unknown key type");
        }
    }

    return consumed;
}

void clippy_scene_config_on_exit(void* context) {
    ClippyApp* clippy = context;
    VariableItemList* var_item_list = clippy->variable_item_list;

    variable_item_list_reset(var_item_list);
}
