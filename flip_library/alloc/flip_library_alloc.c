#include "alloc/flip_library_alloc.h"

// Function to allocate resources for the FlipLibraryApp
FlipLibraryApp *flip_library_app_alloc()
{
    FlipLibraryApp *app = (FlipLibraryApp *)malloc(sizeof(FlipLibraryApp));

    Gui *gui = furi_record_open(RECORD_GUI);

    if (!flipper_http_init(flipper_http_rx_callback, app))
    {
        FURI_LOG_E(TAG, "Failed to initialize flipper http");
        return NULL;
    }

    // Allocate the text input buffer
    app->uart_text_input_buffer_size_ssid = 64;
    app->uart_text_input_buffer_size_password = 64;
    app->uart_text_input_buffer_size_query = 64;
    if (!easy_flipper_set_buffer(&app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_size_ssid))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->uart_text_input_temp_buffer_ssid, app->uart_text_input_buffer_size_ssid))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->uart_text_input_buffer_password, app->uart_text_input_buffer_size_password))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->uart_text_input_temp_buffer_password, app->uart_text_input_buffer_size_password))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->uart_text_input_buffer_query, app->uart_text_input_buffer_size_query))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->uart_text_input_temp_buffer_query, app->uart_text_input_buffer_size_query))
    {
        return NULL;
    }

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app))
    {
        return NULL;
    }
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, flip_library_custom_event_callback);

    // Main view
    if (!easy_flipper_set_view(&app->view_loader, FlipLibraryViewLoader, flip_library_loader_draw_callback, NULL, callback_to_random_facts, &app->view_dispatcher, app))
    {
        return NULL;
    }
    flip_library_loader_init(app->view_loader);

    // Widget
    if (!easy_flipper_set_widget(&app->widget_about, FlipLibraryViewAbout, "FlipLibrary v1.5\n-----\nUtilize WiFi to retrieve data\nfrom 20 different APIs.\n-----\nCreated by JBlanked and\nDerek Jamison.\n-----\nwww.github.com/jblanked/\nFlipLibrary\n-----\nPress BACK to return.", callback_to_submenu, &app->view_dispatcher))
    {
        return NULL;
    }
    if (!easy_flipper_set_widget(&app->widget_result, FlipLibraryViewWidgetResult, "Error, try again.", callback_to_random_facts, &app->view_dispatcher))
    {
        return NULL;
    }

    // Text Input
    if (!easy_flipper_set_uart_text_input(&app->uart_text_input_ssid, FlipLibraryViewTextInputSSID, "Enter SSID", app->uart_text_input_temp_buffer_ssid, app->uart_text_input_buffer_size_ssid, text_updated_ssid, callback_to_wifi_settings, &app->view_dispatcher, app))
    {
        return NULL;
    }
    if (!easy_flipper_set_uart_text_input(&app->uart_text_input_password, FlipLibraryViewTextInputPassword, "Enter Password", app->uart_text_input_temp_buffer_password, app->uart_text_input_buffer_size_password, text_updated_password, callback_to_wifi_settings, &app->view_dispatcher, app))
    {
        return NULL;
    }
    if (!easy_flipper_set_uart_text_input(&app->uart_text_input_query, FlipLibraryViewTextInputQuery, "Enter Query", app->uart_text_input_temp_buffer_query, app->uart_text_input_buffer_size_query, text_updated_query, callback_to_submenu_library, &app->view_dispatcher, app))
    {
        return NULL;
    }

    // Variable Item List
    if (!easy_flipper_set_variable_item_list(&app->variable_item_list_wifi, FlipLibraryViewSettings, settings_item_selected, callback_to_submenu, &app->view_dispatcher, app))
    {
        return NULL;
    }

    app->variable_item_ssid = variable_item_list_add(app->variable_item_list_wifi, "SSID", 0, NULL, NULL);
    app->variable_item_password = variable_item_list_add(app->variable_item_list_wifi, "Password", 0, NULL, NULL);
    app->variable_item_temperature_unit = variable_item_list_add(app->variable_item_list_wifi, "Weather Unit", 2, temperature_unit_change, app);
    variable_item_set_current_value_text(app->variable_item_ssid, "");
    variable_item_set_current_value_text(app->variable_item_password, "");
    variable_item_set_current_value_index(app->variable_item_temperature_unit, 0);
    variable_item_set_current_value_text(app->variable_item_temperature_unit, "Celsius");

    // Submenu
    if (!easy_flipper_set_submenu(&app->submenu_main, FlipLibraryViewSubmenuMain, "FlipLibrary v1.5", callback_exit_app, &app->view_dispatcher))
    {
        return NULL;
    }
    if (!easy_flipper_set_submenu(&app->submenu_library, FlipLibraryViewSubmenuLibrary, "Library", callback_to_submenu, &app->view_dispatcher))
    {
        return NULL;
    }
    if (!easy_flipper_set_submenu(&app->submenu_random_facts, FlipLibraryViewRandomFacts, "Random", callback_to_submenu_library, &app->view_dispatcher))
    {
        return NULL;
    }
    if (!easy_flipper_set_submenu(&app->submenu_predict, FlipLibraryViewPredict, "Predict", callback_to_submenu_library, &app->view_dispatcher))
    {
        return NULL;
    }

    submenu_add_item(app->submenu_main, "Library", FlipLibrarySubmenuIndexLibrary, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "About", FlipLibrarySubmenuIndexAbout, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "Settings", FlipLibrarySubmenuIndexSettings, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "Wikipedia", FlipLibrarySubmenuIndexWiki, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "Dictionary", FlipLibrarySubmenuIndexDictionary, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "Predict", FlipLibrarySubmenuIndexPredict, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "Random", FlipLibrarySubmenuIndexRandomFacts, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "Weather", FlipLibrarySubmenuIndexWeather, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "GPS", FlipLibrarySubmenuIndexGPS, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "Elevation", FlipLibrarySubmenuIndexElevation, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "Asset Price", FlipLibrarySubmenuIndexAssetPrice, callback_submenu_choices, app);
    submenu_add_item(app->submenu_library, "Next Holiday", FlipLibrarySubmenuIndexNextHoliday, callback_submenu_choices, app);
    //
    submenu_add_item(app->submenu_random_facts, "Trivia", FlipLibrarySubmenuIndexRandomTrivia, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "Advice", FlipLibrarySubmenuIndexRandomAdvice, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "Quote", FlipLibrarySubmenuIndexRandomFactsQuotes, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "Fact", FlipLibrarySubmenuIndexRandomFactsAll, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "Cat Fact", FlipLibrarySubmenuIndexRandomFactsCats, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "Dog Fact", FlipLibrarySubmenuIndexRandomFactsDogs, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "Tech Phrase", FlipLibrarySubmenuIndexRandomTechPhrase, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "UUID", FlipLibrarySubmenuIndexRandomUUID, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "Address", FlipLibrarySubmenuIndexRandomAddress, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "Credit Card", FlipLibrarySubmenuIndexRandomCreditCard, callback_submenu_choices, app);
    submenu_add_item(app->submenu_random_facts, "User Info", FlipLibrarySubmenuIndexRandomUserInfo, callback_submenu_choices, app);
    //
    submenu_add_item(app->submenu_predict, "Age", FlipLibrarySubmenuIndexPredictAge, callback_submenu_choices, app);
    submenu_add_item(app->submenu_predict, "Ethnicity", FlipLibrarySubmenuIndexPredictEthnicity, callback_submenu_choices, app);
    submenu_add_item(app->submenu_predict, "Gender", FlipLibrarySubmenuIndexPredictGender, callback_submenu_choices, app);

    // load settings
    if (load_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_size_ssid, app->uart_text_input_buffer_password, app->uart_text_input_buffer_size_password, &use_fahrenheit))
    {
        // Update variable items
        if (app->variable_item_ssid)
        {
            variable_item_set_current_value_text(app->variable_item_ssid, app->uart_text_input_buffer_ssid);
        }
        // dont show password

        // Copy items into their temp buffers with safety checks
        if (app->uart_text_input_buffer_ssid && app->uart_text_input_temp_buffer_ssid)
        {
            strncpy(app->uart_text_input_temp_buffer_ssid, app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_size_ssid - 1);
            app->uart_text_input_temp_buffer_ssid[app->uart_text_input_buffer_size_ssid - 1] = '\0';
        }
        if (app->uart_text_input_buffer_password && app->uart_text_input_temp_buffer_password)
        {
            strncpy(app->uart_text_input_temp_buffer_password, app->uart_text_input_buffer_password, app->uart_text_input_buffer_size_password - 1);
            app->uart_text_input_temp_buffer_password[app->uart_text_input_buffer_size_password - 1] = '\0';
        }
        // Apply loaded temperature unit
        if (app->variable_item_temperature_unit)
        {
            variable_item_set_current_value_index(app->variable_item_temperature_unit, use_fahrenheit ? 1 : 0);
            variable_item_set_current_value_text(app->variable_item_temperature_unit, use_fahrenheit ? "Fahrenheit" : "Celsius");
        }
    }

    // assign app instance
    app_instance = app;

    // start with the main view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewSubmenuMain);

    return app;
}
