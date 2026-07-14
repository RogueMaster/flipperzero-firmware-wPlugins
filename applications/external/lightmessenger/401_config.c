/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401_config.h"
#include "401LightMsg_config.h"

static const char* TAG = "401_Configuration";

l401_err config_alloc(Configuration** config) {
    if(*config != NULL) {
        free(*config);
    }

    // Allocate new memory
    *config = (Configuration*)malloc(sizeof(Configuration));

    // Initialize the newly allocated memory or handle malloc failure
    if(*config == NULL) {
        return L401_ERR_INTERNAL;
    }

    return L401_OK;
}

void config_default_init(Configuration* config) {
    if(config == NULL) {
        return;
    }
    // Initialize with default values
    config->version = strdup(LIGHTMSG_VERSION); // Default version
    strncpy(config->text, LIGHTMSG_DEFAULT_TEXT, LIGHTMSG_MAX_TEXT_LEN); // Default text
    config->text[LIGHTMSG_MAX_TEXT_LEN] = '\0';
    strncpy(
        config->bitmapPath,
        LIGHTMSG_DEFAULT_BITMAPPATH,
        LIGHTMSG_MAX_BITMAPPATH_LEN); // Default text
    config->bitmapPath[LIGHTMSG_MAX_BITMAPPATH_LEN] = '\0';
    config->color = LIGHTMSG_DEFAULT_COLOR; // Default color
    config->brightness = LIGHTMSG_DEFAULT_BRIGHTNESS; // Default brightness
    config->sensitivity = LIGHTMSG_DEFAULT_SENSIBILITY; // Default sensitivity
    config->orientation = LIGHTMSG_DEFAULT_ORIENTATION; // Default orientation (e.g., false)
    // Add version 1.1 features
    // from Jamisonderek https://github.com/lab-401/fzLightMessenger/pull/5/commits
    config->mirror = LIGHTMSG_DEFAULT_MIRROR;
    config->speed = LIGHTMSG_DEFAULT_SPEED;
    config->width = LIGHTMSG_DEFAULT_WIDTH;
}

/**
 * @brief Converts an Configuration structure to a JSON string.
 *
 * This function takes a pointer to an Configuration structure and
 * converts its contents into a JSON formatted string. The generated
 * JSON string is dynamically allocated and should be freed by the caller.
 *
 * @param config Pointer to the Configuration structure to be converted.
 * @param jsontxt Pointer to a char pointer where the JSON string will be stored.
 *               The function allocates memory for this string, and it is
 *               the caller's responsibility to free it.
 *
 * @return l401_err An enumerated error code.
 *         - L401_OK on success.
 *         - L401_ERR_NULLPTR if input pointers are NULL.
 *         - L401_ERR_INTERNAL if an internal cJSON error occurs.
 */
l401_err config_to_json(Configuration* config, char** jsontxt) {
    if(config == NULL) {
        return L401_ERR_NULLPTR;
    }

    // Create a new cJSON object
    cJSON* json = cJSON_CreateObject();
    if(json == NULL) {
        return L401_ERR_INTERNAL;
    }

    // Add items to the cJSON object
    cJSON_AddStringToObject(json, "version", config->version);
    cJSON_AddStringToObject(json, "text", config->text);
    cJSON_AddStringToObject(json, "bitmapPath", config->bitmapPath);
    cJSON_AddNumberToObject(json, "color", (double)config->color);
    // overrites brightness setting
    cJSON_AddNumberToObject(
        json, "brightness", LightMsg_BrightnessBlinding); // (double)config->brightness);
    cJSON_AddNumberToObject(json, "sensitivity", (double)config->sensitivity);
    cJSON_AddBoolToObject(json, "orientation", config->orientation);
    // Add version 1.1 features
    cJSON_AddBoolToObject(json, "mirror", config->mirror);
    cJSON_AddNumberToObject(json, "speed", config->speed);
    cJSON_AddNumberToObject(json, "width", config->width);
    // Convert cJSON object to string
    char* string = cJSON_PrintUnformatted(json);
    if(string == NULL) {
        cJSON_Delete(json);
        return L401_ERR_INTERNAL;
    }

    // Assign the string to the output
    *jsontxt = string;
    // Clean up
    cJSON_Delete(json);

    return L401_OK;
}

