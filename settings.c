#include "settings.h"

#include <furi.h>
#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

#include <string.h>

#define SETTINGS_PATH APP_DATA_PATH("openshock_settings.cfg")
#define SETTINGS_TYPE "OpenShock Settings"
#define SETTINGS_VER  1u

void openshock_settings_load(bool* transmit_vertical_ui_out) {
    *transmit_vertical_ui_out = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_PATH)) break;

        FuriString* hdr = furi_string_alloc();
        uint32_t ver = 0;
        if(!flipper_format_read_header(ff, hdr, &ver)) {
            furi_string_free(hdr);
            break;
        }
        if(strcmp(furi_string_get_cstr(hdr), SETTINGS_TYPE) != 0 || ver != SETTINGS_VER) {
            furi_string_free(hdr);
            break;
        }
        furi_string_free(hdr);

        uint32_t v = 0;
        if(flipper_format_read_uint32(ff, "TransmitVertical", &v, 1)) {
            *transmit_vertical_ui_out = (v != 0);
        }
    } while(false);
    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

void openshock_settings_save(bool transmit_vertical_ui) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_always(ff, SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_TYPE, SETTINGS_VER)) break;
        uint32_t v = transmit_vertical_ui ? 1u : 0u;
        if(!flipper_format_write_uint32(ff, "TransmitVertical", &v, 1)) break;
    } while(false);
    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}
