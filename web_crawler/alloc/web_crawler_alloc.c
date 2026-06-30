#include <alloc/web_crawler_alloc.h>

/**
 * @brief      Function to allocate resources for the WebCrawlerApp.
 * @return     Pointer to the initialized WebCrawlerApp, or NULL on failure.
 */
WebCrawlerApp *web_crawler_app_alloc()
{
    // Initialize the entire structure to zero to prevent undefined behavior
    WebCrawlerApp *app = (WebCrawlerApp *)malloc(sizeof(WebCrawlerApp));

    // Open GUI
    Gui *gui = furi_record_open(RECORD_GUI);

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app))
    {
        return NULL;
    }
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, web_crawler_custom_event_callback);

    // Allocate and initialize temp_buffer and path
    app->temp_buffer_size_http_method = 16;
    if (!easy_flipper_set_buffer(&app->temp_buffer_http_method, app->temp_buffer_size_http_method))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->http_method, app->temp_buffer_size_http_method))
    {
        return NULL;
    }

    // Allocate Submenu views
    if (!easy_flipper_set_submenu(&app->submenu_main, WebCrawlerViewSubmenuMain, VERSION_TAG, web_crawler_exit_app_callback, &app->view_dispatcher))
    {
        return NULL;
    }

    // Add Submenu items
    submenu_add_item(app->submenu_main, "Run", WebCrawlerSubmenuIndexRun, web_crawler_submenu_callback, app);
    submenu_add_item(app->submenu_main, "About", WebCrawlerSubmenuIndexAbout, web_crawler_submenu_callback, app);
    submenu_add_item(app->submenu_main, "Settings", WebCrawlerSubmenuIndexConfig, web_crawler_submenu_callback, app);

    // Main view
    if (!easy_flipper_set_view(&app->view_loader, WebCrawlerViewLoader, web_crawler_loader_draw_callback, NULL, web_crawler_back_to_main_callback, &app->view_dispatcher, app))
    {
        return NULL;
    }
    web_crawler_loader_init(app->view_loader);
    if (!easy_flipper_set_widget(&app->widget_result, WebCrawlerViewWidgetResult, "Error, try again.", web_crawler_back_to_main_callback, &app->view_dispatcher))
    {
        return NULL;
    }

    // Start with the Submenu view
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewSubmenuMain);

    return app;
}