/**
 * @brief Parses a JSON string and populates an Configuration structure.
 *
 * This function takes a JSON formatted string and parses it to populate
 * an Configuration structure. The function assumes the JSON string
 * is properly formatted and contains the necessary fields. The caller
 * is responsible for ensuring that the passed structure is properly initialized.
 *
 * @param jsontxt The JSON string to parse.
 * @param config Pointer to the Configuration structure where the parsed data will be stored.
 *
 * @return l401_err An enumerated error code.
 *         - L401_OK on successful parsing and data extraction.
 *         - L401_ERR_NULLPTR if any of the input pointers are NULL.
 *         - L401_ERR_PARSE if there is an error in parsing the JSON string.
 *         - L401_ERR_MALFORMED if the JSON string does not contain the required fields or if they are incorrectly
 * formatted.
 */
l401_err json_to_config(char* jsontxt, Configuration* config) {
    if(config == NULL || jsontxt == NULL) {
        return L401_ERR_NULLPTR;
    }

    cJSON* json = cJSON_Parse(jsontxt);
    if(json == NULL) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if(error_ptr != NULL) {
            FURI_LOG_E(TAG, "cJSON Parse error: Error before: %s\n", error_ptr);
        }
        return L401_ERR_PARSE;
    }

    cJSON* json_version = cJSON_GetObjectItemCaseSensitive(json, "version");
    cJSON* json_text = cJSON_GetObjectItemCaseSensitive(json, "text");
    cJSON* json_bitmapPath = cJSON_GetObjectItemCaseSensitive(json, "bitmapPath");
    cJSON* json_color = cJSON_GetObjectItemCaseSensitive(json, "color");
    cJSON* json_brightness = cJSON_GetObjectItemCaseSensitive(json, "brightness");
    cJSON* json_sensitivity = cJSON_GetObjectItemCaseSensitive(json, "sensitivity");
    cJSON* json_orientation = cJSON_GetObjectItemCaseSensitive(json, "orientation");
    // Add version 1.1 features
    cJSON* json_mirror = cJSON_GetObjectItemCaseSensitive(json, "mirror");
    cJSON* json_speed = cJSON_GetObjectItemCaseSensitive(json, "speed");
    cJSON* json_width = cJSON_GetObjectItemCaseSensitive(json, "width");

    if(!cJSON_IsString(json_version) || !cJSON_IsString(json_text) ||
       !cJSON_IsString(json_bitmapPath) || !cJSON_IsNumber(json_color) ||
       !cJSON_IsNumber(json_brightness) || !cJSON_IsNumber(json_sensitivity) ||
       !cJSON_IsBool(json_orientation) || !cJSON_IsBool(json_mirror) ||
       !cJSON_IsNumber(json_speed) || !cJSON_IsNumber(json_width)) {
        cJSON_Delete(json);
        FURI_LOG_E(TAG, "Error: Malformed configuration");
        return L401_ERR_MALFORMED;
    }

    free(config->version);

    config->version = strdup(json_version->valuestring);
    strncpy(config->text, json_text->valuestring, LIGHTMSG_MAX_TEXT_LEN);
    config->text[LIGHTMSG_MAX_TEXT_LEN] = '\0';
    strncpy(config->bitmapPath, json_bitmapPath->valuestring, LIGHTMSG_MAX_BITMAPPATH_LEN);
    config->bitmapPath[LIGHTMSG_MAX_BITMAPPATH_LEN] = '\0';
    // config->text = strdup(json_text->valuestring);
    config->color = (uint8_t)json_color->valuedouble;
    config->brightness = (uint8_t)json_brightness->valuedouble;
    config->sensitivity = (uint8_t)json_sensitivity->valuedouble;
    config->orientation = cJSON_IsTrue(json_orientation) ? true : false;
    config->mirror = cJSON_IsTrue(json_mirror) ? true : false;
    config->speed = (uint8_t)json_speed->valuedouble;
    config->width = (uint8_t)json_width->valuedouble;
    cJSON_Delete(json);
    return L401_OK;
}

