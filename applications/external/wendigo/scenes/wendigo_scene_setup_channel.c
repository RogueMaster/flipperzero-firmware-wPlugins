#include "../wendigo_app_i.h"

static const WendigoItem items[SETUP_CHANNEL_MENU_ITEMS] = {
    {"1", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"2", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"3", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"4", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"5", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"6", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"7", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"8", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"9", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"10", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"11", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"12", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"13", {"On", "Off"}, 2, NO_ACTION, OFF},
    {"14", {"On", "Off"}, 2, NO_ACTION, OFF},
}; // TODO: Determine whether 5GHz channels are available, add them if so

static const uint8_t CH_ON = 0;
static const uint8_t CH_OFF = 1;

/* I'm not a fan of UART Terminal recent design decisions - I should have based
   this project off Gravity rather than it.
   Instead of adding yet another application variable to track selected menu item
   use a static variable here */
static uint8_t selected_menu_index = 0;

static void wendigo_scene_setup_channel_var_list_enter_callback(void* context, uint32_t index) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_channel_var_list_enter_callback()");
    UNUSED(context);
    UNUSED(index);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_channel_var_list_enter_callback()");
}

static void wendigo_scene_setup_channel_var_list_change_callback(VariableItem* item) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_channel_var_list_change_callback()");
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    selected_menu_index = variable_item_list_get_selected_item_index(app->var_item_list);
    furi_assert(selected_menu_index < SETUP_CHANNEL_MENU_ITEMS);

    const WendigoItem* menu_item = &items[selected_menu_index];
    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < menu_item->num_options_menu);
    variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);

    /* Store selected channel state */
    if(item_index == CH_ON) {
        app->channel_mask = app->channel_mask | app->CH_MASK[selected_menu_index + 1];
    } else {
        /* Eck. This got less elegant.
           If channel currently ON, disable it by subtracting that channel's bit value */
        if((app->channel_mask & app->CH_MASK[selected_menu_index + 1]) ==
           app->CH_MASK[selected_menu_index + 1]) {
            app->channel_mask -= app->CH_MASK[selected_menu_index + 1];
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_channel_var_list_change_callback()");
}

void wendigo_scene_setup_channel_on_enter(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_channel_on_enter()");
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    app->current_view = WendigoAppViewSetupChannel;
    variable_item_list_reset(var_item_list);
    variable_item_list_set_header(var_item_list, NULL);
    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_setup_channel_var_list_enter_callback, app);

    VariableItem* item;
    for(uint8_t i = 0; i < SETUP_CHANNEL_MENU_ITEMS; ++i) {
        item = variable_item_list_add(
            var_item_list,
            items[i].item_string,
            items[i].num_options_menu,
            wendigo_scene_setup_channel_var_list_change_callback,
            app);

        /* Use channels array and bitmask to determine current option value
         *  I'd rather use the ternary operator but this is more readable and I think they'll be
         *  optimised down to the same code */
        uint16_t ch_value;
        if((app->channel_mask & app->CH_MASK[i + 1]) == app->CH_MASK[i + 1]) {
            ch_value = CH_ON;
        } else {
            ch_value = CH_OFF;
        }
        variable_item_set_current_value_index(item, ch_value);
        variable_item_set_current_value_text(item, items[i].options_menu[ch_value]);
    }
    variable_item_list_set_selected_item(var_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_channel_on_enter()");
}

bool wendigo_scene_setup_channel_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_channel_on_event()");
    UNUSED(context);
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->setup_selected_menu_index =
            variable_item_list_get_selected_item_index(app->var_item_list);
        consumed = true;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_channel_on_event()");
    return consumed;
}

void wendigo_scene_setup_channel_on_exit(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_channel_on_exit()");
    WendigoApp* app = context;
    variable_item_list_reset(app->var_item_list);
    /* Update ESP32-Wendigo's channels when the scene is exited */
    char channel_cmd[149];
    char temp_ch[4];
    bzero(channel_cmd, 149);
    uint8_t cmd_idx = 0;
    channel_cmd[cmd_idx++] = 'c';
    for(uint8_t i = 0; i < SETUP_CHANNEL_MENU_ITEMS; ++i) {
        /* Add this channel to channel_cmd if it's selected (or if all channels selected) */
        if(((app->channel_mask & app->CH_MASK[i + 1]) == app->CH_MASK[i + 1]) ||
           ((app->channel_mask & CH_MASK_ALL) == CH_MASK_ALL)) {
            // TODO: This only works for 2.4GHz channels
            snprintf(temp_ch, sizeof(temp_ch), " %d", i + 1);
            for(uint8_t j = 0; j < strlen(temp_ch); ++j) {
                channel_cmd[cmd_idx++] = temp_ch[j];
            }
        }
    }
    channel_cmd[cmd_idx++] = '\n';
    wendigo_uart_set_binary_cb(app->uart);
    wendigo_uart_tx(app->uart, (uint8_t*)channel_cmd, strlen(channel_cmd) + 1);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_channel_on_exit()");
}
