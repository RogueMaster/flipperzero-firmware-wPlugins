#include "flip_library.h"

bool use_fahrenheit = false;

FlipLibraryApp *app_instance = NULL;

void flip_library_loader_free_model(View *view);

// Function to free the resources used by FlipLibraryApp
void flip_library_app_free(FlipLibraryApp *app)
{
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipLibraryApp is NULL");
        return;
    }

    // Free View(s)
    if (app->view_loader)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewLoader);
        flip_library_loader_free_model(app->view_loader);
        view_free(app->view_loader);
    }

    // Free Submenu(s)
    if (app->submenu_main)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewSubmenuMain);
        submenu_free(app->submenu_main);
    }
    if (app->submenu_library)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewSubmenuLibrary);
        submenu_free(app->submenu_library);
    }
    if (app->submenu_random_facts)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewRandomFacts);
        submenu_free(app->submenu_random_facts);
    }
    if (app->submenu_predict)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewPredict);
        submenu_free(app->submenu_predict);
    }

    // Free Widget(s)
    if (app->widget_about)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewAbout);
        widget_free(app->widget_about);
    }
    if (app->widget_result)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewWidgetResult);
        widget_free(app->widget_result);
    }

    // Free Variable Item List(s)
    if (app->variable_item_list_wifi)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewSettings);
        variable_item_list_free(app->variable_item_list_wifi);
    }

    // Free Text Input(s)
    if (app->uart_text_input_ssid)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewTextInputSSID);
        text_input_free(app->uart_text_input_ssid);
    }
    if (app->uart_text_input_password)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewTextInputPassword);
        text_input_free(app->uart_text_input_password);
    }
    if (app->uart_text_input_query)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipLibraryViewTextInputQuery);
        text_input_free(app->uart_text_input_query);
    }

    // deinitalize flipper http
    flipper_http_deinit();

    // free the view dispatcher
    if (app->view_dispatcher)
    {
        view_dispatcher_free(app->view_dispatcher);
    }

    // close the gui
    furi_record_close(RECORD_GUI);

    if (app_instance)
    {
        // free the app instance
        free(app_instance);
        app_instance = NULL;
    }
    // free the app
    free(app);
}
