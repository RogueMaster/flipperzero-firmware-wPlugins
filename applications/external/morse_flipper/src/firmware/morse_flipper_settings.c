/*
 * Purpose: Provide shared settings-list helpers and scene glue.
 * Owns: public VariableItemList state helpers and common settings lifecycle code.
 * Depends on: morse_flipper_app_i.h and split settings modules.
 * Tests: firmware build; settings UI is hardware-only.
 */

#include "morse_flipper_app_i.h"

uint32_t morse_flipper_settings_list_state(VariableItemList* list) {
    if(list == NULL) return 0U;
    return variable_item_list_get_selected_item_index(list);
}

void morse_flipper_settings_list_restore(VariableItemList* list, uint32_t state) {
    if(list == NULL) return;
    variable_item_list_set_selected_item(list, (uint8_t)(state & 0xffU));
}

void morse_flipper_settings_enter_callback(void* context, uint32_t index) {
    MorseFlipperApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void morse_flipper_settings_noop_enter(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}
