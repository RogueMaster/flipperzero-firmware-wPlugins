#include <callback/flip_library_callback.h>
#include "stdlib.h"

// FURI_LOG_DEV will log only during app development. Be sure that Settings/System/Log Device is "LPUART"; so we dont use serial port.
#ifdef DEVELOPMENT
#define FURI_LOG_DEV(tag, format, ...) furi_log_print_format(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define DEV_CRASH() furi_crash()
#else
#define FURI_LOG_DEV(tag, format, ...)
#define DEV_CRASH()
#endif

static bool flip_library_wiki_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    snprintf(
        fhttp.file_path,
        sizeof(fhttp.file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_library/wiki.json");

    // encode spaces for url
    for (size_t i = 0; i < strlen(app_instance->uart_text_input_buffer_query); i++)
    {
        if (app_instance->uart_text_input_buffer_query[i] == ' ')
        {
            app_instance->uart_text_input_buffer_query[i] = '_';
        }
    }

    char url[260];
    snprintf(url, sizeof(url), "https://api.wikimedia.org/core/v1/wikipedia/en/search/title?q=%s&limit=1", app_instance->uart_text_input_buffer_query);
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove_recursive(storage, fhttp.file_path);
    fhttp.save_received_data = true;

    return flipper_http_get_request_with_headers(url, "{\"Content-Type\":\"application/json\"}");
}
static char *flip_library_wiki_parse(FactLoaderModel *model)
{
    UNUSED(model);
    FuriString *data = flipper_http_load_from_file(fhttp.file_path);
    if (data == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load received data from file.");
        return "Failed to load received data from file.";
    }
    char *data_cstr = (char *)furi_string_get_cstr(data);
    if (data_cstr == NULL)
    {
        FURI_LOG_E(TAG, "Failed to get C-string from FuriString.");
        furi_string_free(data);
        return "Failed to get C-string from FuriString.";
    }
    char *pages = get_json_array_value("pages", 0, data_cstr, MAX_TOKENS);
    if (pages == NULL)
    {
        furi_string_free(data);
        return data_cstr;
    }
    char *description = get_json_value("description", pages, 64);
    if (description == NULL)
    {
        furi_string_free(data);
        return data_cstr;
    }
    return description;
}
static void flip_library_wiki_switch_to_view(FlipLibraryApp *app)
{
    text_input_set_header_text(app->uart_text_input_query, "Search Wikipedia");
    flip_library_generic_switch_to_view(app, "Searching..", flip_library_wiki_fetch, flip_library_wiki_parse, 1, callback_to_submenu_library, FlipLibraryViewTextInputQuery);
}
//
static bool flip_library_random_fact_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request("https://uselessfacts.jsph.pl/api/v2/facts/random");
}
static char *flip_library_random_fact_parse(FactLoaderModel *model)
{
    UNUSED(model);
    return get_json_value("text", fhttp.last_response, 128);
}
static void flip_library_random_fact_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Fact", flip_library_random_fact_fetch, flip_library_random_fact_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_cat_fact_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request_with_headers("https://catfact.ninja/fact", "{\"Content-Type\":\"application/json\"}");
}
static char *flip_library_cat_fact_parse(FactLoaderModel *model)
{
    UNUSED(model);
    return get_json_value("fact", fhttp.last_response, 128);
}
static void flip_library_cat_fact_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Cat Fact", flip_library_cat_fact_fetch, flip_library_cat_fact_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_dog_fact_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request_with_headers("https://dog-api.kinduff.com/api/facts", "{\"Content-Type\":\"application/json\"}");
}
static char *flip_library_dog_fact_parse(FactLoaderModel *model)
{
    UNUSED(model);
    return get_json_array_value("facts", 0, fhttp.last_response, 256);
}
static void flip_library_dog_fact_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Dog Fact", flip_library_dog_fact_fetch, flip_library_dog_fact_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_quote_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request("https://zenquotes.io/api/random");
}
static char *flip_library_quote_parse(FactLoaderModel *model)
{
    UNUSED(model);
    // remove [ and ] from the start and end of the string
    char *response = fhttp.last_response;
    if (response[0] == '[')
    {
        response++;
    }
    if (response[strlen(response) - 1] == ']')
    {
        response[strlen(response) - 1] = '\0';
    }
    // remove white space from both sides
    while (response[0] == ' ')
    {
        response++;
    }
    while (response[strlen(response) - 1] == ' ')
    {
        response[strlen(response) - 1] = '\0';
    }
    return get_json_value("q", response, 128);
}
static void flip_library_quote_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Quote", flip_library_quote_fetch, flip_library_quote_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_dictionary_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"word\":\"%s\"}", app_instance->uart_text_input_buffer_query);

    return flipper_http_post_request_with_headers("https://www.flipsocial.net/api/define/", "{\"Content-Type\":\"application/json\"}", payload);
}
static char *flip_library_dictionary_parse(FactLoaderModel *model)
{
    UNUSED(model);
    char *defn = get_json_value("definition", fhttp.last_response, 16);
    if (defn == NULL)
    {
        defn = get_json_value("[ERROR]", fhttp.last_response, 16);
    }
    return defn;
}
static void flip_library_dictionary_switch_to_view(FlipLibraryApp *app)
{
    text_input_set_header_text(app->uart_text_input_query, "Enter a word");
    flip_library_generic_switch_to_view(app, "Defining", flip_library_dictionary_fetch, flip_library_dictionary_parse, 1, callback_to_submenu_library, FlipLibraryViewTextInputQuery);
}
// GPS
static bool flip_library_gps_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.state == INACTIVE)
    {
        FURI_LOG_E(TAG, "Board is INACTIVE");
        flipper_http_ping(); // ping the device
        fhttp.state = ISSUE;
        return false;
    }
    if (!flipper_http_get_request_with_headers("https://ipwhois.app/json/", "{\"Content-Type\": \"application/json\"}"))
    {
        FURI_LOG_E(TAG, "Failed to send GET request");
        fhttp.state = ISSUE;
        return false;
    }
    fhttp.state = RECEIVING;
    return true;
}
static char *flip_library_gps_parse(FactLoaderModel *model)
{
    UNUSED(model);
    char *total_data = NULL;
    if (fhttp.last_response != NULL)
    {
        char *city = get_json_value("city", fhttp.last_response, MAX_TOKENS);
        char *region = get_json_value("region", fhttp.last_response, MAX_TOKENS);
        char *country = get_json_value("country", fhttp.last_response, MAX_TOKENS);
        char *latitude = get_json_value("latitude", fhttp.last_response, MAX_TOKENS);
        char *longitude = get_json_value("longitude", fhttp.last_response, MAX_TOKENS);

        if (city == NULL || region == NULL || country == NULL || latitude == NULL || longitude == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get geo location data");
            fhttp.state = ISSUE;
            return NULL;
        }

        if (!total_data)
        {
            total_data = (char *)malloc(512);
            if (!total_data)
            {
                FURI_LOG_E(TAG, "Failed to allocate memory for total_data");
                fhttp.state = ISSUE;
                return NULL;
            }
        }
        snprintf(total_data, 512, "You are in %s, %s, %s. \nLatitude: %s, Longitude: %s", city, region, country, latitude, longitude);

        fhttp.state = IDLE;
        free(city);
        free(region);
        free(country);
        free(latitude);
        free(longitude);
    }
    return total_data;
}
static void flip_library_gps_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Fetching GPS data..", flip_library_gps_fetch, flip_library_gps_parse, 1, callback_to_submenu_library, FlipLibraryViewLoader);
}
// Weather
static const char *weather_code_to_str(const char *code)
{
    int wmo = atoi(code);
    switch (wmo)
    {
    case 0:  return "Clear sky";
    case 1:  return "Mainly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45: return "Fog";
    case 48: return "Rime fog";
    case 51: return "Light drizzle";
    case 53: return "Moderate drizzle";
    case 55: return "Dense drizzle";
    case 61: return "Slight rain";
    case 63: return "Moderate rain";
    case 65: return "Heavy rain";
    case 71: return "Slight snow";
    case 73: return "Moderate snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: return "Slight showers";
    case 81: return "Moderate showers";
    case 82: return "Violent showers";
    case 85: return "Slight snow showers";
    case 86: return "Heavy snow showers";
    case 95: return "Thunderstorm";
    case 96: return "Thunderstorm+hail";
    case 99: return "Thunderstorm+hail";
    default: return "Unknown";
    }
}
static const char *degrees_to_compass(const char *degrees)
{
    char* endptr;
    float deg = strtod(degrees, &endptr);
    if (deg < 22.5f || deg >= 337.5f) return "N";
    if (deg < 67.5f)  return "NE";
    if (deg < 112.5f) return "E";
    if (deg < 157.5f) return "SE";
    if (deg < 202.5f) return "S";
    if (deg < 247.5f) return "SW";
    if (deg < 292.5f) return "W";
    return "NW";
}
static bool flip_library_weather_fetch(FactLoaderModel *model)
{
    if (model->request_index == 0)
    {
        fhttp.save_received_data = true;
        snprintf(
            fhttp.file_path,
            sizeof(fhttp.file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_library/gps.json");
        return flip_library_gps_fetch(model); // fetch the Lat and Long
    }
    else if (model->request_index == 1)
    {
        FuriString *data = flipper_http_load_from_file(fhttp.file_path);
        if (data == NULL)
        {
            FURI_LOG_E(TAG, "Failed to load received data from file.");
            return false;
        }
        char *data_cstr = (char *)furi_string_get_cstr(data);
        if (data_cstr == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get C-string from FuriString.");
            furi_string_free(data);
            return false;
        }
        char url[512];
        char *latitude = get_json_value("latitude", data_cstr, MAX_TOKENS);
        char *longitude = get_json_value("longitude", data_cstr, MAX_TOKENS);
        if (latitude == NULL || longitude == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get geo location data");
            fhttp.state = ISSUE;
            return false;
        }
        snprintf(url, 512, "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,precipitation,rain,showers,snowfall,wind_speed_10m,wind_direction_10m,weather_code&temperature_unit=%s&wind_speed_unit=mph&precipitation_unit=inch&forecast_days=1", latitude, longitude, use_fahrenheit ? "fahrenheit" : "celsius");
        if (!flipper_http_get_request_with_headers(url, "{\"Content-Type\": \"application/json\"}"))
        {
            FURI_LOG_E(TAG, "Failed to send GET request");
            fhttp.state = ISSUE;
            return false;
        }
        fhttp.state = RECEIVING;
        return true;
    }
    return false;
}
static char *flip_library_weather_parse(FactLoaderModel *model)
{
    if (model->request_index == 0)
    {
        return flip_library_gps_parse(model); // show the location
    }
    else if (model->request_index == 1)
    {
        UNUSED(model);
        char *weather_data = NULL;
        if (fhttp.last_response != NULL)
        {
            char *current_data = get_json_value("current", fhttp.last_response, MAX_TOKENS);
            char *temperature = get_json_value("temperature_2m", current_data, MAX_TOKENS);
            char *precipitation = get_json_value("precipitation", current_data, MAX_TOKENS);
            char *rain = get_json_value("rain", current_data, MAX_TOKENS);
            char *showers = get_json_value("showers", current_data, MAX_TOKENS);
            char *snowfall = get_json_value("snowfall", current_data, MAX_TOKENS);
            char *wind_speed = get_json_value("wind_speed_10m", current_data, MAX_TOKENS);
            char *wind_direction = get_json_value("wind_direction_10m", current_data, MAX_TOKENS);
            char *weather_code = get_json_value("weather_code", current_data, MAX_TOKENS);
            char *time = get_json_value("time", current_data, MAX_TOKENS);

            if (current_data == NULL || temperature == NULL || precipitation == NULL || rain == NULL || showers == NULL || snowfall == NULL || wind_speed == NULL || wind_direction == NULL || weather_code == NULL || time == NULL)
            {
                FURI_LOG_E(TAG, "Failed to get weather data");
                fhttp.state = ISSUE;
                return NULL;
            }

            // replace the "T" in time with a space
            char *ptr = strstr(time, "T");
            if (ptr != NULL)
            {
                *ptr = ' ';
            }
        
            const char *condition = weather_code_to_str(weather_code);
            const char *compass = degrees_to_compass(wind_direction);

            if (!weather_data)
            {
                weather_data = (char *)malloc(512);
                if (!weather_data)
                {
                    FURI_LOG_E(TAG, "Failed to allocate memory for weather_data");
                    fhttp.state = ISSUE;
                    return NULL;
                }
            }
            snprintf(weather_data, 512, "Condition: %s\nTemperature: %s %s\nWind: %s mph %s\nPrecipitation: %s\nRain: %s\nShowers: %s\nSnowfall: %s\nTime: %s", condition, temperature, use_fahrenheit ? "F" : "C", wind_speed, compass, precipitation, rain, showers, snowfall, time);

            fhttp.state = IDLE;
            free(current_data);
            free(temperature);
            free(precipitation);
            free(rain);
            free(showers);
            free(snowfall);
            free(wind_speed);
            free(wind_direction);
            free(weather_code);
            free(time);
        }
        return weather_data;
    }
    return NULL;
}
static void flip_library_weather_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Fetching Weather data..", flip_library_weather_fetch, flip_library_weather_parse, 2, callback_to_submenu_library, FlipLibraryViewLoader);
}

void temperature_unit_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    use_fahrenheit = (index == 1);
    variable_item_set_current_value_text(item, use_fahrenheit ? "Fahrenheit" : "Celsius");
    save_settings(
        app_instance->uart_text_input_buffer_ssid,
        app_instance->uart_text_input_buffer_password,
        use_fahrenheit);
}

