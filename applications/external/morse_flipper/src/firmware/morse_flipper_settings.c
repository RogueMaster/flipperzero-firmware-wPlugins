/*
 * Purpose: Provide shared settings-list helpers and scene glue.
 * Owns: VariableItemList state mirroring and common settings lifecycle code.
 * Depends on: morse_flipper_app_i.h and split settings modules.
 * Tests: firmware build; settings UI is hardware-only.
 */

#include "morse_flipper_app_i.h"

uint32_t morse_flipper_settings_list_state(VariableItemList* list) {
    MorseFlipperVilModel* model;
    View* view;
    uint8_t row;
    uint32_t state;

    if(list == NULL) return 0U;

    view = variable_item_list_get_view(list);
    model = view_get_model(view);
    row = model->position >= model->window_position ?
              (uint8_t)(model->position - model->window_position) :
              0U;
    if(row > 3U) row = 3U;
    state = (uint32_t)model->position | ((uint32_t)row << 8);
    view_commit_model(view, false);
    return state;
}

void morse_flipper_settings_list_restore(VariableItemList* list, uint32_t state) {
    View* view;
    uint8_t pos;
    uint8_t row;

    if(list == NULL) return;

    pos = (uint8_t)(state & 0xffU);
    row = (uint8_t)((state >> 8) & 0xffU);
    variable_item_list_set_selected_item(list, pos);

    view = variable_item_list_get_view(list);
    with_view_model(
        view,
        MorseFlipperVilModel * model,
        {
            uint8_t count = MorseFlipperVilArray_size(model->items);
            uint8_t win = 0U;

            if(count == 0U) {
                model->position = 0U;
                model->window_position = 0U;
            } else {
                if(pos >= count) pos = 0U;
                if(row > 3U) row = 1U;
                if(row > pos) row = pos;
                if(count > 4U) {
                    uint8_t max_win = (uint8_t)(count - 4U);
                    win = (uint8_t)(pos - row);
                    if(win > max_win) win = max_win;
                }
                model->position = pos;
                model->window_position = win;
            }
        },
        true);
}

void morse_flipper_settings_enter_callback(void* context, uint32_t index) {
    MorseFlipperApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void morse_flipper_settings_noop_enter(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}
