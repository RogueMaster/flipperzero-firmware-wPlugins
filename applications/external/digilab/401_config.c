/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401_config.h"
#include "401DigiLab_config.h"

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
    config->version = strdup(DIGILAB_VERSION); // Default version
    config->ScopeSound = DIGILAB_DEFAULT_SCOPE_SOUND;
    config->ScopeVibro = DIGILAB_DEFAULT_SCOPE_VIBRO;
    config->ScopeLed = DIGILAB_DEFAULT_SCOPE_LED;
    config->ScopeAlert = DIGILAB_DEFAULT_SCOPE_ALERT;
    config->BridgeFactor = DIGILAB_DEFAULT_SCOPE_BRIDGEFACTOR;
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

    FURI_LOG_I(TAG, "[config_to_json] cJSON add");
    // Add items to the cJSON object
    cJSON_AddStringToObject(json, "version", config->version);
    cJSON_AddNumberToObject(json, "ScopeSound", config->ScopeSound);
    cJSON_AddNumberToObject(json, "ScopeVibro", config->ScopeVibro);
    cJSON_AddNumberToObject(json, "ScopeLed", config->ScopeLed);
    cJSON_AddNumberToObject(json, "ScopeAlert", config->ScopeAlert);
    cJSON_AddNumberToObject(json, "BridgeFactor", config->BridgeFactor);

    FURI_LOG_I(TAG, "[config_to_json] cJSON_Print");

    // Convert cJSON object to string
    char* string = cJSON_PrintUnformatted(json);
    if(string == NULL) {
        cJSON_Delete(json);
        return L401_ERR_INTERNAL;
    }

    // Assign the string to the output
    *jsontxt = string;
    FURI_LOG_I(TAG, "[config_to_json] cJSON_Delete");
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
    cJSON* json_ScopeSound = cJSON_GetObjectItemCaseSensitive(json, "ScopeSound");
    cJSON* json_ScopeVibro = cJSON_GetObjectItemCaseSensitive(json, "ScopeVibro");
    cJSON* json_ScopeLed = cJSON_GetObjectItemCaseSensitive(json, "ScopeLed");
    cJSON* json_ScopeAlert = cJSON_GetObjectItemCaseSensitive(json, "ScopeAlert");
    cJSON* json_BridgeFactor = cJSON_GetObjectItemCaseSensitive(json, "BridgeFactor");
    if(!cJSON_IsString(json_version) || !cJSON_IsNumber(json_ScopeSound) ||
       !cJSON_IsNumber(json_ScopeVibro) || !cJSON_IsNumber(json_ScopeLed) ||
       !cJSON_IsNumber(json_ScopeAlert) || !cJSON_IsNumber(json_BridgeFactor)) {
        cJSON_Delete(json);
        FURI_LOG_E(TAG, "Error: Malformed configuration");
        return L401_ERR_MALFORMED;
    }

    // Free existing memory if any
    free(config->version);

    config->version = strdup(json_version->valuestring);
    config->ScopeSound = (uint8_t)json_ScopeSound->valuedouble;
    config->ScopeVibro = (uint8_t)json_ScopeVibro->valuedouble;
    config->ScopeLed = (uint8_t)json_ScopeLed->valuedouble;
    config->ScopeAlert = (uint8_t)json_ScopeAlert->valuedouble;
    config->BridgeFactor = json_BridgeFactor->valuedouble;

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
    FURI_LOG_I(TAG, "[config_save_json] Cleanup");
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
    config_reset_calibration();
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
        if(storage_common_remove(storage, DIGILABCONF_CONFIG_FILE) != FSE_OK) {
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

/**
 * @brief Reset the calibration by deleting the calibration file.
 *
 * @return L401_OK                  Calibration file successfully removed.
 * @return L401_ERR_FILE_DOESNT_EXISTS
 *                                  Calibration file was not found.
 * @return L401_ERR_FILESYSTEM      Failed to open storage or remove file.
 */
l401_err config_reset_calibration(void) {
    l401_err err = L401_OK;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "COuld not open storage.");
        return L401_ERR_FILESYSTEM;
    }

    // If it doesn't exist, return an error.
    if(storage_common_stat(storage, DIGILABCONF_CALIBRATION_FILE, NULL) != FSE_OK) {
        err = L401_ERR_FILE_DOESNT_EXISTS;
    }
    // If it does, we remove it.
    else if(storage_common_remove(storage, DIGILABCONF_CALIBRATION_FILE) != FSE_OK) {
        FURI_LOG_E(TAG, "COuld not delete file \"%s\".", DIGILABCONF_CALIBRATION_FILE);
        err = L401_ERR_FILESYSTEM;
    }

    furi_record_close(RECORD_STORAGE);
    return err;
}

/**
 * @brief Ensure the calibration file exists, creating it if necessary.
 *
 *
 * @return L401_OK             Calibration file already existed or was created successfully.
 * @return L401_ERR_FILESYSTEM Failed to open storage, create the file, or write data.
 */
l401_err config_set_calibration(void) {
    l401_err err = L401_OK;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        return L401_ERR_FILESYSTEM;
    }

    // If the file already exists, we do nothing
    if(storage_common_stat(storage, DIGILABCONF_CALIBRATION_FILE, NULL) == FSE_OK) {
        goto cleanup;
    }

    // Create the calibration file.
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, DIGILABCONF_CALIBRATION_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(
            TAG,
            "Write failed \"%s\" : %s",
            DIGILABCONF_CALIBRATION_FILE,
            storage_file_get_error_desc(file));
        err = L401_ERR_FILESYSTEM;
        storage_file_free(file);
        goto cleanup;
    }

    if(storage_file_write(file, "CAL", 3) != 3) {
        FURI_LOG_E(TAG, "Could'nt write into file \"%s\".", DIGILABCONF_CALIBRATION_FILE);
        err = L401_ERR_FILESYSTEM;
    }

    storage_file_close(file);
    storage_file_free(file);

cleanup:
    furi_record_close(RECORD_STORAGE);
    return err;
}

/**
 * @brief Check if the calibration file exists.
 *
 * @return L401_OK                   Calibration file exists.
 * @return L401_ERR_FILE_DOESNT_EXISTS
 *                                   Calibration file was not found.
 * @return L401_ERR_FILESYSTEM       Failed to open storage.
 */
l401_err config_check_calibration(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        return L401_ERR_FILESYSTEM;
    }

    l401_err err;
    if(storage_common_stat(storage, DIGILABCONF_CALIBRATION_FILE, NULL) != FSE_OK) {
        err = L401_ERR_FILE_DOESNT_EXISTS;
    } else {
        err = L401_OK;
    }

    furi_record_close(RECORD_STORAGE);
    return err;
}