//
static bool flip_library_elevation_fetch(FactLoaderModel *model)
{
    if (model->request_index == 0)
    {
        fhttp.save_received_data = true;
        snprintf(
            fhttp.file_path,
            sizeof(fhttp.file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_library/gps.json");
        return flip_library_gps_fetch(model); // fetch the Lat and Long
    }
    else if (model->request_index == 1)
    {
        FuriString *data = flipper_http_load_from_file(fhttp.file_path);
        if (data == NULL)
        {
            FURI_LOG_E(TAG, "Failed to load received data from file.");
            return false;
        }
        char *data_cstr = (char *)furi_string_get_cstr(data);
        if (data_cstr == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get C-string from FuriString.");
            furi_string_free(data);
            return false;
        }
        char url[256];
        char *latitude = get_json_value("latitude", data_cstr, MAX_TOKENS);
        char *longitude = get_json_value("longitude", data_cstr, MAX_TOKENS);
        if (latitude == NULL || longitude == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get geo location data");
            fhttp.state = ISSUE;
            return false;
        }
        fhttp.save_received_data = true;
        snprintf(
            fhttp.file_path,
            sizeof(fhttp.file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_library/elevation.json");
        snprintf(url, sizeof(url), "https://api.opentopodata.org/v1/aster30m?locations=%s,%s", latitude, longitude);
        if (!flipper_http_get_request_with_headers(url, "{\"Content-Type\": \"application/json\"}"))
        {
            FURI_LOG_E(TAG, "Failed to send GET request");
            fhttp.state = ISSUE;
            return false;
        }
        fhttp.state = RECEIVING;
        return true;
    }
    return false;
}
static char *flip_library_elevation_parse(FactLoaderModel *model)
{
    if (model->request_index == 0)
    {
        return flip_library_gps_parse(model); // show the location
    }
    else if (model->request_index == 1)
    {
        UNUSED(model);
        FuriString *data = flipper_http_load_from_file(fhttp.file_path);
        if (data == NULL)
        {
            FURI_LOG_E(TAG, "Failed to load received data from file.");
            return NULL;
        }
        char *elevation_data = NULL;
        char *results = get_json_array_value("results", 0, (char *)furi_string_get_cstr(data), 64);
        char *elevation = get_json_value("elevation", results, 16);
        if (elevation == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get elevation data");
            fhttp.state = ISSUE;
            return NULL;
        }

        elevation_data = (char *)malloc(64);
        if (!elevation_data)
        {
            FURI_LOG_E(TAG, "Failed to allocate memory for elevation_data");
            fhttp.state = ISSUE;
            return NULL;
        }
        snprintf(elevation_data, 64, "Elevation: %s meters", elevation);

        fhttp.state = IDLE;
        free(elevation);

        return elevation_data;
    }
    return NULL;
}
static void flip_library_elevation_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Fetching Elevation data..", flip_library_elevation_fetch, flip_library_elevation_parse, 2, callback_to_submenu_library, FlipLibraryViewLoader);
}
//
static bool flip_library_asset_price_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    if (!app_instance->uart_text_input_buffer_query)
    {
        FURI_LOG_E(TAG, "Query is NULL");
        fhttp.state = ISSUE;
        return false;
    }
    char url[128];
    snprintf(
        fhttp.file_path,
        sizeof(fhttp.file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_library/price.txt");
    // capitalize the query
    for (size_t i = 0; i < strlen(app_instance->uart_text_input_buffer_query); i++)
    {
        app_instance->uart_text_input_buffer_query[i] = toupper(app_instance->uart_text_input_buffer_query[i]);
    }
    fhttp.save_received_data = true;
    snprintf(url, 128, "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=%s&apikey=2X90WLEFMP43OJKE", app_instance->uart_text_input_buffer_query);
    if (!flipper_http_get_request_with_headers(url, "{\"Content-Type\":\"application/json\"}"))
    {
        FURI_LOG_E(TAG, "Failed to send GET request");
        fhttp.state = ISSUE;
        return false;
    }
    fhttp.state = RECEIVING;
    return true;
}
static char *flip_library_asset_price_parse(FactLoaderModel *model)
{
    UNUSED(model);
    // load the received data from the saved file
    FuriString *price_data = flipper_http_load_from_file(fhttp.file_path);
    if (price_data == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load received data from file.");
        fhttp.state = ISSUE;
        return NULL;
    }
    char *data_cstr = (char *)furi_string_get_cstr(price_data);
    if (data_cstr == NULL)
    {
        FURI_LOG_E(TAG, "Failed to get C-string from FuriString.");
        furi_string_free(price_data);
        fhttp.state = ISSUE;
        return NULL;
    }
    char *global_quote = get_json_value("Global Quote", data_cstr, MAX_TOKENS);
    if (global_quote == NULL)
    {
        FURI_LOG_E(TAG, "Failed to get Global Quote");
        fhttp.state = ISSUE;
        furi_string_free(price_data);
        free(global_quote);
        free(data_cstr);
        return NULL;
    }
    char *price = get_json_value("05. price", global_quote, MAX_TOKENS);
    if (price == NULL)
    {
        FURI_LOG_E(TAG, "Failed to get price");
        fhttp.state = ISSUE;
        furi_string_free(price_data);
        free(global_quote);
        free(price);
        free(data_cstr);
        return NULL;
    }
    char *asset_price = (char *)malloc(64);
    if (!asset_price)
    {
        FURI_LOG_E(TAG, "Failed to allocate memory for asset_price");
        fhttp.state = ISSUE;
        furi_string_free(price_data);
        free(global_quote);
        free(price);
        free(data_cstr);
        return NULL;
    }
    snprintf(asset_price, 64, "%s: $%s", app_instance->uart_text_input_buffer_query, price);

    fhttp.state = IDLE;
    furi_string_free(price_data);
    free(global_quote);
    free(price);
    free(data_cstr);
    return asset_price;
}
static void flip_library_asset_price_switch_to_view(FlipLibraryApp *app)
{
    text_input_set_header_text(app->uart_text_input_query, "Enter an asset");
    flip_library_generic_switch_to_view(app, "Fetching Asset Price..", flip_library_asset_price_fetch, flip_library_asset_price_parse, 1, callback_to_submenu_library, FlipLibraryViewTextInputQuery);
}
//
static bool flip_library_next_holiday_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    char url[128];
    snprintf(
        fhttp.file_path,
        sizeof(fhttp.file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_library/holiday.json");
    fhttp.save_received_data = true;
    snprintf(url, sizeof(url), "https://date.nager.at/api/v3/NextPublicHolidays/%s", app_instance->uart_text_input_buffer_query);
    return flipper_http_get_request_with_headers(url, "{\"Content-Type\":\"application/json\"}");
}
static char *flip_library_next_holiday_parse(FactLoaderModel *model)
{
    UNUSED(model);
    FuriString *holiday_data = flipper_http_load_from_file(fhttp.file_path);
    if (holiday_data == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load received data from file.");
        return "Failed to load received data from file.";
    }
    furi_string_replace_at(holiday_data, 0, 0, "{\"holidays\": ");
    size_t term = furi_string_search_char(holiday_data, 0x0d, 0);
    if (term == FURI_STRING_FAILURE)
    {
        term = furi_string_size(holiday_data);
    }
    furi_string_replace_at(holiday_data, term, 0, "}");
    char *holiday = get_json_array_value("holidays", 0, (char *)furi_string_get_cstr(holiday_data), 512);
    if (holiday == NULL)
    {
        furi_string_free(holiday_data);
        return NULL;
    }
    char *name = get_json_value("name", holiday, 32);
    char *date = get_json_value("date", holiday, 32);
    char *result = (char *)malloc(strlen(name) + strlen(date) + 3);
    if (result == NULL)
    {
        furi_string_free(holiday_data);
        free(name);
        free(date);
        return NULL;
    }
    snprintf(result, strlen(name) + strlen(date) + 3, "%s: %s", date, name);
    furi_string_free(holiday_data);
    free(name);
    free(date);
    return result;
}
static void flip_library_next_holiday_switch_to_view(FlipLibraryApp *app)
{
    text_input_set_header_text(app->uart_text_input_query, "Enter a country code");
    flip_library_generic_switch_to_view(app, "Fetching Next Holiday..", flip_library_next_holiday_fetch, flip_library_next_holiday_parse, 1, callback_to_submenu_library, FlipLibraryViewTextInputQuery);
}
//
static bool flip_library_predict_age_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    char url[128];
    snprintf(url, sizeof(url), "https://api.agify.io/?name=%s", app_instance->uart_text_input_buffer_query);
    return flipper_http_get_request_with_headers(url, "{\"Content-Type\":\"application/json\"}");
}
static char *flip_library_predict_age_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.last_response == NULL)
    {
        return "Failed to get response";
    }
    char *age = get_json_value("age", fhttp.last_response, 16);
    if (age == NULL)
    {
        age = get_json_value("[ERROR]", fhttp.last_response, 16);
    }
    return age;
}
static void flip_library_predict_age_switch_to_view(FlipLibraryApp *app)
{
    text_input_set_header_text(app->uart_text_input_query, "Enter a name");
    flip_library_generic_switch_to_view(app, "Predicting Age..", flip_library_predict_age_fetch, flip_library_predict_age_parse, 1, callback_to_submenu_predict, FlipLibraryViewTextInputQuery);
}
//
static bool flip_library_predict_gender_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    char url[128];
    snprintf(url, sizeof(url), "https://api.genderize.io/?name=%s", app_instance->uart_text_input_buffer_query);
    return flipper_http_get_request_with_headers(url, "{\"Content-Type\":\"application/json\"}");
}
static char *flip_library_predict_gender_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.last_response == NULL)
    {
        return "Failed to get response";
    }
    char *gender = get_json_value("gender", fhttp.last_response, 16);
    if (gender == NULL)
    {
        gender = get_json_value("[ERROR]", fhttp.last_response, 16);
    }
    return gender;
}
static void flip_library_predict_gender_switch_to_view(FlipLibraryApp *app)
{
    text_input_set_header_text(app->uart_text_input_query, "Enter a name");
    flip_library_generic_switch_to_view(app, "Predicing Gender..", flip_library_predict_gender_fetch, flip_library_predict_gender_parse, 1, callback_to_submenu_predict, FlipLibraryViewTextInputQuery);
}
//
static bool flip_library_predict_ethnicity_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    char url[128];
    snprintf(url, sizeof(url), "https://api.nationalize.io/?name=%s", app_instance->uart_text_input_buffer_query);
    return flipper_http_get_request_with_headers(url, "{\"Content-Type\":\"application/json\"}");
}
static char *flip_library_predict_ethnicity_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.last_response == NULL)
    {
        return "Failed to get response";
    }
    char *country = get_json_array_value("country", 0, fhttp.last_response, 64);
    if (country == NULL)
    {
        return get_json_value("[ERROR]", fhttp.last_response, 16);
    }
    return get_json_value("country_id", country, 16);
}
static void flip_library_predict_ethnicity_switch_to_view(FlipLibraryApp *app)
{
    text_input_set_header_text(app->uart_text_input_query, "Enter a name");
    flip_library_generic_switch_to_view(app, "Predicting Ethnicity..", flip_library_predict_ethnicity_fetch, flip_library_predict_ethnicity_parse, 1, callback_to_submenu_predict, FlipLibraryViewTextInputQuery);
}
//
static bool flip_library_random_advice_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request("https://api.adviceslip.com/advice");
}
static char *flip_library_random_advice_parse(FactLoaderModel *model)
{
    UNUSED(model);
    char *slip = get_json_value("slip", fhttp.last_response, 32);
    if (!slip)
    {
        return "Failed to get response";
    }
    return get_json_value("advice", slip, 32);
}
static void flip_library_random_advice_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Advice", flip_library_random_advice_fetch, flip_library_random_advice_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_random_trivia_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request_with_headers("https://opentdb.com/api.php?amount=1", "{\"Content-Type\":\"application/json\"}");
}
static char *flip_library_random_trivia_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (!fhttp.last_response)
    {
        return "Failed to get response";
    }
    // replace the &quot; with "
    char *ptr = fhttp.last_response;
    while ((ptr = strstr(ptr, "&quot;")) != NULL)
    {
        ptr[0] = '"';
        ptr[1] = '"';
        ptr[2] = '"';
        ptr[3] = '"';
    }
    // replace the &#039; with '
    ptr = fhttp.last_response;
    while ((ptr = strstr(ptr, "&#039;")) != NULL)
    {
        ptr[0] = '\'';
        ptr[1] = '\0';
    }
    // replace the &amp; with &
    ptr = fhttp.last_response;
    while ((ptr = strstr(ptr, "&amp;")) != NULL)
    {
        ptr[0] = '&';
        ptr[1] = '\0';
    }
    char *results = get_json_array_value("results", 0, fhttp.last_response, 256);
    if (!results)
    {
        FURI_LOG_E(TAG, "Failed to get response");
        FURI_LOG_E(TAG, fhttp.last_response);
        return "Failed to get response";
    }
    char *question = get_json_value("question", results, 32);
    char *correct_answer = get_json_value("correct_answer", results, 32);
    if (!question || !correct_answer)
    {
        return "Failed to get response";
    }
    char *result = (char *)malloc(strlen(question) + strlen(correct_answer) + 128);
    if (!result)
    {
        return "Failed to allocate memory";
    }
    snprintf(result, strlen(question) + strlen(correct_answer) + 128, "Q: %s\n\n(Scroll down for the answer)\n\nA: %s", question, correct_answer);
    return result;
}
static void flip_library_random_trivia_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Trivia", flip_library_random_trivia_fetch, flip_library_random_trivia_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_random_tech_phrase_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request("https://techy-api.vercel.app/api/json");
}
static char *flip_library_random_tech_phrase_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (!fhttp.last_response)
    {
        return "Failed to get response";
    }
    char *message = get_json_value("message", fhttp.last_response, 32);
    if (!message)
    {
        return "Failed to get response";
    }
    return message;
}
static void flip_library_random_tech_phrase_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Tech Phrase", flip_library_random_tech_phrase_fetch, flip_library_random_tech_phrase_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_random_uuid_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request("https://www.uuidtools.com/api/generate/v4");
}
static char *flip_library_random_uuid_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (!fhttp.last_response)
    {
        return "Failed to get response";
    }
    // remove [ and ] from the start and end of the string and " from the start and end of the uuid
    char *response = fhttp.last_response;
    if (response[0] == '[')
    {
        response++;
    }
    if (response[strlen(response) - 1] == ']')
    {
        response[strlen(response) - 1] = '\0';
    }
    if (response[0] == '"')
    {
        response++;
    }
    if (response[strlen(response) - 1] == '"')
    {
        response[strlen(response) - 1] = '\0';
    }
    return response;
}
static void flip_library_random_uuid_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random UUID", flip_library_random_uuid_fetch, flip_library_random_uuid_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_random_address_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request("https://fakerapi.it/api/v2/addresses?_quantity=1");
}
static char *flip_library_random_address_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.last_response == NULL)
    {
        return "Failed to get response";
    }
    char *data = get_json_array_value("data", 0, fhttp.last_response, 128);
    if (!data)
    {
        return "Failed to get response";
    }
    char *street = get_json_value("street", data, 32);
    char *city = get_json_value("city", data, 32);
    char *country = get_json_value("country", data, 32);
    char *zip = get_json_value("zipcode", data, 32);
    if (!street || !city || !country || !zip)
    {
        return "Failed to get response";
    }
    char *result = (char *)malloc(strlen(street) + strlen(city) + strlen(country) + strlen(zip) + 128);
    if (!result)
    {
        return "Failed to allocate memory";
    }
    snprintf(result, strlen(street) + strlen(city) + strlen(country) + strlen(zip) + 128, "Street: %s\nCity: %s\nCountry: %s\nZip: %s", street, city, country, zip);
    return result;
}
static void flip_library_random_address_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Address", flip_library_random_address_fetch, flip_library_random_address_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_random_credit_card_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request("https://fakerapi.it/api/v2/creditCards?_quantity=1");
}
static char *flip_library_random_credit_card_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (!fhttp.last_response)
    {
        return "Failed to get response";
    }
    char *data = get_json_array_value("data", 0, fhttp.last_response, 128);
    if (!data)
    {
        return "Failed to get response";
    }
    char *card_number = get_json_value("number", data, 32);
    char *card_name = get_json_value("owner", data, 32);
    char *card_expiration = get_json_value("expiration", data, 32);
    char *card_type = get_json_value("type", data, 32);
    if (!card_number || !card_name || !card_expiration || !card_type)
    {
        return "Failed to get response";
    }
    char *result = (char *)malloc(strlen(card_number) + strlen(card_name) + strlen(card_expiration) + strlen(card_type) + 128);
    if (!result)
    {
        return "Failed to allocate memory";
    }
    snprintf(result, strlen(card_number) + strlen(card_name) + strlen(card_expiration) + strlen(card_type) + 128, "Card Number: %s\nName: %s\nExpiration: %s\nType: %s", card_number, card_name, card_expiration, card_type);
    return result;
}
static void flip_library_random_credit_card_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random Credit Card", flip_library_random_credit_card_fetch, flip_library_random_credit_card_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static bool flip_library_random_user_info_fetch(FactLoaderModel *model)
{
    UNUSED(model);
    return flipper_http_get_request("https://fakerapi.it/api/v2/users?_quantity=1&_gender=male");
}
static char *flip_library_random_user_info_parse(FactLoaderModel *model)
{
    UNUSED(model);
    if (!fhttp.last_response)
    {
        return "Failed to get response";
    }
    char *data = get_json_array_value("data", 0, fhttp.last_response, 128);
    if (!data)
    {
        return "Failed to get response";
    }
    char *firstname = get_json_value("firstname", data, 32);
    char *lastname = get_json_value("lastname", data, 32);
    char *email = get_json_value("email", data, 32);
    char *username = get_json_value("username", data, 32);
    char *password = get_json_value("password", data, 32);
    if (!firstname || !lastname || !email || !username || !password)
    {
        return "Failed to get response";
    }
    char *result = (char *)malloc(strlen(firstname) + strlen(lastname) + strlen(email) + strlen(username) + strlen(password) + 128);
    if (!result)
    {
        return "Failed to allocate memory";
    }
    snprintf(result, strlen(firstname) + strlen(lastname) + strlen(email) + strlen(username) + strlen(password) + 128, "First Name: %s\nLast Name: %s\nEmail: %s\nUsername: %s\nPassword: %s", firstname, lastname, email, username, password);
    return result;
}
static void flip_library_random_user_info_switch_to_view(FlipLibraryApp *app)
{
    flip_library_generic_switch_to_view(app, "Random User Info", flip_library_random_user_info_fetch, flip_library_random_user_info_parse, 1, callback_to_random_facts, FlipLibraryViewLoader);
}
//
static void flip_library_request_error_draw(Canvas *canvas)
{
    if (canvas == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_request_error_draw - canvas is NULL");
        DEV_CRASH();
        return;
    }
    if (fhttp.last_response != NULL)
    {
        if (strstr(fhttp.last_response, "[ERROR] Not connected to Wifi. Failed to reconnect.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 22, "Failed to reconnect.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(fhttp.last_response, "[ERROR] Failed to connect to Wifi.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(fhttp.last_response, "[PONG]") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[STATUS]Connecting to AP...");
        }
        else
        {
            canvas_clear(canvas);
            FURI_LOG_E(TAG, "Received an error: %s", fhttp.last_response);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Unusual error...");
            canvas_draw_str(canvas, 0, 60, "Press BACK and retry.");
        }
    }
    else
    {
        canvas_clear(canvas);
        canvas_draw_str(canvas, 0, 10, "Failed to receive data.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
    }
}

static void flip_library_widget_set_text(char *message, Widget **widget)
{
    if (widget == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_set_widget_text - widget is NULL");
        DEV_CRASH();
        return;
    }
    if (message == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_set_widget_text - message is NULL");
        DEV_CRASH();
        return;
    }
    widget_reset(*widget);

    uint32_t message_length = strlen(message); // Length of the message
    uint32_t i = 0;                            // Index tracker
    uint32_t formatted_index = 0;              // Tracker for where we are in the formatted message
    char *formatted_message;                   // Buffer to hold the final formatted message

    // Allocate buffer with double the message length plus one for safety
    if (!easy_flipper_set_buffer(&formatted_message, message_length * 2 + 1))
    {
        return;
    }

    while (i < message_length)
    {
        uint32_t max_line_length = 31;                  // Maximum characters per line
        uint32_t remaining_length = message_length - i; // Remaining characters
        uint32_t line_length = (remaining_length < max_line_length) ? remaining_length : max_line_length;

        // Check for newline character within the current segment
        uint32_t newline_pos = i;
        bool found_newline = false;
        for (; newline_pos < i + line_length && newline_pos < message_length; newline_pos++)
        {
            if (message[newline_pos] == '\n')
            {
                found_newline = true;
                break;
            }
        }

        if (found_newline)
        {
            // If newline found, set line_length up to the newline
            line_length = newline_pos - i;
        }

        // Temporary buffer to hold the current line
        char line[32];
        strncpy(line, message + i, line_length);
        line[line_length] = '\0';

        // If newline was found, skip it for the next iteration
        if (found_newline)
        {
            i += line_length + 1; // +1 to skip the '\n' character
        }
        else
        {
            // Check if the line ends in the middle of a word and adjust accordingly
            if (line_length == max_line_length && message[i + line_length] != '\0' && message[i + line_length] != ' ')
            {
                // Find the last space within the current line to avoid breaking a word
                char *last_space = strrchr(line, ' ');
                if (last_space != NULL)
                {
                    // Adjust the line_length to avoid cutting the word
                    line_length = last_space - line;
                    line[line_length] = '\0'; // Null-terminate at the space
                }
            }

            // Move the index forward by the determined line_length
            i += line_length;

            // Skip any spaces at the beginning of the next line
            while (i < message_length && message[i] == ' ')
            {
                i++;
            }
        }

        // Manually copy the fixed line into the formatted_message buffer
        for (uint32_t j = 0; j < line_length; j++)
        {
            formatted_message[formatted_index++] = line[j];
        }

        // Add a newline character for line spacing
        formatted_message[formatted_index++] = '\n';
    }

    // Null-terminate the formatted_message
    formatted_message[formatted_index] = '\0';

    // Add the formatted message to the widget
    widget_add_text_scroll_element(*widget, 0, 0, 128, 64, formatted_message);
}

void flip_library_loader_draw_callback(Canvas *canvas, void *model)
{
    if (!canvas || !model)
    {
        FURI_LOG_E(TAG, "flip_library_loader_draw_callback - canvas or model is NULL");
        return;
    }

    SerialState http_state = fhttp.state;
    FactLoaderModel *fact_loader_model = (FactLoaderModel *)model;
    FactState fact_state = fact_loader_model->fact_state;
    char *title = fact_loader_model->title;

    canvas_set_font(canvas, FontSecondary);

    if (http_state == INACTIVE)
    {
        canvas_draw_str(canvas, 0, 7, "Wifi Dev Board disconnected.");
        canvas_draw_str(canvas, 0, 17, "Please connect to the board.");
        canvas_draw_str(canvas, 0, 32, "If your board is connected,");
        canvas_draw_str(canvas, 0, 42, "make sure you have flashed");
        canvas_draw_str(canvas, 0, 52, "your WiFi Devboard with the");
        canvas_draw_str(canvas, 0, 62, "latest FlipperHTTP flash.");
        return;
    }

    if (fact_state == FactStateError || fact_state == FactStateParseError)
    {
        flip_library_request_error_draw(canvas);
        return;
    }

    canvas_draw_str(canvas, 0, 7, title);
    canvas_draw_str(canvas, 0, 15, "Loading...");

    if (fact_state == FactStateInitial)
    {
        return;
    }

    if (http_state == SENDING)
    {
        canvas_draw_str(canvas, 0, 22, "Sending...");
        return;
    }

    if (http_state == RECEIVING || fact_state == FactStateRequested)
    {
        canvas_draw_str(canvas, 0, 22, "Receiving...");
        return;
    }

    if (http_state == IDLE && fact_state == FactStateReceived)
    {
        canvas_draw_str(canvas, 0, 22, "Processing...");
        return;
    }

    if (http_state == IDLE && fact_state == FactStateParsed)
    {
        canvas_draw_str(canvas, 0, 22, "Processed...");
        return;
    }
}

static void flip_library_loader_process_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_loader_process_callback - context is NULL");
        DEV_CRASH();
        return;
    }

    FlipLibraryApp *app = (FlipLibraryApp *)context;
    View *view = app->view_loader;

    FactState current_fact_state;
    with_view_model(view, FactLoaderModel * model, { current_fact_state = model->fact_state; }, false);

    if (current_fact_state == FactStateInitial)
    {
        with_view_model(
            view,
            FactLoaderModel * model,
            {
                model->fact_state = FactStateRequested;
                FactLoaderFetch fetch = model->fetcher;
                if (fetch == NULL)
                {
                    FURI_LOG_E(TAG, "Model doesn't have Fetch function assigned.");
                    model->fact_state = FactStateError;
                    return;
                }

                // Clear any previous responses
                strncpy(fhttp.last_response, "", 1);
                bool request_status = fetch(model);
                if (!request_status)
                {
                    model->fact_state = FactStateError;
                }
            },
            true);
    }
    else if (current_fact_state == FactStateRequested || current_fact_state == FactStateError)
    {
        if (fhttp.state == IDLE && fhttp.last_response != NULL)
        {
            if (strstr(fhttp.last_response, "[PONG]") != NULL)
            {
                FURI_LOG_DEV(TAG, "PONG received.");
            }
            else if (strncmp(fhttp.last_response, "[SUCCESS]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "SUCCESS received. %s", fhttp.last_response ? fhttp.last_response : "NULL");
            }
            else if (strncmp(fhttp.last_response, "[ERROR]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "ERROR received. %s", fhttp.last_response ? fhttp.last_response : "NULL");
            }
            else if (strlen(fhttp.last_response) == 0)
            {
                // Still waiting on response
            }
            else
            {
                with_view_model(view, FactLoaderModel * model, { model->fact_state = FactStateReceived; }, true);
            }
        }
        else if (fhttp.state == SENDING || fhttp.state == RECEIVING)
        {
            // continue waiting
        }
        else if (fhttp.state == INACTIVE)
        {
            // inactive. try again
        }
        else if (fhttp.state == ISSUE)
        {
            with_view_model(view, FactLoaderModel * model, { model->fact_state = FactStateError; }, true);
        }
        else
        {
            FURI_LOG_DEV(TAG, "Unexpected state: %d lastresp: %s", fhttp.state, fhttp.last_response ? fhttp.last_response : "NULL");
            DEV_CRASH();
        }
    }
    else if (current_fact_state == FactStateReceived)
    {
        with_view_model(
            view,
            FactLoaderModel * model,
            {
                char *fact_text;
                if (model->parser == NULL)
                {
                    fact_text = NULL;
                    FURI_LOG_DEV(TAG, "Parser is NULL");
                    DEV_CRASH();
                }
                else
                {
                    fact_text = model->parser(model);
                }
                FURI_LOG_DEV(TAG, "Parsed fact: %s\r\ntext: %s", fhttp.last_response ? fhttp.last_response : "NULL", fact_text ? fact_text : "NULL");
                model->fact_text = fact_text;
                if (fact_text == NULL)
                {
                    model->fact_state = FactStateParseError;
                }
                else
                {
                    model->fact_state = FactStateParsed;
                }
            },
            true);
    }
    else if (current_fact_state == FactStateParsed)
    {
        with_view_model(
            view,
            FactLoaderModel * model,
            {
                if (++model->request_index < model->request_count)
                {
                    model->fact_state = FactStateInitial;
                }
                else
                {
                    flip_library_widget_set_text(model->fact_text != NULL ? model->fact_text : "Empty result", &app_instance->widget_result);
                    if (model->fact_text != NULL)
                    {
                        free(model->fact_text);
                        model->fact_text = NULL;
                    }
                    view_set_previous_callback(widget_get_view(app_instance->widget_result), model->back_callback);
                    view_dispatcher_switch_to_view(app_instance->view_dispatcher, FlipLibraryViewWidgetResult);
                }
            },
            true);
    }
}

static void flip_library_loader_timer_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_loader_timer_callback - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipLibraryApp *app = (FlipLibraryApp *)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipLibraryCustomEventProcess);
}

