#include "protopirate_saved_match.h"
#include "protopirate_storage.h"
#include "../protocols/protocols_common.h"
#include <storage/storage.h>
#include <string.h>

#define TAG              "ProtoPirateMatch"
#define CNT_MATCH_MARGIN 50

bool protopirate_saved_match_signal(
    FlipperFormat* received_ff,
    FuriString* out_matched_name,
    FuriString* out_matched_path) {
    furi_check(received_ff);
    furi_check(out_matched_name);
    furi_check(out_matched_path);

    FuriString* rx_protocol = furi_string_alloc();
    FuriString* rx_key = furi_string_alloc();
    uint32_t rx_serial = 0;
    uint32_t rx_cnt = 0;
    bool rx_has_serial = false;
    bool rx_has_key = false;
    bool rx_has_cnt = false;

    flipper_format_rewind(received_ff);
    if(!flipper_format_read_string(received_ff, FF_PROTOCOL, rx_protocol)) {
        furi_string_free(rx_protocol);
        furi_string_free(rx_key);
        return false;
    }

    flipper_format_rewind(received_ff);
    if(flipper_format_read_uint32(received_ff, FF_SERIAL, &rx_serial, 1)) {
        rx_has_serial = true;
    }

    if(!rx_has_serial) {
        flipper_format_rewind(received_ff);
        if(flipper_format_read_string(received_ff, FF_KEY, rx_key)) {
            rx_has_key = true;
        }
    }

    flipper_format_rewind(received_ff);
    if(flipper_format_read_uint32(received_ff, FF_CNT, &rx_cnt, 1)) {
        rx_has_cnt = true;
    }

    if(!rx_has_serial && !rx_has_key) {
        FURI_LOG_D(TAG, "No Serial or Key in received signal, skip match");
        furi_string_free(rx_protocol);
        furi_string_free(rx_key);
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    bool found = false;

    if(!storage_dir_open(dir, PROTOPIRATE_APP_FOLDER)) {
        FURI_LOG_D(TAG, "Cannot open saved/ folder");
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(rx_protocol);
        furi_string_free(rx_key);
        return false;
    }

    FileInfo file_info;
    char file_name[128];

    while(storage_dir_read(dir, &file_info, file_name, sizeof(file_name))) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        size_t name_len = strlen(file_name);
        if(name_len < 5) continue;

        if(strcmp(file_name + name_len - 4, PROTOPIRATE_APP_EXTENSION) != 0) continue;

        if(strcmp(file_name, ".temp.psf") == 0) continue;

        FuriString* saved_path =
            furi_string_alloc_printf("%s/%s", PROTOPIRATE_APP_FOLDER, file_name);

        FlipperFormat* saved_ff = flipper_format_file_alloc(storage);
        if(!flipper_format_file_open_existing(saved_ff, furi_string_get_cstr(saved_path))) {
            flipper_format_free(saved_ff);
            furi_string_free(saved_path);
            continue;
        }

        FuriString* saved_protocol = furi_string_alloc();
        flipper_format_rewind(saved_ff);
        bool protocol_match = flipper_format_read_string(saved_ff, FF_PROTOCOL, saved_protocol) &&
                              furi_string_cmp(rx_protocol, saved_protocol) == 0;
        furi_string_free(saved_protocol);

        if(!protocol_match) {
            flipper_format_free(saved_ff);
            furi_string_free(saved_path);
            continue;
        }

        bool identity_match = false;
        if(rx_has_serial) {
            uint32_t saved_serial = 0;
            flipper_format_rewind(saved_ff);
            if(flipper_format_read_uint32(saved_ff, FF_SERIAL, &saved_serial, 1)) {
                identity_match = (saved_serial == rx_serial);
            }
        } else {
            FuriString* saved_key = furi_string_alloc();
            flipper_format_rewind(saved_ff);
            if(flipper_format_read_string(saved_ff, FF_KEY, saved_key)) {
                identity_match = (furi_string_cmp(rx_key, saved_key) == 0);
            }
            furi_string_free(saved_key);
        }

        if(!identity_match) {
            flipper_format_free(saved_ff);
            furi_string_free(saved_path);
            continue;
        }

        if(rx_has_cnt) {
            uint32_t saved_cnt = 0;
            flipper_format_rewind(saved_ff);
            if(flipper_format_read_uint32(saved_ff, FF_CNT, &saved_cnt, 1)) {
                int64_t diff = (int64_t)rx_cnt - (int64_t)saved_cnt;
                if(diff < 0) diff = -diff;
                if(diff > CNT_MATCH_MARGIN) {
                    FURI_LOG_D(
                        TAG,
                        "Cnt diff %lld > %d, skip %s",
                        (long long)diff,
                        CNT_MATCH_MARGIN,
                        file_name);
                    flipper_format_free(saved_ff);
                    furi_string_free(saved_path);
                    continue;
                }
            }
        }

        size_t ext_pos = name_len - 4;
        furi_string_set_strn(out_matched_name, file_name, ext_pos);
        furi_string_set(out_matched_path, saved_path);
        found = true;

        flipper_format_free(saved_ff);
        furi_string_free(saved_path);
        break;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    furi_string_free(rx_protocol);
    furi_string_free(rx_key);

    return found;
}
