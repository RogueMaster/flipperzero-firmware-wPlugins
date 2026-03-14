/*
 * main_menu_view.c — simple menu with "Settings" item.
 * Adapted from _ref_unitemp/views/MainMenu_view.c
 */
#include "../co2_app_i.h"
#include <gui/modules/variable_item_list.h>

static View* view;
static VariableItemList* variable_item_list;

#define VIEW_ID ViewMainMenu

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    return ViewMain;
}

static void _enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    if(index == 0) {
        view_settings_switch();
    }
}

void view_main_menu_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);

    variable_item_list_add(variable_item_list, "Settings", 1, NULL, NULL);

    variable_item_list_set_enter_callback(variable_item_list, _enter_callback, app);

    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_main_menu_switch(void) {
    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_main_menu_free(void) {
    variable_item_list_free(variable_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
}
