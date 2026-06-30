#ifndef FLIP_WEATHER_E_H
#define FLIP_WEATHER_E_H

#include <flipper_http/flipper_http.h>
#include <easy_flipper/easy_flipper.h>
#include <jsmn/jsmn.h>

#define TAG "FlipWeather"

// Define the submenu items for our FlipWeather application
typedef enum {
    FlipWeatherSubmenuIndexWeather, // Click to view the weather
    FlipWeatherSubmenuIndexGPS, // Click to view the GPS
    FlipWeatherSubmenuIndexAbout, // Click to view the about screen
    FlipWeatherSubmenuIndexSettings, // Click to view the settings screen
} FlipWeatherSubmenuIndex;

// Define a single view for our FlipWeather application
typedef enum {
    FlipWeatherViewSubmenu, // The main submenu
    FlipWeatherViewAbout, // The about screen
    FlipWeatherViewSettings, // The wifi settings screen
    FlipWeatherViewTextInputSSID, // The text input screen for SSID
    FlipWeatherViewTextInputPassword, // The text input screen for password
    FlipWeatherViewTextInputLocation, // The text input screen for custom location
    //
    FlipWeatherViewPopupError, // The error popup screen
    FlipWeatherViewWidgetResult, // The text box that displays the random fact
    FlipWeatherViewLoader, // The loader screen retrieves data from the internet
} FlipWeatherView;

// Each screen will have its own view
typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    View* view_loader; // The screen that loads data from internet
    Submenu* submenu; // The main submenu
    Widget* widget; // The widget (about)
    Widget* widget_result; // The widget that displays the result
    Popup* popup_error; // The error popup
    VariableItemList* variable_item_list; // The variable item list (settngs)
    VariableItem* variable_item_ssid; // The variable item
    VariableItem* variable_item_password; // The variable item
    VariableItem* variable_item_location; // The variable item for custom location
    VariableItem* variable_item_temperature_unit; // The variable item for temperature unit
    TextInput* uart_text_input_ssid; // The text input
    TextInput* uart_text_input_password; // The text input
    TextInput* uart_text_input_location; // The text input for custom location

    char* uart_text_input_buffer_ssid; // Buffer for the text input
    char* uart_text_input_temp_buffer_ssid; // Temporary buffer for the text input
    uint32_t uart_text_input_buffer_size_ssid; // Size of the text input buffer

    char* uart_text_input_buffer_password; // Buffer for the text input
    char* uart_text_input_temp_buffer_password; // Temporary buffer for the text input
    uint32_t uart_text_input_buffer_size_password; // Size of the text input buffer

    char* uart_text_input_buffer_location; // Buffer for the custom location text input
    char* uart_text_input_temp_buffer_location; // Temporary buffer for the custom location text input
    uint32_t uart_text_input_buffer_size_location; // Size of the custom location text input buffer
} FlipWeatherApp;

extern char lat_data[32];
extern char lon_data[32];

extern char* total_data;
extern char* weather_data;
extern bool use_fahrenheit;
extern char custom_location[64];

// Function to free the resources used by FlipWeatherApp
void flip_weather_app_free(FlipWeatherApp* app);
extern FlipWeatherApp* app_instance;

#endif
