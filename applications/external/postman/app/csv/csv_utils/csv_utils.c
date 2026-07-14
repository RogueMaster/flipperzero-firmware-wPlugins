#include "../../app.h"
#include "./csv_utils.h"
#include "../../structs.h"

bool read_line_from_file(File* file, FuriString* str_result) {
    FURI_LOG_T(TAG, "Reading line from CSV file");
    furi_string_reset(str_result);
    uint8_t buffer[1];
    bool result = false;

    while(true) {
        size_t read_count = storage_file_read(file, buffer, sizeof(buffer));
        if(read_count == 0) break;
        if(buffer[0] == '\n') {
            result = true;
            break;
        } else {
            furi_string_push_back(str_result, buffer[0]);
        }
    }

    furi_string_push_back(str_result, '\0');
    FURI_LOG_T(TAG, "Done with reading line from CSV file");
    return result;
}

bool compare_url(const FuriString* url1, const FuriString* url2) {
    if(url1 == NULL || url2 == NULL) {
        return false;
    }

    return furi_string_cmp(url1, url2) == 0;
}

bool url_in_csv(App* app, const char* url, StateType state_type) {
    FuriString* url_str = furi_string_alloc();
    furi_string_set_str(url_str, url);
    FuriString* csv_url = furi_string_alloc();
    bool url_in_csv = false;

    size_t max_urls = 0;

    switch(state_type) {
    case StateTypeGet:
        max_urls = app->url_list_count;
        break;
    case StateTypePost:
        max_urls = app->post_url_list_count;
        break;
    case StateTypeBuildHttp:
        max_urls = app->build_http_list_size; // Use dynamic size
        break;
    default:
        goto cleanup;
    }

    for(size_t i = 0; i < max_urls; i++) {
        switch(state_type) {
        case StateTypeGet:
            furi_string_set_str(csv_url, app->url_list[i].url);
            break;
        case StateTypePost:
            furi_string_set_str(csv_url, app->post_url_list[i].url);
            break;
        case StateTypeBuildHttp:
            furi_string_set_str(csv_url, app->build_http_list[i].url);
            break;
        default:
            break;
        }

        FURI_LOG_D(
            TAG,
            "Comparing URLs: %s and %s",
            furi_string_get_cstr(url_str),
            furi_string_get_cstr(csv_url));
        if(compare_url(url_str, csv_url)) {
            url_in_csv = true;
            break;
        }

        furi_string_reset(csv_url);
    }

cleanup:
    furi_string_free(url_str);
    furi_string_free(csv_url);
    return url_in_csv;
}
