#include "./csv_build_url.h"
#include "../../app.h"
#include "../../structs.h"
#include "../csv_utils/csv_utils.h"
#include <furi.h>
#include <stdlib.h>
#include <storage/storage.h>

#define BUILD_HTTP_CSV_PATH APP_DATA_PATH("build_http.csv")
#define HEADER_DELIMITER    "@"
#define HEADER_KV_DELIMITER ":"
#define NO_HEADERS          "NO_HEADERS"
#define NO_PAYLOAD          "NO_PAYLOAD"
#define EMPTY_VALUE         "EMPTY_VALUE"
#define MAX_BUFFER_SIZE     128
#define SMALL_BUFFER_SIZE   64

size_t count_headers(const char* headers_str) {
    if(!headers_str || strlen(headers_str) == 0) {
        return 0;
    }

    size_t count = 0;
    const char* ptr = headers_str;

    while(ptr && *ptr) {
        ptr = strchr(ptr, '@');
        if(ptr) {
            count++;
            ptr++;
        }
    }
    // Add one for the last header if string doesn't end with '@'
    if(headers_str[strlen(headers_str) - 1] != '@') {
        count++;
    }
    return count;
}

// Helper function to parse headers
// Helper function to parse headers
void string_to_headers_c(const char* str, HttpBuildHeader** headers, size_t* headers_count) {
    if(!str || !headers || !headers_count) {
        return;
    }

    // Count the number of headers
    size_t count = 0;
    const char* ptr = str;
    while(ptr && *ptr) {
        if(*ptr == '@') {
            count++;
        }
        ptr++;
    }
    // Add one more if the string is not empty
    if(strlen(str) > 0) {
        count++;
    }

    // Allocate memory for headers
    *headers = malloc(count * sizeof(HttpBuildHeader));
    if(!*headers) {
        *headers_count = 0;
        return;
    }
    memset(*headers, 0, count * sizeof(HttpBuildHeader));

    // Parse headers
    size_t index = 0;
    const char* start = str;
    while(index < count && start && *start) {
        const char* end = strchr(start, '@');
        size_t len = end ? (size_t)(end - start) : strlen(start);

        // Find key-value separator
        const char* sep = strchr(start, ':');
        if(!sep || sep >= start + len) {
            // Invalid header format, skip this header
            free(*headers);
            *headers = NULL;
            *headers_count = 0;
            return;
        }

        size_t key_len = (size_t)(sep - start);
        size_t value_len = len - key_len - 1;

        if(key_len >= TEXT_STORE_SIZE || value_len >= TEXT_STORE_SIZE) {
            // Key or value is too long
            free(*headers);
            *headers = NULL;
            *headers_count = 0;
            return;
        }

        // Copy key
        strncpy((*headers)[index].key, start, key_len);
        (*headers)[index].key[key_len] = '\0';

        // Copy value
        strncpy((*headers)[index].value, sep + 1, value_len);
        (*headers)[index].value[value_len] = '\0';

        start = end ? end + 1 : NULL;
        index++;
    }

    *headers_count = index;
}

// The line will look like this:
// url,mode,http_method,headers,payload,show_response_headers
// Example:
// https://www.example.com,1,1,Content-Type:application/json@Authorization:Bearer token,Hello,1
// Example with no headers:
// https://www.example.com,1,1,NO_HEADERS,Hello,1
// Example with no payload:
// https://www.example.com,1,1,Content-Type:application/json@Authorization:Bearer,NO_PAYLOAD,1
// Example with no headers and no payload:
// https://www.example.com,1,1,NO_HEADERS,NO_PAYLOAD,1

