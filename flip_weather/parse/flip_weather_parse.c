#include "parse/flip_weather_parse.h"
#include "stdlib.h"

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

bool sent_get_request = false;
bool get_request_success = false;
bool got_ip_address = false;
bool geo_information_processed = false;
bool weather_information_processed = false;

bool send_geo_location_request()
{
    if (fhttp.state == INACTIVE)
    {
        FURI_LOG_E(TAG, "Board is INACTIVE");
        flipper_http_ping(); // ping the device
        fhttp.state = ISSUE;
        return false;
    }
    char url[512];
    if (strlen(custom_location) > 0)
    {
        // URL-encode custom_location (replace spaces with %20)
        char encoded_location[192];
        size_t j = 0;
        for (size_t i = 0; i < strlen(custom_location) && j < sizeof(encoded_location) - 3; i++)
        {
            if (custom_location[i] == ' ')
            {
                encoded_location[j++] = '%';
                encoded_location[j++] = '2';
                encoded_location[j++] = '0';
            }
            else
            {
                encoded_location[j++] = custom_location[i];
            }
        }
        encoded_location[j] = '\0';
        snprintf(url, sizeof(url), "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=en&format=json", encoded_location);
    }
    else
    {
        snprintf(url, sizeof(url), "https://ipwhois.app/json/");
    }
    if (!flipper_http_get_request_with_headers(url, "{\"Content-Type\": \"application/json\"}"))
    {
        FURI_LOG_E(TAG, "Failed to send GET request");
        fhttp.state = ISSUE;
        return false;
    }
    fhttp.state = RECEIVING;
    return true;
}

bool send_geo_weather_request(DataLoaderModel *model)
{
    UNUSED(model);
    char url[512];
    char *lattitude = lat_data + 10;
    char *longitude = lon_data + 11;
    snprintf(url, 512, "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,precipitation,rain,showers,snowfall,wind_speed_10m,wind_direction_10m,weather_code&temperature_unit=%s&wind_speed_unit=mph&precipitation_unit=inch&forecast_days=1", lattitude, longitude, use_fahrenheit ? "fahrenheit" : "celsius");
    if (!flipper_http_get_request_with_headers(url, "{\"Content-Type\": \"application/json\"}"))
    {
        FURI_LOG_E(TAG, "Failed to send GET request");
        fhttp.state = ISSUE;
        return false;
    }
    fhttp.state = RECEIVING;
    return true;
}
char *process_geo_location(DataLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.last_response == NULL)
    {
        FURI_LOG_E(TAG, "last_response is NULL in process_geo_location");
        fhttp.state = ISSUE;
        return NULL;
    }
    {
        char *latitude = NULL;
        char *longitude = NULL;
        char *city = NULL;
        char *region = NULL;
        char *country = NULL;

        if (strlen(custom_location) > 0)
        {
            // Parse open-meteo geocoding API response: {"results":[{"latitude":...,"longitude":...,"name":...,"admin1":...,"country":...}]}
            char *result = get_json_array_value("results", 0, fhttp.last_response);
            if (result == NULL)
            {
                FURI_LOG_E(TAG, "Failed to get geocoding results");
                fhttp.state = ISSUE;
                return NULL;
            }
            latitude = get_json_value("latitude", result);
            longitude = get_json_value("longitude", result);
            city = get_json_value("name", result);
            region = get_json_value("admin1", result);
            country = get_json_value("country", result);
            free(result);
        }
        else
        {
            city = get_json_value("city", fhttp.last_response);
            region = get_json_value("region", fhttp.last_response);
            country = get_json_value("country", fhttp.last_response);
            latitude = get_json_value("latitude", fhttp.last_response);
            longitude = get_json_value("longitude", fhttp.last_response);
        }

        if (city == NULL || region == NULL || country == NULL || latitude == NULL || longitude == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get geo location data");
            fhttp.state = ISSUE;
            if (city) free(city);
            if (region) free(region);
            if (country) free(country);
            if (latitude) free(latitude);
            if (longitude) free(longitude);
            return NULL;
        }

        snprintf(lat_data, sizeof(lat_data), "Latitude: %s", latitude);
        snprintf(lon_data, sizeof(lon_data), "Longitude: %s", longitude);

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


bool process_geo_location_2()
{
    if (fhttp.last_response == NULL)
    {
        FURI_LOG_E(TAG, "last_response is NULL in process_geo_location_2");
        fhttp.state = ISSUE;
        return false;
    }
    {
        char *latitude = NULL;
        char *longitude = NULL;

        if (strlen(custom_location) > 0)
        {
            // Parse open-meteo geocoding API response
            char *result = get_json_array_value("results", 0, fhttp.last_response);
            if (result == NULL)
            {
                FURI_LOG_E(TAG, "Failed to get geocoding results");
                fhttp.state = ISSUE;
                return false;
            }
            latitude = get_json_value("latitude", result);
            longitude = get_json_value("longitude", result);
            free(result);
        }
        else
        {
            latitude = get_json_value("latitude", fhttp.last_response);
            longitude = get_json_value("longitude", fhttp.last_response);
        }

        if (latitude == NULL || longitude == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get geo location data");
            fhttp.state = ISSUE;
            if (latitude) free(latitude);
            if (longitude) free(longitude);
            return false;
        }

        snprintf(lat_data, sizeof(lat_data), "Latitude: %s", latitude);
        snprintf(lon_data, sizeof(lon_data), "Longitude: %s", longitude);

        fhttp.state = IDLE;
        free(latitude);
        free(longitude);
        return true;
    }
}

char *process_weather(DataLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.last_response == NULL)
    {
        FURI_LOG_E(TAG, "last_response is NULL in process_weather");
        fhttp.state = ISSUE;
        return NULL;
    }
    {
        char *current_data = get_json_value("current", fhttp.last_response);
        if (current_data == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get 'current' object from weather response");
            fhttp.state = ISSUE;
            return NULL;
        }
        char *temperature = get_json_value("temperature_2m", current_data);
        char *precipitation = get_json_value("precipitation", current_data);
        char *rain = get_json_value("rain", current_data);
        char *showers = get_json_value("showers", current_data);
        char *snowfall = get_json_value("snowfall", current_data);
        char *wind_speed = get_json_value("wind_speed_10m", current_data);
        char *wind_direction = get_json_value("wind_direction_10m", current_data);
        char *weather_code = get_json_value("weather_code", current_data);
        char *time = get_json_value("time", current_data);

        if (temperature == NULL || precipitation == NULL || rain == NULL || showers == NULL || snowfall == NULL || wind_speed == NULL || wind_direction == NULL || weather_code == NULL || time == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get weather data fields");
            fhttp.state = ISSUE;
            free(current_data);
            if (temperature) free(temperature);
            if (precipitation) free(precipitation);
            if (rain) free(rain);
            if (showers) free(showers);
            if (snowfall) free(snowfall);
            if (wind_speed) free(wind_speed);
            if (wind_direction) free(wind_direction);
            if (weather_code) free(weather_code);
            if (time) free(time);
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
        return weather_data;
    }
}
