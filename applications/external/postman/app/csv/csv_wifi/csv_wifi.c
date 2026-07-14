#include "./csv_wifi.h"
#include "../../app.h"
#include "../../structs.h"
#include <furi.h>
#include <stdlib.h>
#include <storage/storage.h>
#include "../csv_utils/csv_utils.h"

bool init_csv_wifi(App* app) {
    FURI_LOG_T(TAG, "Initializing CSV");

    if(!app->file) {
        FURI_LOG_E(TAG, "Failed to allocate storage file");
        return false;
    }

    // Check if the file exists
    if(storage_file_exists(app->storage, APP_DATA_PATH("wifi.csv"))) {
        FURI_LOG_I(TAG, "CSV file exists, reading WiFi credentials from file");

        if(!sync_csv_to_mem(app)) {
            FURI_LOG_E(TAG, "Failed to read WiFi credentials from CSV file");
            storage_file_close(app->file);
            return false;
        }
    } else {
        FURI_LOG_I(TAG, "CSV file does not exist, creating a new one.");
        if(!storage_file_open(
               app->file, APP_DATA_PATH("wifi.csv"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to create CSV file");
            storage_file_close(app->file);
            return false;
        }
        storage_file_close(app->file);

        // Initialize csv_networks as empty
        app->csv_networks = NULL;
        app->csv_networks_count = 0;
    }
    FURI_LOG_T(TAG, "Done with initializing CSV");
    return true;
}

bool sync_csv_to_mem(App* app) {
    FURI_LOG_T(TAG, "Syncing CSV with flipper memory");
    FuriString* buffer = furi_string_alloc();

    if(!storage_file_open(app->file, APP_DATA_PATH("wifi.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
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
    if(!storage_file_open(app->file, APP_DATA_PATH("wifi.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to reopen file");
        furi_string_free(buffer);
        return false;
    }

    // Allocate memory for the csv_networks based on the number of lines
    free(app->csv_networks); // Free the minimal memory allocated during init
    if(line_count > 0) {
        app->csv_networks = malloc(line_count * sizeof(WifiCredential));
        if(!app->csv_networks) {
            FURI_LOG_E(TAG, "Failed to allocate memory for csv_networks");
            furi_string_free(buffer);
            storage_file_close(app->file);
            return false;
        }
    } else {
        app->csv_networks = NULL;
    }
    app->csv_networks_count = line_count;

    // Read and parse each line into the csv_networks
    size_t index = 0;
    while(index < line_count && read_line_from_file(app->file, buffer)) {
        const char* buffer_str = furi_string_get_cstr(buffer);
        sscanf(
            buffer_str,
            "%31[^,],%63[^,],%d",
            app->csv_networks[index].ssid,
            app->csv_networks[index].password,
            (int*)&app->csv_networks[index].is_default);
        index++;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);
    FURI_LOG_T(TAG, "Done with syncing CSV with flipper memory");
    return true;
}

bool write_wifi_to_csv(App* app, const WifiCredential* wifi) {
    FURI_LOG_T(TAG, "Writing wifi to CSV");
    if(!storage_file_open(app->file, APP_DATA_PATH("wifi.csv"), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FURI_LOG_E(TAG, "Failed to open file for writing");
        return false;
    }

    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "%s,%s,%d\n", wifi->ssid, wifi->password, wifi->is_default);

    if(!storage_file_write(app->file, furi_string_get_cstr(buffer), furi_string_size(buffer))) {
        FURI_LOG_E(TAG, "Failed to write wifi to file");
        furi_string_free(buffer);
        storage_file_close(app->file);
        return false;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);
    FURI_LOG_T(TAG, "Done with writing wifi to CSV");
    return true;
}

bool read_wifis_from_csv(App* app) {
    FURI_LOG_T(TAG, "Reading wifi's from CSV");

    FuriString* buffer = furi_string_alloc();
    if(!storage_file_open(app->file, APP_DATA_PATH("wifi.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
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
    if(!storage_file_open(app->file, APP_DATA_PATH("wifi.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to reopen file");
        furi_string_free(buffer);
        return false;
    }

    // Allocate memory for the csv_networks based on the number of lines
    free(app->csv_networks); // Free the minimal memory allocated during init
    if(line_count > 0) {
        app->csv_networks = malloc(line_count * sizeof(WifiCredential));
        if(!app->csv_networks) {
            FURI_LOG_E(TAG, "Failed to allocate memory for csv_networks");
            furi_string_free(buffer);
            storage_file_close(app->file);
            return false;
        }
    } else {
        app->csv_networks = NULL;
    }
    app->csv_networks_count = line_count;

    // Read and parse each line into the csv_networks
    size_t index = 0;
    while(index < line_count && read_line_from_file(app->file, buffer)) {
        const char* buffer_str = furi_string_get_cstr(buffer);
        sscanf(
            buffer_str,
            "%31[^,],%63[^,],%d",
            app->csv_networks[index].ssid,
            app->csv_networks[index].password,
            (int*)&app->csv_networks[index].is_default);
        index++;
    }

    furi_string_free(buffer);
    storage_file_close(app->file);

    FURI_LOG_T(TAG, "Done with reading wifi's from CSV");
    return true;
}

bool delete_wifi_from_csv(App* app, const char* ssid) {
    FURI_LOG_T(TAG, "Deleting wifi from CSV");
    FuriString* buffer_wifi = furi_string_alloc();
    FuriString* buffer_new_content = furi_string_alloc();
    bool wifi_found = false;

    if(!storage_file_open(app->file, APP_DATA_PATH("wifi.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file for reading");
        furi_string_free(buffer_wifi);
        furi_string_free(buffer_new_content);
        return false;
    }

    while(read_line_from_file(app->file, buffer_wifi)) {
        WifiCredential wifi = {0};
        const char* buffer_str = furi_string_get_cstr(buffer_wifi);
        sscanf(buffer_str, "%31[^,],%63[^,],%d", wifi.ssid, wifi.password, (int*)&wifi.is_default);

        if(strcmp(wifi.ssid, ssid) != 0) {
            furi_string_cat(buffer_new_content, buffer_str);
            furi_string_cat(buffer_new_content, "\n");
        } else {
            wifi_found = true;
        }
    }

    storage_file_close(app->file);

    if(wifi_found) {
        if(!storage_file_open(
               app->file, APP_DATA_PATH("wifi.csv"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to open file for writing");
            furi_string_free(buffer_wifi);
            furi_string_free(buffer_new_content);
            return false;
        }

        //if the new content is empty, just close the file and return
        if(furi_string_size(buffer_new_content) == 0) {
            storage_file_close(app->file);
            return true;
        }

        if(!storage_file_write(
               app->file,
               furi_string_get_cstr(buffer_new_content),
               furi_string_size(buffer_new_content))) {
            FURI_LOG_E(TAG, "Failed to write new content to file");
            furi_string_free(buffer_wifi);
            furi_string_free(buffer_new_content);
            return false;
        }

        storage_file_close(app->file);
    }

    furi_string_free(buffer_wifi);
    furi_string_free(buffer_new_content);

    FURI_LOG_T(TAG, "Done with deleting wifi from CSV");
    return wifi_found;
}