static void flip_library_loader_on_enter(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_loader_on_enter - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipLibraryApp *app = (FlipLibraryApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        FactLoaderModel * model,
        {
            view_set_previous_callback(view, model->back_callback);
            if (model->timer == NULL)
            {
                model->timer = furi_timer_alloc(flip_library_loader_timer_callback, FuriTimerTypePeriodic, app);
            }
            furi_timer_start(model->timer, 250);
        },
        true);
}

static void flip_library_loader_on_exit(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_loader_on_exit - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipLibraryApp *app = (FlipLibraryApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        FactLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_stop(model->timer);
            }
        },
        false);
}

void flip_library_loader_init(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_loader_init - view is NULL");
        DEV_CRASH();
        return;
    }
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FactLoaderModel));
    view_set_enter_callback(view, flip_library_loader_on_enter);
    view_set_exit_callback(view, flip_library_loader_on_exit);
}

void flip_library_loader_free_model(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_loader_free_model - view is NULL");
        DEV_CRASH();
        return;
    }
    with_view_model(
        view,
        FactLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_free(model->timer);
                model->timer = NULL;
            }
            if (model->parser_context)
            {
                free(model->parser_context);
                model->parser_context = NULL;
            }
        },
        false);
}

bool flip_library_custom_event_callback(void *context, uint32_t index)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_custom_event_callback - context is NULL");
        DEV_CRASH();
        return false;
    }

    switch (index)
    {
    case FlipLibraryCustomEventProcess:
        flip_library_loader_process_callback(context);
        return true;
    default:
        FURI_LOG_DEV(TAG, "flip_library_custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}

void callback_submenu_choices(void *context, uint32_t index)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "callback_submenu_choices - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipLibraryApp *app = (FlipLibraryApp *)context;
    switch (index)
    {
    case FlipLibrarySubmenuIndexRandomFacts:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewRandomFacts);
        break;
    case FlipLibrarySubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewAbout);
        break;
    case FlipLibrarySubmenuIndexSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewSettings);
        break;
    case FlipLibrarySubmenuIndexDictionary:
        flip_library_dictionary_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomFactsCats:
        flip_library_cat_fact_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomFactsDogs:
        flip_library_dog_fact_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomFactsQuotes:
        flip_library_quote_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomFactsAll:
        flip_library_random_fact_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomAdvice:
        flip_library_random_advice_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomTrivia:
        flip_library_random_trivia_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomTechPhrase:
        flip_library_random_tech_phrase_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomUUID:
        flip_library_random_uuid_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomAddress:
        flip_library_random_address_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomCreditCard:
        flip_library_random_credit_card_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexRandomUserInfo:
        flip_library_random_user_info_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexWiki:
        flip_library_wiki_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexGPS:
        flip_library_gps_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexWeather:
        flip_library_weather_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexElevation:
        flip_library_elevation_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexAssetPrice:
        flip_library_asset_price_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexNextHoliday:
        flip_library_next_holiday_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexPredict: // predict submenu
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewPredict);
        break;
    case FlipLibrarySubmenuIndexPredictGender:
        flip_library_predict_gender_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexPredictAge:
        flip_library_predict_age_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexPredictEthnicity:
        flip_library_predict_ethnicity_switch_to_view(app);
        break;
    case FlipLibrarySubmenuIndexLibrary:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewSubmenuLibrary);
        break;
    default:
        break;
    }
}

