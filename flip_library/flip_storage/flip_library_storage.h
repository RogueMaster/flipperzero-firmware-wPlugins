#ifndef FLIP_LIBRARY_STORAGE_H
#define FLIP_LIBRARY_STORAGE_H

#include <furi.h>
#include <storage/storage.h>
#include <flip_library.h>
#define SETTINGS_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/flip_library/settings.bin"

void save_settings(const char *ssid, const char *password, bool use_fahrenheit);

bool load_settings(char *ssid, size_t ssid_size, char *password, size_t password_size, bool *use_fahrenheit);

#endif // FLIP_LIBRARY_STORAGE_H
