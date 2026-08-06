#include "../app_user.h"
#include <stdio.h>

/**
 * The main menu is the first scene to see in the Ethernet App
 * here the user selects an option that wants to do.
 */

// Time to show the LOGO. F0.7 — was 1000 ms. The splash blocks the GUI
// thread for the entire delay and runs once on first entry to the
// main menu. 250 ms is still legible as a splash but feels responsive.
const uint32_t time_showing = 250;

// List for the menu options
// Order follows the natural network-audit flow:
//   setup (Get IP) → discovery (Scan Hosts) → recon (Ping/Ports/OS)
//   → attack (ARP Actions) → capture/analyze (Sniffer/Read Pcaps)
//   → admin (Settings/About).

enum {
    ADMIN_MENU_OPTION,
    PENTEST_MENU_OPTION,
    SETTINGS_OPTION,
    ABOUT_US_OPTION,
};

// Function to display init at the start of the app
void draw_start(App* app) {
    widget_reset(app->widget);

    widget_add_icon_element(app->widget, 40, 1, &I_EC48x26);
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignCenter, FontPrimary, APP_NAME);
    widget_add_string_element(
        app->widget, 64, 55, AlignCenter, AlignCenter, FontSecondary, "Electronic Cats");

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

    furi_delay_ms(time_showing);
}

static void main_category_menu_callback(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case ADMIN_MENU_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_main_menu_option);
        break;

    case PENTEST_MENU_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_pentest_menu_option);
        break;

    case SETTINGS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_settings_option);
        break;

    case ABOUT_US_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_about_us_option);
        break;

    default:
        break;
    }
}

void app_scene_main_category_menu_on_enter(void* context) {
    App* app = context;

    // Variable used to show the EC logo once
    static bool is_logo_shown = false;
    if(!is_logo_shown) draw_start(app);

    // F0.4c — the resume-on-entry guard is obsolete: scenes no longer
    // suspend app->thread (rx_dispatch + scanner_session own the chip).

    is_logo_shown = true;

    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "ETHERNET APP");

    submenu_add_item(
        app->submenu, "Administration", ADMIN_MENU_OPTION, main_category_menu_callback, app);

    submenu_add_item(
        app->submenu, "Pentesting", PENTEST_MENU_OPTION, main_category_menu_callback, app);

    submenu_add_item(app->submenu, "Settings", SETTINGS_OPTION, main_category_menu_callback, app);

    submenu_add_item(app->submenu, "About Us", ABOUT_US_OPTION, main_category_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

bool app_scene_main_category_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void app_scene_main_category_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