void text_updated_ssid(void *context)
{
    FlipLibraryApp *app = (FlipLibraryApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "text_updated_ssid - FlipLibraryApp is NULL");
        DEV_CRASH();
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer_ssid, app->uart_text_input_temp_buffer_ssid, app->uart_text_input_buffer_size_ssid);

    // Ensure null-termination
    app->uart_text_input_buffer_ssid[app->uart_text_input_buffer_size_ssid - 1] = '\0';

    // update the variable item text
    if (app->variable_item_ssid)
    {
        variable_item_set_current_value_text(app->variable_item_ssid, app->uart_text_input_buffer_ssid);
    }

    // save settings
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password, use_fahrenheit);

    // save wifi settings to devboard
    if (strlen(app->uart_text_input_buffer_ssid) > 0 && strlen(app->uart_text_input_buffer_password) > 0)
    {
        if (!flipper_http_save_wifi(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password))
        {
            FURI_LOG_E(TAG, "Failed to save wifi settings.");
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewSettings);
}

void text_updated_password(void *context)
{
    FlipLibraryApp *app = (FlipLibraryApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "text_updated_password - FlipLibraryApp is NULL");
        DEV_CRASH();
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer_password, app->uart_text_input_temp_buffer_password, app->uart_text_input_buffer_size_password);

    // Ensure null-termination
    app->uart_text_input_buffer_password[app->uart_text_input_buffer_size_password - 1] = '\0';

    // update the variable item text
    if (app->variable_item_password)
    {
        variable_item_set_current_value_text(app->variable_item_password, app->uart_text_input_buffer_password);
    }

    // save settings
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password, use_fahrenheit);

    // save wifi settings to devboard
    if (strlen(app->uart_text_input_buffer_ssid) > 0 && strlen(app->uart_text_input_buffer_password) > 0)
    {
        if (!flipper_http_save_wifi(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password))
        {
            FURI_LOG_E(TAG, "Failed to save wifi settings");
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewSettings);
}

