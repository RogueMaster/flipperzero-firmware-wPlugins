#include "./csv_post_url.h"
#include "../../app.h"
#include "../../structs.h"
#include "../csv_utils/csv_utils.h"
#include <furi.h>
#include <stdlib.h>
#include <storage/storage.h>

#define POST_URL_CSV_PATH APP_DATA_PATH("post_url.csv")

bool init_csv_post_url(App* app) {
    FURI_LOG_T(TAG, "Initializing POST URL CSV");

    if(!app->file) {
        FURI_LOG_E(TAG, "Failed to allocate storage file");
        return false;
    }

    if(storage_file_exists(app->storage, POST_URL_CSV_PATH)) {
        FURI_LOG_I(TAG, "POST URL CSV file exists, reading URLs from file");

        if(!sync_csv_post_url_to_mem(app)) {
            FURI_LOG_E(TAG, "Failed to read POST URLs from CSV file");
            storage_file_close(app->file);
            return false;
        }
    } else {
        FURI_LOG_I(TAG, "POST URL CSV file does not exist, creating a new one.");
        if(!storage_file_open(app->file, POST_URL_CSV_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to create POST URL CSV file");
            storage_file_close(app->file);
            return false;
        }
        storage_file_close(app->file);

        // Initialize post_url_list as empty
        app->post_url_list = NULL;
        app->post_url_list_count = 0;
    }
    FURI_LOG_T(TAG, "Done with initializing POST URL CSV");
    return true;
}

bool sync_csv_post_url_to_mem(App* app) {
    FURI_LOG_T(TAG, "Syncing POST URL CSV with flipper memory");
    FuriString* buffer = furi_string_alloc();

    if(!storage_file_open(app->file, POST_URL_CSV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open POST URL file");
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
    if(!storage_file_open(app->file, POST_URL_CSV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to reopen POST URL file");
        furi_string_free(buffer);
        return false;
    }

    // Allocate memory for the post_url_list based on the number of lines
    free(app->post_url_list); // Free the minimal memory allocated during init
    if(line_count > 0) {
        app->post_url_list = malloc(line_count * sizeof(PostUrlList));
        if(!app->post_url_list) {
            FURI_LOG_E(TAG, "Failed to allocate memory for post_url_list");
            furi_string_free(buffer);
            storage_file_close(app->file);
            return false;
        }
    } else {
        app->post_url_list = NULL;
    }
    app->post_url_list_count = line_count;

    // Read and parse each line into the post_url_list
    size_t index = 0;
    while(index < line_count && read_line_from_file(app->file, buffer)) {
        const char* buffer_str = furi_string_get_cstr(buffer);
        char payload_str[TEXT_STORE_SIZE] = {0};
        sscanf(buffer_str, "%[^,],%[^\n]", app->post_url_list[index].url, payload_str);
        app->post_url_list[index].payload = furi_string_alloc_set(payload_str);
        index++;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);
    FURI_LOG_T(TAG, "Done with syncing POST URL CSV with flipper memory");
    return true;
}

bool write_post_url_to_csv(App* app, const PostUrlList* post_url) {
    FURI_LOG_T(TAG, "Writing POST URL to CSV");
    if(!storage_file_open(app->file, POST_URL_CSV_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FURI_LOG_E(TAG, "Failed to open POST URL file for writing");
        return false;
    }

    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "%s,%s\n", post_url->url, furi_string_get_cstr(post_url->payload));

    if(!storage_file_write(app->file, furi_string_get_cstr(buffer), furi_string_size(buffer))) {
        FURI_LOG_E(TAG, "Failed to write POST URL to file");
        furi_string_free(buffer);
        storage_file_close(app->file);
        return false;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);
    FURI_LOG_T(TAG, "Done with writing POST URL to CSV");
    return true;
}

bool delete_post_url_from_csv(App* app, const char* url) {
    FURI_LOG_T(TAG, "Deleting POST URL from CSV");
    FuriString* buffer_url = furi_string_alloc();
    FuriString* buffer_new_content = furi_string_alloc();
    bool url_found = false;

    if(!storage_file_open(app->file, POST_URL_CSV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open POST URL file for reading");
        furi_string_free(buffer_url);
        furi_string_free(buffer_new_content);
        return false;
    }

    while(read_line_from_file(app->file, buffer_url)) {
        PostUrlList post_url = {0};
        const char* buffer_str = furi_string_get_cstr(buffer_url);
        char payload_str[TEXT_STORE_SIZE] = {0};
        sscanf(buffer_str, "%[^,],%[^\n]", post_url.url, payload_str);

        if(strcmp(post_url.url, url) != 0) {
            furi_string_cat(buffer_new_content, buffer_str);
            furi_string_cat(buffer_new_content, "\n");
        } else {
            url_found = true;
        }
    }

    storage_file_close(app->file);

    if(url_found) {
        if(!storage_file_open(app->file, POST_URL_CSV_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to open POST URL file for writing");
            furi_string_free(buffer_url);
            furi_string_free(buffer_new_content);
            return false;
        }

        if(furi_string_size(buffer_new_content) == 0) {
            storage_file_close(app->file);
            return true;
        }

        if(!storage_file_write(
               app->file,
               furi_string_get_cstr(buffer_new_content),
               furi_string_size(buffer_new_content))) {
            FURI_LOG_E(TAG, "Failed to write new content to POST URL file");
            furi_string_free(buffer_url);
            furi_string_free(buffer_new_content);
            return false;
        }

        storage_file_close(app->file);
    }

    furi_string_free(buffer_url);
    furi_string_free(buffer_new_content);

    FURI_LOG_T(TAG, "Done with deleting POST URL from CSV");
    return url_found;
}
