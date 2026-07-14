#include "flip_weather.h"

char lat_data[32];
char lon_data[32];

char* total_data = NULL;
char* weather_data = NULL;
bool use_fahrenheit = false;
char custom_location[64] = "";

FlipWeatherApp* app_instance = NULL;
void flip_weather_loader_free_model(View* view);

// Function to free the resources used by FlipWeatherApp
void flip_weather_app_free(FlipWeatherApp* app) {
    if(!app) {
        FURI_LOG_E(TAG, "FlipWeatherApp is NULL");
        return;
    }

    // Free View(s)
    if(app->view_loader) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWeatherViewLoader);
        flip_weather_loader_free_model(app->view_loader);
        view_free(app->view_loader);
    }

    // Free Submenu(s)
    if(app->submenu) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWeatherViewSubmenu);
        submenu_free(app->submenu);
    }

    // Free Widget(s)
    if(app->widget) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWeatherViewAbout);
        widget_free(app->widget);
    }
    if(app->widget_result) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWeatherViewWidgetResult);
        widget_free(app->widget_result);
    }

    // Free Variable Item List(s)
    if(app->variable_item_list) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWeatherViewSettings);
        variable_item_list_free(app->variable_item_list);
    }

    // Free Text Input(s)
    if(app->uart_text_input_ssid) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWeatherViewTextInputSSID);
        text_input_free(app->uart_text_input_ssid);
    }
    if(app->uart_text_input_password) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWeatherViewTextInputPassword);
        text_input_free(app->uart_text_input_password);
    }
    if(app->uart_text_input_location) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWeatherViewTextInputLocation);
        text_input_free(app->uart_text_input_location);
    }

    // Free the text input buffer
    if(app->uart_text_input_buffer_ssid) free(app->uart_text_input_buffer_ssid);
    if(app->uart_text_input_temp_buffer_ssid) free(app->uart_text_input_temp_buffer_ssid);
    if(app->uart_text_input_buffer_password) free(app->uart_text_input_buffer_password);
    if(app->uart_text_input_temp_buffer_password) free(app->uart_text_input_temp_buffer_password);
    if(app->uart_text_input_buffer_location) free(app->uart_text_input_buffer_location);
    if(app->uart_text_input_temp_buffer_location) free(app->uart_text_input_temp_buffer_location);

    // deinitalize flipper http
    flipper_http_deinit();

    // free the view dispatcher
    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    if(total_data) free(total_data);

    // free the app
    if(app) free(app);
}