void text_updated_query(void *context)
{
    FlipLibraryApp *app = (FlipLibraryApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "text_updated_query - FlipLibraryApp is NULL");
        DEV_CRASH();
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer_query, app->uart_text_input_temp_buffer_query, app->uart_text_input_buffer_size_query);

    // Ensure null-termination
    app->uart_text_input_buffer_query[app->uart_text_input_buffer_size_query - 1] = '\0';

    // switch to the loader view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewLoader);
}

uint32_t callback_to_submenu(void *context)
{
    UNUSED(context);
    return FlipLibraryViewSubmenuMain;
}

uint32_t callback_to_wifi_settings(void *context)
{
    UNUSED(context);
    return FlipLibraryViewSettings;
}

uint32_t callback_to_random_facts(void *context)
{
    UNUSED(context);
    return FlipLibraryViewRandomFacts;
}

uint32_t callback_to_submenu_library(void *context)
{
    UNUSED(context);
    return FlipLibraryViewSubmenuLibrary;
}

void settings_item_selected(void *context, uint32_t index)
{
    FlipLibraryApp *app = (FlipLibraryApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "settings_item_selected - FlipLibraryApp is NULL");
        DEV_CRASH();
        return;
    }
    switch (index)
    {
    case 0: // Input SSID
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewTextInputSSID);
        break;
    case 1: // Input Password
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipLibraryViewTextInputPassword);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}

/**
 * @brief Navigation callback for exiting the application
 * @param context The context - unused
 * @return next view id (VIEW_NONE to exit the app)
 */
uint32_t callback_exit_app(void *context)
{
    // Exit the application
    if (!context)
    {
        FURI_LOG_E(TAG, "callback_exit_app - Context is NULL");
        return VIEW_NONE;
    }
    UNUSED(context);
    return VIEW_NONE; // Return VIEW_NONE to exit the app
}
uint32_t callback_to_submenu_predict(void *context)
{
    UNUSED(context);
    return FlipLibraryViewPredict;
}

void flip_library_generic_switch_to_view(FlipLibraryApp *app, char *title, FactLoaderFetch fetcher, FactLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id)
{
    if (app == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_generic_switch_to_view - app is NULL");
        DEV_CRASH();
        return;
    }

    View *view = app->view_loader;
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_library_generic_switch_to_view - view is NULL");
        DEV_CRASH();
        return;
    }

    with_view_model(
        view,
        FactLoaderModel * model,
        {
            model->title = title;
            model->fetcher = fetcher;
            model->parser = parser;
            model->request_index = 0;
            model->request_count = request_count;
            model->back_callback = back;
            model->fact_state = FactStateInitial;
            model->fact_text = NULL;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}
