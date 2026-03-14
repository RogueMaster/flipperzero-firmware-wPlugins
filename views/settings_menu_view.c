/*
 * settings_menu_view.c — top-level settings submenu.
 * CO2 Sensor / Climate Sensor / Settings / Help / About
 */
#include "../air_stats_i.h"
#include <gui/modules/submenu.h>

static Submenu* submenu;

#define VIEW_ID ViewSettingsMenu

typedef enum {
    SubmenuItemCO2Sensor = 0,
    SubmenuItemClimateSensor,
    SubmenuItemSettings,
    SubmenuItemHelp,
    SubmenuItemAbout,
} SettingsMenuIndex;

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    return ViewMainMenu;
}

static void _item_callback(void* context, uint32_t index) {
    UNUSED(context);
    switch(index) {
    case SubmenuItemCO2Sensor:
        view_co2_settings_switch();
        break;
    case SubmenuItemClimateSensor:
        view_climate_settings_switch();
        break;
    case SubmenuItemSettings:
        view_settings_switch();
        break;
    case SubmenuItemHelp:
        view_widget_help_from_settings_switch();
        break;
    case SubmenuItemAbout:
        view_widget_about_from_settings_switch();
        break;
    }
}

void view_settings_menu_alloc(void) {
    submenu = submenu_alloc();
    submenu_add_item(submenu, "CO2 Sensor",    SubmenuItemCO2Sensor,     _item_callback, app);
    submenu_add_item(submenu, "Climate Sensor", SubmenuItemClimateSensor, _item_callback, app);
    submenu_add_item(submenu, "Settings",       SubmenuItemSettings,      _item_callback, app);
    submenu_add_item(submenu, "Help",           SubmenuItemHelp,          _item_callback, app);
    submenu_add_item(submenu, "About",          SubmenuItemAbout,         _item_callback, app);
    view_set_previous_callback(submenu_get_view(submenu), _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, submenu_get_view(submenu));
}

void view_settings_menu_switch(void) {
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_settings_menu_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    submenu_free(submenu);
}
