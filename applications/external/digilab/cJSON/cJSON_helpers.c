#include <cJSON/cJSON_helpers.h>

l401_err json_read(const char* filename, cJSON** json) {
    furi_assert(filename);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    char* jsontxt = NULL;
    size_t bytes = 0;

    File* jsonfile = storage_file_alloc(storage);
    if(storage_file_open(jsonfile, filename, FSAM_READ, FSOM_OPEN_ALWAYS)) {
        size_t jsonfile_size = storage_file_size(jsonfile);
        jsontxt = (char*)malloc(jsonfile_size + 1);
        bytes = storage_file_read(jsonfile, jsontxt, jsonfile_size);
        jsontxt[bytes] = '\0';
        *json = cJSON_Parse(jsontxt);
        if(json == NULL) {
            const char* error_ptr = cJSON_GetErrorPtr();
            if(error_ptr != NULL) {
                FURI_LOG_E(__FUNCTION__, "cJSON Parse error: Error before: %s\n", error_ptr);
            }
            return L401_ERR_PARSE;
        }
        storage_file_close(jsonfile);
        furi_record_close(RECORD_STORAGE);
        free(jsontxt);
        return L401_OK;
    }

    furi_record_close(RECORD_STORAGE);
    return L401_ERR_FILESYSTEM;
}

l401_err json_read_hex_array(cJSON* json_array, uint8_t** dst, size_t* len) {
    // Validate input
    if(!cJSON_IsArray(json_array) || dst == NULL || len == NULL) {
        FURI_LOG_E(__FUNCTION__, "Invalid input to json_read_hex_array");
        return L401_ERR_NULLPTR;
    }

    // Get the size of the JSON array
    size_t array_size = cJSON_GetArraySize(json_array);
    if(array_size == 0) {
        FURI_LOG_E(__FUNCTION__, "JSON array is empty");
        return L401_ERR_MALFORMED;
    }

    // Allocate memory for the output array directly into *dst
    *dst = (uint8_t*)calloc(array_size, sizeof(uint8_t));
    if(*dst == NULL) {
        FURI_LOG_E(__FUNCTION__, "Memory allocation failed");
        return L401_ERR_NULLPTR;
    }

    // Parse each element in the JSON array and write directly to *dst
    for(size_t i = 0; i < array_size; i++) {
        cJSON* item = cJSON_GetArrayItem(json_array, i);
        if(!cJSON_IsString(item) || item->valuestring == NULL || strlen(item->valuestring) != 2) {
            FURI_LOG_E(__FUNCTION__, "Invalid hex string at index %zu", i);
            free(*dst); // Free memory in case of failure
            *dst = NULL;
            return L401_ERR_MALFORMED;
        }

        // Convert hex string to uint8_t
        char* endptr = NULL;
        uint8_t value = (uint8_t)strtol(item->valuestring, &endptr, 16);
        if(*endptr != '\0') {
            FURI_LOG_E(__FUNCTION__, "Invalid hex value at index %zu: %s", i, item->valuestring);
            free(*dst); // Free memory in case of failure
            *dst = NULL;
            return L401_ERR_MALFORMED;
        }

        FURI_LOG_W(__FUNCTION__, "Expected: %s, Converted: 0x%02X", item->valuestring, value);
        (*dst)[i] = value; // Write directly to *dst
    }

    // Set length
    *len = array_size;

    return L401_OK;
}
