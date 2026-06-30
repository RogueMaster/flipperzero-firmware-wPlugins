#pragma once

#include <flip_wifi.h>

// define the paths for all of the FlipperHTTP apps
#define WIFI_SSID_LIST_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi/data/wifi_list.txt"

// Function to save the playlist
void save_playlist(WiFiPlaylist *playlist);

// Function to load the playlist
bool load_playlist(WiFiPlaylist *playlist);

void save_settings(const char *ssid, const char *password);

bool save_char(
    const char *path_name, const char *value);

bool load_char(
    const char *path_name,
    char *value,
    size_t value_size);
