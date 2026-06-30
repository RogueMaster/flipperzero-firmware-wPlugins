#include <alloc/alloc.h>
#include <callback/callback.h>
#include <callback/loader.h>
#include <callback/free.h>

uint32_t callback_exit_app(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

uint32_t callback_to_submenu(void *context)
{
    UNUSED(context);
    return FlipWorldViewSubmenu;
}

uint32_t callback_to_wifi_settings(void *context)
{
    UNUSED(context);
    return FlipWorldViewVariableItemList;
}
uint32_t callback_to_settings(void *context)
{
    UNUSED(context);
    return FlipWorldViewSubmenuOther;
}

void *global_app;
void flip_world_show_submenu()
{
    FlipWorldApp *app = (FlipWorldApp *)global_app;
    if (app->submenu)
    {
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);
    }
}

// Function to allocate resources for the FlipWorldApp
FlipWorldApp *flip_world_app_alloc()
{
    FlipWorldApp *app = (FlipWorldApp *)malloc(sizeof(FlipWorldApp));
    global_app = app;

    Gui *gui = furi_record_open(RECORD_GUI);

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app))
    {
        return NULL;
    }

    // Submenu
    if (!easy_flipper_set_submenu(&app->submenu, FlipWorldViewSubmenu, VERSION_TAG, callback_exit_app, &app->view_dispatcher))
    {
        return NULL;
    }

    submenu_add_item(app->submenu, "Play", FlipWorldSubmenuIndexGameSubmenu, callback_submenu_choices, app);
    submenu_add_item(app->submenu, "About", FlipWorldSubmenuIndexMessage, callback_submenu_choices, app);
    submenu_add_item(app->submenu, "Settings", FlipWorldSubmenuIndexSettings, callback_submenu_choices, app);

    // Switch to the main view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);

    return app;
}

// Function to free the resources used by FlipWorldApp
void flip_world_app_free(FlipWorldApp *app)
{
    furi_check(app, "FlipWorldApp is NULL");

    // Free Submenu(s)
    if (app->submenu)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewSubmenu);
        submenu_free(app->submenu);
    }

    free_all_views(app, true, true, true);

    // free the view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    // free the app
    if (app)
        free(app);
}