bool parse_csv_line(FuriString* buffer, BuildHttpList* item) {
    if(!buffer || !item) {
        return false;
    }

    FuriString* token = furi_string_alloc();
    if(!token) {
        return false;
    }
    size_t pos = 0;
    bool success = false;

    // Parse URL
    pos = furi_string_search(buffer, ",", 0);
    if(pos == FURI_STRING_FAILURE) goto cleanup;
    furi_string_set(token, buffer);
    furi_string_left(token, pos);
    strncpy(item->url, furi_string_get_cstr(token), TEXT_STORE_SIZE - 1);
    item->url[TEXT_STORE_SIZE - 1] = '\0';
    furi_string_right(buffer, pos + 1);

    // Parse mode
    pos = furi_string_search(buffer, ",", 0);
    if(pos == FURI_STRING_FAILURE) goto cleanup;
    furi_string_set(token, buffer);
    furi_string_left(token, pos);
    item->mode = atoi(furi_string_get_cstr(token));
    furi_string_right(buffer, pos + 1);

    // Parse http_method
    pos = furi_string_search(buffer, ",", 0);
    if(pos == FURI_STRING_FAILURE) goto cleanup;
    furi_string_set(token, buffer);
    furi_string_left(token, pos);
    item->http_method = (HttpBuildMethod)atoi(furi_string_get_cstr(token));
    furi_string_right(buffer, pos + 1);

    // Parse headers
    pos = furi_string_search(buffer, ",", 0);
    if(pos == FURI_STRING_FAILURE) goto cleanup;
    furi_string_set(token, buffer);
    furi_string_left(token, pos);
    const char* headers_str = furi_string_get_cstr(token);
    if(strcmp(headers_str, NO_HEADERS) != 0 && strlen(headers_str) > 0) {
        // Use string_to_headers_c to parse headers
        string_to_headers_c(headers_str, &item->headers, &item->headers_count);
        if(!item->headers) {
            FURI_LOG_E(TAG, "Failed to parse headers");
            goto cleanup;
        }
    } else {
        item->headers = malloc(sizeof(HttpBuildHeader)); // Allocate a small amount of memory
        item->headers_count = 0;
    }
    furi_string_right(buffer, pos + 1);

    // Parse payload
    pos = furi_string_search(buffer, ",", 0);
    if(pos == FURI_STRING_FAILURE) goto cleanup;
    furi_string_set(token, buffer);
    furi_string_left(token, pos);
    if(strcmp(furi_string_get_cstr(token), NO_PAYLOAD) != 0) {
        item->payload = furi_string_alloc_set(token);
        if(!item->payload) {
            FURI_LOG_E(TAG, "Failed to allocate memory for payload");
            goto cleanup;
        }
    } else {
        item->payload = furi_string_alloc(); // Allocate an empty FuriString
    }
    furi_string_right(buffer, pos + 1);

    // Parse show_response_headers
    furi_string_set(token, buffer);
    item->show_response_headers = atoi(furi_string_get_cstr(token));

    success = true;

cleanup:
    if(!success) {
        if(item->headers) {
            free(item->headers);
            item->headers = NULL;
            item->headers_count = 0;
        }
        if(item->payload) {
            furi_string_free(item->payload);
            item->payload = NULL;
        }
    }
    furi_string_free(token);
    return success;
}

