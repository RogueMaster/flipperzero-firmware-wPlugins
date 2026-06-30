
#include "flip_storage/flip_weather_storage.h"

void save_settings(
    const char *ssid,
    const char *password,
    const char *custom_location,
    bool use_fahrenheit)
{
    // Create the directory for saving settings
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_weather");

    // Create the directory
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);

    // Open the settings file
    File *file = storage_file_alloc(storage);
    if (!storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E(TAG, "Failed to open settings file for writing: %s", SETTINGS_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    // Save the ssid length and data
    size_t ssid_length = strlen(ssid) + 1; // Include null terminator
    if (storage_file_write(file, &ssid_length, sizeof(size_t)) != sizeof(size_t) ||
        storage_file_write(file, ssid, ssid_length) != ssid_length)
    {
        FURI_LOG_E(TAG, "Failed to write SSID");
    }

    // Save the password length and data
    size_t password_length = strlen(password) + 1; // Include null terminator
    if (storage_file_write(file, &password_length, sizeof(size_t)) != sizeof(size_t) ||
        storage_file_write(file, password, password_length) != password_length)
    {
        FURI_LOG_E(TAG, "Failed to write password");
    }

    // Save the temperature unit
    uint8_t temp_unit = use_fahrenheit ? 1 : 0;
    if (storage_file_write(file, &temp_unit, sizeof(uint8_t)) != sizeof(uint8_t))
    {
        FURI_LOG_E(TAG, "Failed to write temperature unit");
    }

    // Save the custom location length and data
    size_t custom_location_length = strlen(custom_location) + 1; // Include null terminator
    if (storage_file_write(file, &custom_location_length, sizeof(size_t)) != sizeof(size_t) ||
        storage_file_write(file, custom_location, custom_location_length) != custom_location_length)
    {
        FURI_LOG_E(TAG, "Failed to write custom location");
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool load_settings(
    char *ssid,
    size_t ssid_size,
    char *password,
    size_t password_size,
    char *custom_location,
    size_t custom_location_size,
    bool *use_fahrenheit)
{
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    if (!storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        FURI_LOG_E(TAG, "Failed to open settings file for reading: %s", SETTINGS_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false; // Return false if the file does not exist
    }

    // Load the ssid
    size_t ssid_length;
    if (storage_file_read(file, &ssid_length, sizeof(size_t)) != sizeof(size_t) || ssid_length > ssid_size ||
        storage_file_read(file, ssid, ssid_length) != ssid_length)
    {
        FURI_LOG_E(TAG, "Failed to read SSID");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    ssid[ssid_length - 1] = '\0'; // Ensure null-termination

    // Load the password
    size_t password_length;
    if (storage_file_read(file, &password_length, sizeof(size_t)) != sizeof(size_t) || password_length > password_size ||
        storage_file_read(file, password, password_length) != password_length)
    {
        FURI_LOG_E(TAG, "Failed to read password");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    password[password_length - 1] = '\0'; // Ensure null-termination

    // Load the temperature unit (optional — may not exist in older settings files)
    *use_fahrenheit = false;
    uint8_t temp_unit = 0;
    if (storage_file_read(file, &temp_unit, sizeof(uint8_t)) == sizeof(uint8_t))
    {
        *use_fahrenheit = (temp_unit == 1);
    }

    // Load the custom location (optional — may not exist in older settings files)
    custom_location[0] = '\0';
    size_t custom_location_length;
    if (storage_file_read(file, &custom_location_length, sizeof(size_t)) == sizeof(size_t) &&
        custom_location_length <= custom_location_size &&
        storage_file_read(file, custom_location, custom_location_length) == custom_location_length)
    {
        custom_location[custom_location_length - 1] = '\0'; // Ensure null-termination
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return true;
}