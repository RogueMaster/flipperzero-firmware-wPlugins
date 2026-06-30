#ifndef FLIP_WEATHER_STORAGE_H
#define FLIP_WEATHER_STORAGE_H

#include <furi.h>
#include <storage/storage.h>
#include <flip_weather.h>

#define SETTINGS_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/flip_weather/settings.bin"

void save_settings(
    const char *ssid,
    const char *password,
    const char *custom_location,
    bool use_fahrenheit);

bool load_settings(
    char *ssid,
    size_t ssid_size,
    char *password,
    size_t password_size,
    char *custom_location,
    size_t custom_location_size,
    bool *use_fahrenheit);
#endif // FLIP_WEATHER_STORAGE_H