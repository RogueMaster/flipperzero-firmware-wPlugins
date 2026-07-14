#include "csv_get_url.h"
#include "../csv_utils/csv_utils.h"
#include <furi.h>
#include <stdlib.h>
#include <storage/storage.h>
#include "../../app.h"

bool sync_csv_get_url_to_mem(App* app) {
    FURI_LOG_T(TAG, "Syncing CSV with flipper memory");
    FuriString* buffer = furi_string_alloc();

    if(!storage_file_open(app->file, APP_DATA_PATH("get_url.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file");
        furi_string_free(buffer);
        return false;
    }

    // First, count the number of lines in the CSV file
    size_t line_count = 0;
    while(read_line_from_file(app->file, buffer)) {
        line_count++;
    }

    // Close and reopen the file to reset the file pointer
    storage_file_close(app->file);
    if(!storage_file_open(app->file, APP_DATA_PATH("get_url.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to reopen file");
        furi_string_free(buffer);
        return false;
    }

    // Allocate memory for the url_list based on the number of lines
    free(app->url_list); // Free the minimal memory allocated during init
    if(line_count > 0) {
        app->url_list = malloc(line_count * sizeof(UrlList));
        if(!app->url_list) {
            FURI_LOG_E(TAG, "Failed to allocate memory for url_list");
            furi_string_free(buffer);
            storage_file_close(app->file);
            return false;
        }
    } else {
        app->url_list = NULL;
    }
    app->url_list_count = line_count;

    // Read and parse each line into the url_list
    size_t index = 0;
    while(index < line_count && read_line_from_file(app->file, buffer)) {
        const char* buffer_str = furi_string_get_cstr(buffer);
        strncpy(app->url_list[index].url, buffer_str, TEXT_STORE_SIZE - 1);
        app->url_list[index].url[TEXT_STORE_SIZE - 1] = '\0';
        index++;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);
    FURI_LOG_T(TAG, "Done with syncing CSV with flipper memory");
    return true;
}

bool write_url_to_csv(App* app, const char* url) {
    FURI_LOG_T(TAG, "Writing URL to CSV");
    if(!storage_file_open(app->file, APP_DATA_PATH("get_url.csv"), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FURI_LOG_E(TAG, "Failed to open file for writing");
        return false;
    }

    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "%s\n", url);

    if(!storage_file_write(app->file, furi_string_get_cstr(buffer), furi_string_size(buffer))) {
        FURI_LOG_E(TAG, "Failed to write URL to file");
        furi_string_free(buffer);
        storage_file_close(app->file);
        return false;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);
    FURI_LOG_T(TAG, "Done with writing URL to CSV");
    return true;
}

bool read_urls_from_csv(App* app) {
    FURI_LOG_T(TAG, "Reading URLs from CSV");

    FuriString* buffer = furi_string_alloc();
    if(!storage_file_open(app->file, APP_DATA_PATH("get_url.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file");
        furi_string_free(buffer);
        return false;
    }

    // First, count the number of lines in the CSV file
    size_t line_count = 0;
    while(read_line_from_file(app->file, buffer)) {
        line_count++;
    }

    // Close and reopen the file to reset the file pointer
    storage_file_close(app->file);
    if(!storage_file_open(app->file, APP_DATA_PATH("get_url.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to reopen file");
        furi_string_free(buffer);
        return false;
    }

    // Allocate memory for the url_list based on the number of lines
    free(app->url_list); // Free the minimal memory allocated during init
    if(line_count > 0) {
        app->url_list = malloc(line_count * sizeof(UrlList));
        if(!app->url_list) {
            FURI_LOG_E(TAG, "Failed to allocate memory for url_list");
            furi_string_free(buffer);
            storage_file_close(app->file);
            return false;
        }
    } else {
        app->url_list = NULL;
    }
    app->url_list_count = line_count;

    // Read and parse each line into the url_list
    size_t index = 0;
    while(index < line_count && read_line_from_file(app->file, buffer)) {
        const char* buffer_str = furi_string_get_cstr(buffer);
        strncpy(app->url_list[index].url, buffer_str, TEXT_STORE_SIZE - 1);
        app->url_list[index].url[TEXT_STORE_SIZE - 1] = '\0';
        index++;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);

    FURI_LOG_T(TAG, "Done with reading URLs from CSV");
    return true;
}

bool delete_url_from_csv(App* app, const char* url) {
    FURI_LOG_T(TAG, "Deleting URL from CSV");
    FuriString* buffer_url = furi_string_alloc();
    FuriString* buffer_new_content = furi_string_alloc();
    bool url_found = false;

    if(!storage_file_open(app->file, APP_DATA_PATH("get_url.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file for reading");
        furi_string_free(buffer_url);
        furi_string_free(buffer_new_content);
        return false;
    }

    while(read_line_from_file(app->file, buffer_url)) {
        const char* buffer_str = furi_string_get_cstr(buffer_url);

        if(strcmp(buffer_str, url) != 0) {
            furi_string_cat(buffer_new_content, buffer_str);
            furi_string_cat(buffer_new_content, "\n");
        } else {
            url_found = true;
        }
    }

    storage_file_close(app->file);

    if(url_found) {
        if(!storage_file_open(
               app->file, APP_DATA_PATH("get_url.csv"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to open file for writing");
            furi_string_free(buffer_url);
            furi_string_free(buffer_new_content);
            storage_file_close(app->file);
            return false;
        }

        // If the new content is empty, just close the file and return
        if(furi_string_size(buffer_new_content) == 0) {
            storage_file_close(app->file);
            return true;
        }

        if(!storage_file_write(
               app->file,
               furi_string_get_cstr(buffer_new_content),
               furi_string_size(buffer_new_content))) {
            FURI_LOG_E(TAG, "Failed to write new content to file");
            furi_string_free(buffer_url);
            furi_string_free(buffer_new_content);
            storage_file_close(app->file);
            return false;
        }

        storage_file_close(app->file);
    }

    furi_string_free(buffer_url);
    furi_string_free(buffer_new_content);

    FURI_LOG_T(TAG, "Done with deleting URL from CSV");
    return url_found;
}

bool init_csv_get_url(App* app) {
    FURI_LOG_T(TAG, "Initializing CSV for get urls");

    if(!app->file) {
        FURI_LOG_E(TAG, "Failed to allocate storage file");
        return false;
    }

    if(storage_file_exists(app->storage, APP_DATA_PATH("get_url.csv"))) {
        FURI_LOG_T(TAG, "URL CSV file exists");

        if(!sync_csv_get_url_to_mem(app)) {
            FURI_LOG_E(TAG, "Failed to sync CSV with flipper memory");
            storage_file_close(app->file);
            return false;
        }

    } else {
        FURI_LOG_T(TAG, "URL CSV file does not exist, creating a new one");

        if(!storage_file_open(
               app->file, APP_DATA_PATH("get_url.csv"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to create CSV file");
            storage_file_close(app->file);
            return false;
        }
        storage_file_close(app->file);

        // Initialize url_list as empty
        app->url_list = NULL;
        app->url_list_count = 0;
    }

    FURI_LOG_T(TAG, "Done with initializing CSV for get urls");
    return true;
}
