#include "callback/free.h"
#include <callback/loader.h>

void free_all(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    free_views(app);
    free_widgets(app);
    free_submenus(app);
    free_text_inputs(app);
    free_playlist();
    free_text_box(app);

    if (back_from_ap)
    {
        // cannot do this in callback_submenu_ap (we got a NULL pointer error)
        // so let's do it here
        if (app->fhttp != NULL)
        {
            app->fhttp->state = IDLE;
            flipper_http_send_data(app->fhttp, "[WIFI/AP/STOP]");
            furi_delay_ms(100);
            flipper_http_free(app->fhttp);
            app->fhttp = NULL;
        }
        back_from_ap = false;
    }

    if (app->fhttp)
    {
        flipper_http_free(app->fhttp);
        app->fhttp = NULL;
    }

    if (app->timer)
    {
        if (furi_timer_is_running(app->timer) > 0)
            furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }

    loader_view_free(app);
}

void free_playlist(void)
{
    if (wifi_playlist)
    {
        free(wifi_playlist);
        wifi_playlist = NULL;
    }
}

void free_submenus(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (app->submenu_wifi)
    {
        free(app->submenu_wifi);
        app->submenu_wifi = NULL;
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewSubmenu);
    }
}

void free_text_box(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app, "FlipWiFiApp is NULL");

    // Free the text box
    if (app->textbox)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewWiFiAP);
        text_box_free(app->textbox);
        app->textbox = NULL;
    }
    // Free the timer
    if (app->timer)
    {
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
}

void free_text_inputs(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (app->uart_text_input)
    {
        free(app->uart_text_input);
        app->uart_text_input = NULL;
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewTextInput);
    }
    if (app->uart_text_input_buffer)
    {
        free(app->uart_text_input_buffer);
        app->uart_text_input_buffer = NULL;
    }
    if (app->uart_text_input_temp_buffer)
    {
        free(app->uart_text_input_temp_buffer);
        app->uart_text_input_temp_buffer = NULL;
    }
}

void free_widgets(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (app->widget_info)
    {
        free(app->widget_info);
        app->widget_info = NULL;
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewAbout);
    }
};

void free_views(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (app->view_wifi)
    {
        free(app->view_wifi);
        app->view_wifi = NULL;
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewGeneric);
    }
}
