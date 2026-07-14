#ifndef FLIP_TRADER_STORAGE_H
#define FLIP_TRADER_STORAGE_H

#include <furi.h>
#include <storage/storage.h>
#include <flip_trader.h>

#define SETTINGS_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/flip_trader/settings.bin"

void save_settings(const char* ssid, const char* password);

bool load_settings(char* ssid, size_t ssid_size, char* password, size_t password_size);

#endif // FLIP_TRADER_STORAGE_H
