#include "http.h"
#include "flipper_http/flipper_http.h"
// #include "app.hpp"

static FlipperHTTP* flipperHttp = nullptr;
static char last_saved_file_path[64] = {0};

void http_deinit() {
    if(flipperHttp) {
        free(flipperHttp); // this is just a pointer now..
        flipperHttp = nullptr;
    }
}

bool http_init(void* httpInstance) {
    if(!httpInstance) {
        FURI_LOG_E("HTTP", "http_init: httpInstance is NULL");
        return false;
    }
    // share instance with app
    flipperHttp = static_cast<FlipperHTTP*>(httpInstance);
    return flipperHttp != nullptr;
}

bool http_get_http_response(char* buffer, size_t buffer_size) {
    if(!flipperHttp || !buffer || buffer_size == 0) {
        FURI_LOG_E("HTTP", "http_get_http_response: Invalid arguments");
        return false;
    }
    Storage* storage = static_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    File* file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(
        file_path,
        sizeof(file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s",
        "ghouls",
        last_saved_file_path);
    if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E("HTTP", "http_get_http_response: Failed to open response file: %s", file_path);
        return false;
    }
    size_t read_count = storage_file_read(file, buffer, buffer_size);
    // ensure we don't go out of bounds
    if(read_count > 0 && read_count < buffer_size) {
        buffer[read_count - 1] = '\0';
    } else if(read_count >= buffer_size && buffer_size > 0) {
        buffer[buffer_size - 1] = '\0';
    } else {
        buffer[0] = '\0';
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

bool http_get_websocket_response(char* buffer, size_t buffer_size) {
    if(!flipperHttp || !buffer || buffer_size == 0) {
        return false;
    }
    memcpy(buffer, flipperHttp->last_response, buffer_size);
    buffer[buffer_size - 1] = '\0'; // Ensure null termination
    flipperHttp->last_response[0] = '\0';
    return true;
}

bool http_websocket_is_connected() {
    if(!flipperHttp || !flipperHttp->last_response) {
        return false;
    }
    return strstr(flipperHttp->last_response, "[SOCKET/CONNECTED]") != NULL;
}

bool http_is_finished() {
    return flipperHttp && flipperHttp->state == IDLE;
}

bool http_send_request(
    const char* url,
    const char* method,
    const char* headers,
    const char* payload) {
    if(!flipperHttp || !url || !method) {
        FURI_LOG_E("HTTP", "http_send_request: Invalid arguments");
        return false;
    }
    snprintf(last_saved_file_path, sizeof(last_saved_file_path), "http_%s.txt", method);
    snprintf(
        flipperHttp->file_path,
        sizeof(flipperHttp->file_path),
        "%s/%s",
        STORAGE_EXT_PATH_PREFIX "/apps_data/ghouls/data",
        last_saved_file_path);
    flipperHttp->save_received_data = true;
    flipperHttp->state = IDLE;
    HTTPMethod http_method = GET;
    // this game only sends post/get so no need to check for others..
    if(strcmp(method, "POST") == 0) {
        http_method = POST;
    }
    if(!flipper_http_request(flipperHttp, http_method, url, headers, payload)) {
        FURI_LOG_E("HTTP", "http_send_request: Failed to send HTTP request");
        return false;
    }
    flipperHttp->state = RECEIVING;
    return true;
}

bool http_websocket_send(const char* message) {
    if(!flipperHttp || !message) {
        FURI_LOG_E("HTTP", "http_websocket_send: invalid arguments");
        return false;
    }
    return flipper_http_send_data(flipperHttp, message);
}

bool http_websocket_start(const char* url, uint16_t port) {
    if(!flipperHttp) {
        FURI_LOG_E("HTTP", "http_websocket_start: FlipperHTTP is not initialized");
        return false;
    }
    if(!url || strlen(url) == 0) {
        FURI_LOG_E("HTTP", "http_websocket_start: WebSocket URL is NULL or empty");
        return false;
    }
    return flipper_http_websocket_start(
        flipperHttp, url, port, "{\"Content-Type\":\"application/json\"}");
}

bool http_websocket_stop() {
    if(!flipperHttp) {
        FURI_LOG_E("HTTP", "http_websocket_stop: FlipperHTTP is NULL");
        return false;
    }
    return flipper_http_websocket_stop(flipperHttp);
}

bool http_file_download(const char* url, const char* destination_path) {
    if(!flipperHttp || !url || !destination_path) {
        FURI_LOG_E("HTTP", "http_file_download: Invalid arguments");
        return false;
    }
    snprintf(flipperHttp->file_path, sizeof(flipperHttp->file_path), destination_path);
    flipperHttp->save_received_data = false;
    flipperHttp->is_bytes_request = true;
    flipperHttp->state = IDLE;
    return flipper_http_request(
        flipperHttp, BYTES, url, "{\"Content-Type\": \"application/octet-stream\"}", NULL);
}