bool write_build_http_to_csv(App* app, const BuildHttpList* item) {
    if(!storage_file_open(app->file, BUILD_HTTP_CSV_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        return false;
    }

    // Prepare the entire line in memory
    FuriString* line = furi_string_alloc();
    furi_string_printf(line, "%s,%d,%d,", item->url, item->mode, item->http_method);

    // Append headers
    if(item->headers && item->headers_count > 0) {
        for(size_t i = 0; i < item->headers_count; i++) {
            furi_string_cat_printf(
                line,
                "%s:%s%s",
                item->headers[i].key,
                item->headers[i].value[0] ? item->headers[i].value : EMPTY_VALUE,
                (i < item->headers_count - 1) ? "@" : "");
        }
    } else {
        furi_string_cat_str(line, NO_HEADERS);
    }
    furi_string_cat_str(line, ",");

    // Append payload
    if(item->payload && furi_string_size(item->payload) > 0) {
        furi_string_cat(line, item->payload);
    } else {
        furi_string_cat_str(line, NO_PAYLOAD);
    }

    // Append final fields
    furi_string_cat_printf(line, ",%d\n", item->show_response_headers);

    FURI_LOG_D(TAG, "Writing line: %s", furi_string_get_cstr(line));
    // Write the entire line at once
    storage_file_write(app->file, furi_string_get_cstr(line), furi_string_size(line));

    furi_string_free(line);
    storage_file_close(app->file);
    return true;
}

bool sync_csv_build_http_to_mem(App* app) {
    if(!app || !app->file) {
        FURI_LOG_E(TAG, "Invalid App structure or uninitialized file");
        return false;
    }

    FuriString* buffer = furi_string_alloc();
    if(!buffer) {
        FURI_LOG_E(TAG, "Failed to allocate buffer");
        return false;
    }

    // Open the CSV file for reading
    if(!storage_file_open(app->file, BUILD_HTTP_CSV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open CSV file");
        furi_string_free(buffer);
        return false;
    }

    // Count the number of lines in the CSV file
    size_t line_count = 0;
    while(read_line_from_file(app->file, buffer)) {
        line_count++;
    }

    // Reset the file pointer to the beginning
    storage_file_close(app->file);
    if(!storage_file_open(app->file, BUILD_HTTP_CSV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to reopen CSV file");
        furi_string_free(buffer);
        return false;
    }

    // Free any existing build_http_list
    free_build_http_list(app);

    // Handle the case where there are no entries
    if(line_count == 0) {
        app->build_http_list = NULL;
        app->build_http_list_size = 0;
        furi_string_free(buffer);
        storage_file_close(app->file);
        return true;
    }

    // Allocate memory for the build_http_list
    app->build_http_list = malloc(line_count * sizeof(BuildHttpList));
    if(!app->build_http_list) {
        FURI_LOG_E(TAG, "Failed to allocate memory for build_http_list");
        furi_string_free(buffer);
        storage_file_close(app->file);
        return false;
    }
    app->build_http_list_size = line_count;

    // Read and parse each line into the build_http_list
    size_t index = 0;
    while(index < line_count && read_line_from_file(app->file, buffer)) {
        BuildHttpList* item = &app->build_http_list[index];
        memset(item, 0, sizeof(BuildHttpList));

        // Initialize item pointers
        item->headers = NULL;
        item->headers_count = 0;
        item->payload = NULL;

        // Parse the CSV line
        if(!parse_csv_line(buffer, item)) {
            FURI_LOG_E(TAG, "Failed to parse CSV line at index %zu", index);
            // Clean up allocated memory
            app->build_http_list_size = index;
            free_build_http_list(app);
            furi_string_free(buffer);
            storage_file_close(app->file);
            return false;
        }

        index++;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);
    return true;
}

bool init_csv_build_http(App* app) {
    if(!app || !app->file || !app->storage) {
        FURI_LOG_E(TAG, "Invalid App structure or uninitialized file/storage");
        return false;
    }

    // Check if the CSV file exists
    if(storage_file_exists(app->storage, BUILD_HTTP_CSV_PATH)) {
        FURI_LOG_I(TAG, "BUILD CSV file exists, syncing data");
        if(!sync_csv_build_http_to_mem(app)) {
            FURI_LOG_E(TAG, "Failed to sync CSV data");
            return false;
        }
    } else {
        // Create the CSV file
        if(!storage_file_open(app->file, BUILD_HTTP_CSV_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to create CSV file");
            return false;
        }
        storage_file_close(app->file);

        // Initialize build_http_list as empty
        app->build_http_list = NULL;
        app->build_http_list_size = 0;
    }

    return true;
}

bool delete_build_http_from_csv(App* app, const char* url) {
    FURI_LOG_T(TAG, "Deleting Build HTTP from CSV");

    if(!app || !url) return false;

    FuriString* buffer = furi_string_alloc();
    FuriString* new_content = furi_string_alloc();
    bool url_found = false;

    // Open file for reading
    if(!storage_file_open(app->file, BUILD_HTTP_CSV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open Build HTTP file for reading");
        furi_string_free(buffer);
        furi_string_free(new_content);
        return false;
    }

    // Read line by line and skip matching URL
    while(read_line_from_file(app->file, buffer)) {
        // Check if line starts with the URL we want to delete
        if(strncmp(furi_string_get_cstr(buffer), url, strlen(url)) != 0) {
            furi_string_cat(new_content, buffer);
            furi_string_cat_str(new_content, "\n");
        } else {
            url_found = true;
        }
    }

    storage_file_close(app->file);

    if(url_found) {
        // Rewrite file with filtered content
        if(!storage_file_open(app->file, BUILD_HTTP_CSV_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to open Build HTTP file for writing");
            furi_string_free(buffer);
            furi_string_free(new_content);
            return false;
        }

        if(furi_string_size(new_content) > 0) {
            if(!storage_file_write(
                   app->file, furi_string_get_cstr(new_content), furi_string_size(new_content))) {
                FURI_LOG_E(TAG, "Failed to write filtered content");
                storage_file_close(app->file);
                furi_string_free(buffer);
                furi_string_free(new_content);
                return false;
            }
        }

        storage_file_close(app->file);
    }

    furi_string_free(buffer);
    furi_string_free(new_content);

    FURI_LOG_T(TAG, "Done with deleting Build HTTP from CSV");
    return url_found;
}

void free_build_http_list(App* app) {
    if(app && app->build_http_list) {
        for(size_t i = 0; i < app->build_http_list_size; i++) {
            BuildHttpList* item = &app->build_http_list[i];
            if(item->payload) {
                furi_string_free(item->payload);
                item->payload = NULL;
            }
            if(item->headers) {
                free(item->headers);
                item->headers = NULL;
            }
        }
        free(app->build_http_list);
        app->build_http_list = NULL;
        app->build_http_list_size = 0;
    }
}
