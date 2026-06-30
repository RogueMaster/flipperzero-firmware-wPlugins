
#include "flip_storage/storage.h"

// Forward declaration for use in other functions
static bool load_flip_social_settings(
    char *ssid,
    size_t ssid_size,
    char *password,
    size_t password_size,
    char *login_username_logged_out,
    size_t username_out_size,
    char *login_username_logged_in,
    size_t username_in_size,
    char *login_password_logged_out,
    size_t password_out_size,
    char *change_password_logged_in,
    size_t change_password_size,
    char *change_bio_logged_in,
    size_t change_bio_size,
    char *is_logged_in,
    size_t is_logged_in_size);

void save_settings(
    const char *wifi_ssid,
    const char *wifi_password,
    const char *username,
    const char *password)
{
    // Create the directory for saving settings
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");

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

    // Save the wifi_ssid length and data
    size_t wifi_ssid_length = strlen(wifi_ssid) + 1; // Include null terminator
    if (storage_file_write(file, &wifi_ssid_length, sizeof(size_t)) != sizeof(size_t) ||
        storage_file_write(file, wifi_ssid, wifi_ssid_length) != wifi_ssid_length)
    {
        FURI_LOG_E(TAG, "Failed to write wifi_SSID");
    }

    // Save the wifi_password length and data
    size_t wifi_password_length = strlen(wifi_password) + 1; // Include null terminator
    if (storage_file_write(file, &wifi_password_length, sizeof(size_t)) != sizeof(size_t) ||
        storage_file_write(file, wifi_password, wifi_password_length) != wifi_password_length)
    {
        FURI_LOG_E(TAG, "Failed to write wifi_password");
    }

    // Save the username length and data
    size_t username_length = strlen(username) + 1; // Include null terminator
    if (storage_file_write(file, &username_length, sizeof(size_t)) != sizeof(size_t) ||
        storage_file_write(file, username, username_length) != username_length)
    {
        FURI_LOG_E(TAG, "Failed to write username");
    }

    // Save the password length and data
    size_t password_length = strlen(password) + 1; // Include null terminator
    if (storage_file_write(file, &password_length, sizeof(size_t)) != sizeof(size_t) ||
        storage_file_write(file, password, password_length) != password_length)
    {
        FURI_LOG_E(TAG, "Failed to write password");
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool load_settings(
    char *wifi_ssid,
    size_t wifi_ssid_size,
    char *wifi_password,
    size_t wifi_password_size,
    char *username,
    size_t username_size,
    char *password,
    size_t password_size)
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

    // Load the wifi_ssid
    size_t wifi_ssid_length;
    if (storage_file_read(file, &wifi_ssid_length, sizeof(size_t)) != sizeof(size_t) || wifi_ssid_length > wifi_ssid_size ||
        storage_file_read(file, wifi_ssid, wifi_ssid_length) != wifi_ssid_length)
    {
        FURI_LOG_E(TAG, "Failed to read wifi_SSID");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    wifi_ssid[wifi_ssid_length - 1] = '\0'; // Ensure null-termination

    // Load the wifi_password
    size_t wifi_password_length;
    if (storage_file_read(file, &wifi_password_length, sizeof(size_t)) != sizeof(size_t) || wifi_password_length > wifi_password_size ||
        storage_file_read(file, wifi_password, wifi_password_length) != wifi_password_length)
    {
        FURI_LOG_E(TAG, "Failed to read wifi_password");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    wifi_password[wifi_password_length - 1] = '\0'; // Ensure null-termination

    // Load the username
    size_t username_length;
    if (storage_file_read(file, &username_length, sizeof(size_t)) != sizeof(size_t) || username_length > username_size ||
        storage_file_read(file, username, username_length) != username_length)
    {
        FURI_LOG_E(TAG, "Failed to read username");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    username[username_length - 1] = '\0'; // Ensure null-termination

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

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return true;
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
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");

    // Create the directory
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data");
    storage_common_mkdir(storage, directory_path);

    // Open the settings file
    File *file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/%s.txt", path_name);

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
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/%s.txt", path_name);

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

    return strlen(value) > 0;
}

FuriString *load_furi_world(
    const char *name)
{
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);
    char file_path[128];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", name);
    // Open the file for reading
    if (!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL; // Return false if the file does not exist
    }

    // Allocate a FuriString to hold the received data
    FuriString *str_result = furi_string_alloc();
    if (!str_result)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate FuriString");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    // Reset the FuriString to ensure it's empty before reading
    furi_string_reset(str_result);

    // Define a buffer to hold the read data
    uint8_t *buffer = (uint8_t *)malloc(MAX_FILE_SHOW);
    if (!buffer)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate buffer");
        furi_string_free(str_result);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Read data into the buffer
    size_t read_count = storage_file_read(file, buffer, MAX_FILE_SHOW);
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E(HTTP_TAG, "Error reading from file.");
        furi_string_free(str_result);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Append each byte to the FuriString
    for (size_t i = 0; i < read_count; i++)
    {
        furi_string_push_back(str_result, buffer[i]);
    }

    // Check if there is more data beyond the maximum size
    char extra_byte;
    storage_file_read(file, &extra_byte, 1);

    // Clean up
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    free(buffer);
    return str_result;
}

bool world_exists(const char *name)
{
    if (!name)
    {
        FURI_LOG_E(TAG, "Invalid name");
        return false;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    if (!storage)
    {
        FURI_LOG_E(TAG, "Failed to open storage");
        return false;
    }
    char file_path[256];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", name);
    bool does_exist = storage_file_exists(storage, file_path);

    // Clean up
    furi_record_close(RECORD_STORAGE);
    return does_exist;
}

bool save_world_names(const FuriString *json)
{
    // Create the directory for saving settings
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds");

    // Create the directory
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);

    // Open the settings file
    File *file = storage_file_alloc(storage);
    char file_path[128];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");

    // Open the file in write mode
    if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E(HTTP_TAG, "Failed to open file for writing: %s", file_path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Write the data to the file
    size_t data_size = furi_string_size(json) + 1; // Include null terminator
    if (storage_file_write(file, furi_string_get_cstr(json), data_size) != data_size)
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

static FuriString *flip_social_info(char *key)
{
    char ssid[64];
    char password[64];
    char login_username_logged_out[64];
    char login_username_logged_in[64];
    char login_password_logged_out[64];
    char change_password_logged_in[64];
    char change_bio_logged_in[64];
    char is_logged_in[64];
    FuriString *result = furi_string_alloc();
    if (!result)
    {
        FURI_LOG_E(TAG, "Failed to allocate FuriString");
        return NULL;
    }
    if (!load_flip_social_settings(ssid, sizeof(ssid), password, sizeof(password), login_username_logged_out, sizeof(login_username_logged_out), login_username_logged_in, sizeof(login_username_logged_in), login_password_logged_out, sizeof(login_password_logged_out), change_password_logged_in, sizeof(change_password_logged_in), change_bio_logged_in, sizeof(change_bio_logged_in), is_logged_in, sizeof(is_logged_in)))
    {
        FURI_LOG_E(TAG, "Failed to load flip social settings");
        return NULL;
    }
    if (strcmp(key, "ssid") == 0)
    {
        furi_string_set_str(result, ssid);
    }
    else if (strcmp(key, "password") == 0)
    {
        furi_string_set_str(result, password);
    }
    else if (strcmp(key, "login_username_logged_out") == 0)
    {
        furi_string_set_str(result, login_username_logged_out);
    }
    else if (strcmp(key, "login_username_logged_in") == 0)
    {
        furi_string_set_str(result, login_username_logged_in);
    }
    else if (strcmp(key, "login_password_logged_out") == 0)
    {
        furi_string_set_str(result, login_password_logged_out);
    }
    else if (strcmp(key, "change_password_logged_in") == 0)
    {
        furi_string_set_str(result, change_password_logged_in);
    }
    else if (strcmp(key, "change_bio_logged_in") == 0)
    {
        furi_string_set_str(result, change_bio_logged_in);
    }
    else if (strcmp(key, "is_logged_in") == 0)
    {
        furi_string_set_str(result, is_logged_in);
    }
    else
    {
        FURI_LOG_E(TAG, "Invalid key");
        furi_string_free(result);
        return NULL;
    }
    return result;
}

bool is_logged_in_to_flip_social()
{
    // load flip social settings and check if logged in
    FuriString *is_logged_in = flip_social_info("is_logged_in");
    if (!is_logged_in)
    {
        FURI_LOG_E(TAG, "Failed to load is_logged_in");
        return false;
    }
    if (furi_string_cmp(is_logged_in, "true") == 0)
    {
        // copy the logged_in FlipSocaial settings to FlipWorld
        FuriString *username = flip_social_info("login_username_logged_in");
        FuriString *password = flip_social_info("change_password_logged_in");
        FuriString *wifi_password = flip_social_info("password");
        FuriString *wifi_ssid = flip_social_info("ssid");
        if (!username || !password || !wifi_password || !wifi_ssid)
        {
            furi_string_free(username);
            furi_string_free(password);
            furi_string_free(wifi_password);
            furi_string_free(wifi_ssid);
            return false;
        }
        save_settings(furi_string_get_cstr(wifi_ssid), furi_string_get_cstr(wifi_password), furi_string_get_cstr(username), furi_string_get_cstr(password));
        furi_string_free(username);
        furi_string_free(password);
        furi_string_free(wifi_password);
        furi_string_free(wifi_ssid);
        furi_string_free(is_logged_in);
        return true;
    }
    furi_string_free(is_logged_in);
    return false;
}

bool is_logged_in()
{
    char is_logged_in[64];
    if (load_char("is_logged_in", is_logged_in, sizeof(is_logged_in)))
    {
        return strcmp(is_logged_in, "true") == 0;
    }
    return false;
}

static bool load_flip_social_settings(
    char *ssid,
    size_t ssid_size,
    char *password,
    size_t password_size,
    char *login_username_logged_out,
    size_t username_out_size,
    char *login_username_logged_in,
    size_t username_in_size,
    char *login_password_logged_out,
    size_t password_out_size,
    char *change_password_logged_in,
    size_t change_password_size,
    char *change_bio_logged_in,
    size_t change_bio_size,
    char *is_logged_in,
    size_t is_logged_in_size)
{
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    // file path from flipsocial
    char file_path[128];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_social/settings.bin");
    if (!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        FURI_LOG_E(TAG, "Failed to open settings file for reading: %s", file_path);
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
    else
    {
        ssid[ssid_length - 1] = '\0'; // Ensure null-termination
    }

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
    else
    {
        password[password_length - 1] = '\0'; // Ensure null-termination
    }

    // Load the login_username_logged_out
    size_t username_out_length;
    if (storage_file_read(file, &username_out_length, sizeof(size_t)) != sizeof(size_t) || username_out_length > username_out_size ||
        storage_file_read(file, login_username_logged_out, username_out_length) != username_out_length)
    {
        FURI_LOG_E(TAG, "Failed to read login_username_logged_out");
        // storage_file_close(file);
        // storage_file_free(file);
        // furi_record_close(RECORD_STORAGE);
        // return false;
    }
    else
    {
        login_username_logged_out[username_out_length - 1] = '\0'; // Ensure null-termination
    }

    // Load the login_username_logged_in
    size_t username_in_length;
    if (storage_file_read(file, &username_in_length, sizeof(size_t)) != sizeof(size_t) || username_in_length > username_in_size ||
        storage_file_read(file, login_username_logged_in, username_in_length) != username_in_length)
    {
        FURI_LOG_E(TAG, "Failed to read login_username_logged_in");
        // storage_file_close(file);
        // storage_file_free(file);
        // furi_record_close(RECORD_STORAGE);
        // return false;
    }
    else
    {
        login_username_logged_in[username_in_length - 1] = '\0'; // Ensure null-termination
    }

    // Load the login_password_logged_out
    size_t password_out_length;
    if (storage_file_read(file, &password_out_length, sizeof(size_t)) != sizeof(size_t) || password_out_length > password_out_size ||
        storage_file_read(file, login_password_logged_out, password_out_length) != password_out_length)
    {
        FURI_LOG_E(TAG, "Failed to read login_password_logged_out");
        // storage_file_close(file);
        // storage_file_free(file);
        // furi_record_close(RECORD_STORAGE);
        // return false;
    }
    else
    {
        login_password_logged_out[password_out_length - 1] = '\0'; // Ensure null-termination
    }

    // Load the change_password_logged_in
    size_t change_password_length;
    if (storage_file_read(file, &change_password_length, sizeof(size_t)) != sizeof(size_t) || change_password_length > change_password_size ||
        storage_file_read(file, change_password_logged_in, change_password_length) != change_password_length)
    {
        FURI_LOG_E(TAG, "Failed to read change_password_logged_in");
        // storage_file_close(file);
        // storage_file_free(file);
        // furi_record_close(RECORD_STORAGE);
        //  return false;
    }
    else
    {
        change_password_logged_in[change_password_length - 1] = '\0'; // Ensure null-termination
    }

    // Load the is_logged_in
    size_t is_logged_in_length;
    if (storage_file_read(file, &is_logged_in_length, sizeof(size_t)) != sizeof(size_t) || is_logged_in_length > is_logged_in_size ||
        storage_file_read(file, is_logged_in, is_logged_in_length) != is_logged_in_length)
    {
        FURI_LOG_E(TAG, "Failed to read is_logged_in");
        // storage_file_close(file);
        // storage_file_free(file);
        // furi_record_close(RECORD_STORAGE);
        //  return false;
    }
    else
    {
        is_logged_in[is_logged_in_length - 1] = '\0'; // Ensure null-termination
    }

    // Load the change_bio_logged_in
    size_t change_bio_length;
    if (storage_file_read(file, &change_bio_length, sizeof(size_t)) != sizeof(size_t) || change_bio_length > change_bio_size ||
        storage_file_read(file, change_bio_logged_in, change_bio_length) != change_bio_length)
    {
        FURI_LOG_E(TAG, "Failed to read change_bio_logged_in");
        // storage_file_close(file);
        // storage_file_free(file);
        // furi_record_close(RECORD_STORAGE);
        //  return false;
    }
    else
    {
        change_bio_logged_in[change_bio_length - 1] = '\0'; // Ensure null-termination
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return true;
}