l401_err config_save_json(const char* filename, Configuration* config) {
    furi_assert(filename);
    furi_assert(config);
    char* jsontxt = NULL;
    size_t bytes = 0;
    size_t jsontxt_len = 0;
    l401_err res = config_to_json(config, &jsontxt);
    if(res != L401_OK) {
        FURI_LOG_E(TAG, "Error while converting conf to json: %d", (uint8_t)res);
        return res;
    }

    jsontxt_len = strlen(jsontxt);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* configuration_file = storage_file_alloc(storage);
    if(!storage_file_open(configuration_file, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(
            TAG,
            "Write failed \"%s\". Error: \'%s\'",
            filename,
            storage_file_get_error_desc(configuration_file));
        res = L401_ERR_FILESYSTEM;
        goto cleanup;
    } else {
        bytes = storage_file_write(configuration_file, jsontxt, jsontxt_len);
        res = L401_OK;
    }

    if(bytes != jsontxt_len) {
        FURI_LOG_E(
            TAG,
            "Write failed \"%s\". Error: \'%s\'",
            filename,
            storage_file_get_error_desc(configuration_file));
        res = L401_ERR_FILESYSTEM;
        goto cleanup;
    }

cleanup:
    storage_file_close(configuration_file);
    storage_file_free(configuration_file);
    free(jsontxt);
    furi_record_close(RECORD_STORAGE);
    return res;
}

l401_err config_read_json(const char* filename, Configuration* config) {
    furi_assert(filename);
    furi_assert(config);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    char* jsontxt = NULL;
    size_t bytes = 0;

    File* configuration_file = storage_file_alloc(storage);
    if(storage_file_open(configuration_file, filename, FSAM_READ, FSOM_OPEN_ALWAYS)) {
        size_t configuration_file_size = storage_file_size(configuration_file);
        jsontxt = (char*)malloc(configuration_file_size + 1);
        bytes = storage_file_read(configuration_file, jsontxt, configuration_file_size);
        jsontxt[bytes] = '\0';
    }

    l401_err res = json_to_config(jsontxt, config);
    if(res != L401_OK) {
        FURI_LOG_E(TAG, "Error while getting JSON from %s. %d", filename, res);
    }

    storage_file_close(configuration_file);
    free(jsontxt);
    furi_record_close(RECORD_STORAGE);
    return res;
}

l401_err config_init_dir(const char* filename) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* configuration_dirname = furi_string_alloc();
    path_extract_dirname(filename, configuration_dirname);
    if(storage_common_stat(storage, furi_string_get_cstr(configuration_dirname), NULL) ==
       FSE_NOT_EXIST) {
        FURI_LOG_I(TAG, "Directory doesn't exist. Will create new.");
        if(!storage_simply_mkdir(storage, furi_string_get_cstr(configuration_dirname))) {
            FURI_LOG_E(
                TAG, "Error creating directory %s", furi_string_get_cstr(configuration_dirname));
            return L401_ERR_FILESYSTEM;
        }
    }

    furi_string_free(configuration_dirname);
    furi_record_close(RECORD_STORAGE);
    return L401_OK;
}

l401_err config_create_json(const char* filename, Configuration* config) {
    furi_assert(filename);
    furi_assert(config);
    config_default_init(config);
    FURI_LOG_I(TAG, "Save JSON to %s", filename);
    l401_err res = config_save_json(filename, config);
    if(res != L401_OK) {
        FURI_LOG_E(TAG, "Error while saving default configuration to %s: %d", filename, res);
        return res;
    }
    return L401_OK;
}

l401_err config_load_json(const char* filename, Configuration* config) {
    furi_assert(filename);
    furi_assert(config);
    l401_err res = config_init_dir(filename);
    if(res != L401_OK) {
        FURI_LOG_E(TAG, "Error while loading from %s: %d", filename, res);
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    // Check if configuration file exists
    if(storage_common_stat(storage, filename, NULL) != FSE_OK) {
        // Create it if it doesn't exists
        if(config_create_json(filename, config) != L401_OK) {
            FURI_LOG_E(TAG, "Could not create configuration file %s", filename);
            return L401_ERR_INTERNAL;
        }
    }
    // Try to load the configuration
    res = config_read_json(filename, config);
    if(res != L401_OK) {
        // If it can't load it, remove the old version.
        if(storage_common_remove(storage, LIGHTMSGCONF_CONFIG_FILE) != FSE_OK) {
            FURI_LOG_E(TAG, "Could not remove old configuration file.");
            // If it fails, it's bad.
            return L401_ERR_FILESYSTEM;
        }
        // Recreate a fresh new config file.
        if(config_create_json(filename, config) != L401_OK) {
            // If it doesn't work, it's bad too.
            FURI_LOG_E(TAG, "Could not get configuration from %s: %d", filename, res);
            return L401_ERR_INTERNAL;
        }
        // Try to reload the configuration from the fresh new one.
        res = config_read_json(filename, config);
        furi_record_close(RECORD_STORAGE);
        if(res != L401_OK) {
            FURI_LOG_E(TAG, "Could not reset configuration file.... Aborting");
        }
    }
    furi_record_close(RECORD_STORAGE);
    return res;
}
