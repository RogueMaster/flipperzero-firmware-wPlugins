#pragma once
#include <web_crawler.h>
#include <furi.h>
#include <storage/storage.h>

#define SETTINGS_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/" http_tag "/settings.bin"
#define RECEIVED_DATA_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/" http_tag "/" // add the file name to the end (e.g. "received_data.txt")

// Function to save settings: path, SSID, and password
void save_settings(
    const char *path,
    const char *ssid,
    const char *password,
    const char *file_rename,
    const char *file_type,
    const char *http_method,
    const char *headers,
    const char *payload);

// Function to load settings (the variables must be opened in the order they were saved)
bool load_settings(
    char *path,
    size_t path_size,
    char *ssid,
    size_t ssid_size,
    char *password,
    size_t password_size,
    char *file_rename,
    size_t file_rename_size,
    char *file_type,
    size_t file_type_size,
    char *http_method,
    size_t http_method_size,
    char *headers,
    size_t headers_size,
    char *payload,
    size_t payload_size,
    WebCrawlerApp *app);

bool delete_received_data();
bool rename_received_data(const char *old_name, const char *new_name, const char *file_type, const char *old_file_type);

bool save_char(
    const char *path_name, const char *value);

bool load_char(
    const char *path_name,
    char *value,
    size_t value_size);
