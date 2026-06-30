#include <web_crawler.h>
#include <callback/web_crawler_callback.h>

/**
 * @brief      Function to free the resources used by WebCrawlerApp.
 * @param      app  The WebCrawlerApp object to free.
 */
void web_crawler_app_free(WebCrawlerApp *app)
{
    furi_check(app, "web_crawler_app_free: App is NULL");

    // Free View(s)
    if (app->view_loader)
    {
        view_dispatcher_remove_view(app->view_dispatcher, WebCrawlerViewLoader);
        web_crawler_loader_free_model(app->view_loader);
        view_free(app->view_loader);
    }

    // Remove and free Submenu
    if (app->submenu_main)
    {
        view_dispatcher_remove_view(app->view_dispatcher, WebCrawlerViewSubmenuMain);
        submenu_free(app->submenu_main);
    }

    // Remove and free Widgets
    if (app->widget_result)
    {
        view_dispatcher_remove_view(app->view_dispatcher, WebCrawlerViewWidgetResult);
        widget_free(app->widget_result);
    }

    // check and free http method
    if (app->temp_buffer_http_method)
    {
        free(app->temp_buffer_http_method);
        app->temp_buffer_http_method = NULL;
    }
    if (app->http_method)
    {
        free(app->http_method);
        app->http_method = NULL;
    }

    free_all(app);
    furi_record_close(RECORD_STORAGE);
    view_dispatcher_free(app->view_dispatcher);
    free(app);
}
char *http_method_names[] = {"GET", "POST", "PUT", "DELETE", "DOWNLOAD", "BROWSE"};