#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <flip_world.h>

#define SETTINGS_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/settings.bin"

void save_settings(
    const char *wifi_ssid,
    const char *wifi_password,
    const char *username,
    const char *password);

bool load_settings(
    char *wifi_ssid,
    size_t wifi_ssid_size,
    char *wifi_password,
    size_t wifi_password_size,
    char *username,
    size_t username_size,
    char *password,
    size_t password_size);

bool save_char(
    const char *path_name, const char *value);

bool load_char(
    const char *path_name,
    char *value,
    size_t value_size);

FuriString *load_furi_world(
    const char *name);

bool world_exists(
    const char *name);

bool save_world_names(const FuriString *json);
bool is_logged_in_to_flip_social();
bool is_logged_in();