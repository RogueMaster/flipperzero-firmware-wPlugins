#include <flip_storage/flip_wifi_storage.h>

static char *app_ids[8] = {
    "flip_wifi",
    "flip_store",
    "flip_social",
    "flip_trader",
    "flip_weather",
    "flip_library",
    "web_crawler",
    "flip_world"};

// Function to save the playlist
void save_playlist(WiFiPlaylist *playlist)
{
    if (!playlist)
    {
        FURI_LOG_E(TAG, "Playlist is NULL");
        return;
    }

    // Create the directory for saving settings
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi");

    // Open storage
    Storage *storage = furi_record_open(RECORD_STORAGE);
    if (!storage)
    {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return;
    }

    // Create the directory
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi/data");
    storage_common_mkdir(storage, directory_path);

    // Open the settings file
    File *file = storage_file_alloc(storage);
    if (!file)
    {
        FURI_LOG_E(TAG, "Failed to allocate file handle");
        furi_record_close(RECORD_STORAGE);
        return;
    }
    if (!storage_file_open(file, WIFI_SSID_LIST_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E(TAG, "Failed to open settings file for writing: %s", WIFI_SSID_LIST_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FuriString *json_result = furi_string_alloc();
    if (!json_result)
    {
        FURI_LOG_E(TAG, "Failed to allocate FuriString");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    furi_string_cat(json_result, "{\"ssids\":[\n");
    for (size_t i = 0; i < playlist->count; i++)
    {
        furi_string_cat_printf(json_result, "{\"ssid\":\"%s\",\"password\":\"%s\"}", playlist->ssids[i], playlist->passwords[i]);
        if (i < playlist->count - 1)
        {
            furi_string_cat(json_result, ",\n");
        }
    }
    furi_string_cat(json_result, "\n]}");
    size_t json_length = furi_string_size(json_result);
    if (storage_file_write(file, furi_string_get_cstr(json_result), json_length) != json_length)
    {
        FURI_LOG_E(TAG, "Failed to write playlist to file");
    }
    furi_string_free(json_result);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool load_playlist(WiFiPlaylist *playlist)
{
    if (!playlist)
    {
        FURI_LOG_E(TAG, "Playlist is NULL");
        return false;
    }

    FuriString *json_result = flipper_http_load_from_file(WIFI_SSID_LIST_PATH);
    if (!json_result)
    {
        FURI_LOG_E(TAG, "Failed to load playlist from file");
        return false;
    }

    // Initialize playlist count
    playlist->count = 0;

    // Parse the JSON result
    for (size_t i = 0; i < MAX_SAVED_NETWORKS; i++)
    {
        FuriString *json_data = get_json_array_value_furi("ssids", i, json_result);
        if (!json_data)
        {
            break;
        }
        FuriString *ssid = get_json_value_furi("ssid", json_data);
        FuriString *password = get_json_value_furi("password", json_data);
        if (!ssid || !password)
        {
            FURI_LOG_E(TAG, "Failed to get SSID or Password from JSON");
            furi_string_free(json_data);
            break;
        }
        snprintf(playlist->ssids[i], MAX_SSID_LENGTH, "%s", furi_string_get_cstr(ssid));
        snprintf(playlist->passwords[i], MAX_SSID_LENGTH, "%s", furi_string_get_cstr(password));
        playlist->count++;
        furi_string_free(json_data);
        furi_string_free(ssid);
        furi_string_free(password);
    }
    furi_string_free(json_result);
    return true;
}

void save_settings(const char *ssid, const char *password)
{
    char edited_directory_path[128];
    char edited_file_path[128];

    for (size_t i = 0; i < 8; i++)
    {
        // Construct the directory and file paths for the current app
        snprintf(edited_directory_path, sizeof(edited_directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s", app_ids[i]);
        snprintf(edited_file_path, sizeof(edited_file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/settings.bin", app_ids[i]);

        // Open the storage record
        Storage *storage = furi_record_open(RECORD_STORAGE);
        if (!storage)
        {
            FURI_LOG_E(TAG, "Failed to open storage record for app: %s", app_ids[i]);
            continue; // Skip to the next app
        }

        // Ensure the directory exists
        storage_common_mkdir(storage, edited_directory_path);

        // Allocate a file handle
        File *file = storage_file_alloc(storage);
        if (!file)
        {
            FURI_LOG_E(TAG, "Failed to allocate storage file for app: %s", app_ids[i]);
            furi_record_close(RECORD_STORAGE);
            continue; // Skip to the next app
        }

        // Open the file in read mode to read existing data
        bool file_opened = storage_file_open(file, edited_file_path, FSAM_READ, FSOM_OPEN_EXISTING);
        size_t file_size = 0;
        uint8_t *buffer = NULL;

        if (file_opened)
        {
            // Get the file size
            file_size = storage_file_size(file);
            buffer = malloc(file_size);
            if (!buffer)
            {
                FURI_LOG_E(TAG, "Failed to allocate buffer for app: %s", app_ids[i]);
                storage_file_close(file);
                storage_file_free(file);
                furi_record_close(RECORD_STORAGE);
                continue;
            }

            // Read the existing data
            if (storage_file_read(file, buffer, file_size) != file_size)
            {
                FURI_LOG_E(TAG, "Failed to read settings file for app: %s", app_ids[i]);
                free(buffer);
                storage_file_close(file);
                storage_file_free(file);
                furi_record_close(RECORD_STORAGE);
                continue;
            }

            storage_file_close(file);
        }
        else
        {
            // If the file doesn't exist, initialize an empty buffer
            file_size = 0;
            buffer = NULL;
        }

        storage_file_free(file);

        // Prepare new SSID and Password
        size_t new_ssid_length = strlen(ssid) + 1;         // Including null terminator
        size_t new_password_length = strlen(password) + 1; // Including null terminator

        // Calculate the new file size
        size_t new_file_size = sizeof(size_t) + new_ssid_length + sizeof(size_t) + new_password_length;

        // If there is additional data beyond SSID and Password, preserve it
        size_t additional_data_size = 0;
        uint8_t *additional_data = NULL;

        if (buffer)
        {
            // Parse existing SSID length
            if (file_size >= sizeof(size_t))
            {
                size_t existing_ssid_length;
                memcpy(&existing_ssid_length, buffer, sizeof(size_t));

                // Parse existing Password length
                if (file_size >= sizeof(size_t) + existing_ssid_length + sizeof(size_t))
                {
                    size_t existing_password_length;
                    memcpy(&existing_password_length, buffer + sizeof(size_t) + existing_ssid_length, sizeof(size_t));

                    // Calculate the offset where additional data starts
                    size_t additional_offset = sizeof(size_t) + existing_ssid_length + sizeof(size_t) + existing_password_length;
                    if (additional_offset < file_size)
                    {
                        additional_data_size = file_size - additional_offset;
                        additional_data = malloc(additional_data_size);
                        if (additional_data)
                        {
                            memcpy(additional_data, buffer + additional_offset, additional_data_size);
                        }
                        else
                        {
                            FURI_LOG_E(TAG, "Failed to allocate memory for additional data for app: %s", app_ids[i]);
                            free(buffer);
                            furi_record_close(RECORD_STORAGE);
                            continue;
                        }
                    }
                }
                else
                {
                    FURI_LOG_E(TAG, "Settings file format invalid for app: %s", app_ids[i]);
                }
            }
            else
            {
                FURI_LOG_E(TAG, "Settings file too small for app: %s", app_ids[i]);
            }
        }

        // Allocate a new buffer for updated data
        size_t total_new_size = new_file_size + additional_data_size;
        uint8_t *new_buffer = malloc(total_new_size);
        if (!new_buffer)
        {
            FURI_LOG_E(TAG, "Failed to allocate new buffer for app: %s", app_ids[i]);
            if (buffer)
                free(buffer);
            if (additional_data)
                free(additional_data);
            furi_record_close(RECORD_STORAGE);
            continue;
        }

        size_t offset = 0;

        // Write new SSID length and SSID
        memcpy(new_buffer + offset, &new_ssid_length, sizeof(size_t));
        offset += sizeof(size_t);
        memcpy(new_buffer + offset, ssid, new_ssid_length);
        offset += new_ssid_length;

        // Write new Password length and Password
        memcpy(new_buffer + offset, &new_password_length, sizeof(size_t));
        offset += sizeof(size_t);
        memcpy(new_buffer + offset, password, new_password_length);
        offset += new_password_length;

        // Append any additional data if present
        if (additional_data_size > 0 && additional_data)
        {
            memcpy(new_buffer + offset, additional_data, additional_data_size);
            offset += additional_data_size;
        }

        // Free temporary buffers
        if (buffer)
            free(buffer);
        if (additional_data)
            free(additional_data);

        // Open the file in write mode with FSOM_CREATE_ALWAYS to overwrite it
        file = storage_file_alloc(storage);
        if (!file)
        {
            FURI_LOG_E(TAG, "Failed to allocate storage file for writing: %s", app_ids[i]);
            free(new_buffer);
            furi_record_close(RECORD_STORAGE);
            continue;
        }

        if (!storage_file_open(file, edited_file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        {
            FURI_LOG_E(TAG, "Failed to open settings file for writing: %s", edited_file_path);
            storage_file_free(file);
            free(new_buffer);
            furi_record_close(RECORD_STORAGE);
            continue;
        }

        // Write the updated buffer back to the file
        if (storage_file_write(file, new_buffer, total_new_size) != total_new_size)
        {
            FURI_LOG_E(TAG, "Failed to write updated settings for app: %s", app_ids[i]);
        }

        // Clean up
        free(new_buffer);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
    }
}
bool save_char(
    const char *path_name, const char *value)
{
    if (!value)
    {
        return false;
    }
    // Create the directory for saving settings
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi");

    // Create the directory
    Storage *storage = furi_record_open(RECORD_STORAGE);
    if (!storage)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to open storage record");
        return false;
    }
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi/data");
    storage_common_mkdir(storage, directory_path);

    // Open the settings file
    File *file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi/data/%s.txt", path_name);

    // Open the file in write mode
    if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E(HTTP_TAG, "Failed to open file for writing: %s", file_path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Write the data to the file
    size_t data_size = strlen(value) + 1; // Include null terminator
    if (storage_file_write(file, value, data_size) != data_size)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to append data to file");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return true;
}

bool load_char(
    const char *path_name,
    char *value,
    size_t value_size)
{
    if (!value)
    {
        return false;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    char file_path[256];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi/data/%s.txt", path_name);

    // Open the file for reading
    if (!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false; // Return false if the file does not exist
    }

    // Read data into the buffer
    size_t read_count = storage_file_read(file, value, value_size);
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E(HTTP_TAG, "Error reading from file.");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Ensure null-termination
    value[read_count - 1] = '\0';

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return true;
}