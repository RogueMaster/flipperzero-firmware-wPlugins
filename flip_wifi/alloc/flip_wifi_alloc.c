#include <callback/callback.h>

// Function to allocate resources for the FlipWiFiApp
FlipWiFiApp *flip_wifi_app_alloc()
{
    FlipWiFiApp *app = (FlipWiFiApp *)malloc(sizeof(FlipWiFiApp));

    Gui *gui = furi_record_open(RECORD_GUI);

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app))
    {
        return NULL;
    }

    // Submenu
    if (!easy_flipper_set_submenu(&app->submenu_main, FlipWiFiViewSubmenuMain, VERSION_TAG, callback_exit_app, &app->view_dispatcher))
    {
        return NULL;
    }
    submenu_add_item(app->submenu_main, "Scan", FlipWiFiSubmenuIndexWiFiScan, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "Deauthentication", FlipWiFiSubmenuIndexWiFiDeauth, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "Captive Portal", FlipWiFiSubmenuIndexWiFiAP, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "Saved APs", FlipWiFiSubmenuIndexWiFiSaved, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "Commands", FlipWiFiSubmenuIndexCommands, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "Info", FlipWiFiSubmenuIndexAbout, callback_submenu_choices, app);

    app->fhttp = NULL;

    // Switch to the main view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);

    return app;
}