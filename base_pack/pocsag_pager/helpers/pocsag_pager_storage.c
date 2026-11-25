#include "pocsag_pager_storage.h"

#include <furi.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format.h>
#include <string.h>

#define PCSG_SETTINGS_FILE_TYPE   "Flipper POCSAG Settings"
#define PCSG_STORAGE_FILE_VERSION 1U
#define PCSG_SETTINGS_PATH        APP_DATA_PATH("settings.txt")

void pcsg_settings_init_defaults(PCSGSettings* settings) {
    furi_assert(settings);
    memset(settings, 0, sizeof(PCSGSettings));
    settings->frequency = PCSG_DEFAULT_FREQ;
    settings->ric = PCSG_DEFAULT_RIC;
    settings->baud = PocsagBaud1200;
    settings->msg_type = PocsagMsgTypeAlpha;
    settings->inverted = false;
    strlcpy(settings->message, "Hello Flipper!", sizeof(settings->message));
}

bool pcsg_settings_load(PCSGSettings* settings) {
    furi_assert(settings);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    uint32_t version = 0;
    bool ok = false;

    if(flipper_format_file_open_existing(ff, PCSG_SETTINGS_PATH)) {
        if(flipper_format_read_header(ff, filetype, &version) &&
           furi_string_cmp_str(filetype, PCSG_SETTINGS_FILE_TYPE) == 0 &&
           version == PCSG_STORAGE_FILE_VERSION) {
            uint32_t tmp = 0;
            if(flipper_format_read_uint32(ff, "Frequency", &tmp, 1) && tmp > 0) {
                settings->frequency = tmp;
            }
            if(flipper_format_read_uint32(ff, "RIC", &tmp, 1) && tmp <= PCSG_RIC_MAX) {
                settings->ric = tmp;
            }
            if(flipper_format_read_uint32(ff, "Baud", &tmp, 1)) {
                if(tmp == PocsagBaud512 || tmp == PocsagBaud1200 || tmp == PocsagBaud2400) {
                    settings->baud = (PocsagBaudRate)tmp;
                }
            }
            if(flipper_format_read_uint32(ff, "MsgType", &tmp, 1) && tmp <= PocsagMsgTypeAlpha) {
                settings->msg_type = (PocsagMsgType)tmp;
            }
            if(flipper_format_read_uint32(ff, "Inverted", &tmp, 1)) {
                settings->inverted = (tmp != 0);
            }
            FuriString* msg = furi_string_alloc();
            if(flipper_format_read_string(ff, "Message", msg)) {
                strlcpy(settings->message, furi_string_get_cstr(msg), sizeof(settings->message));
            }
            furi_string_free(msg);
            ok = true;
        }
    }

    furi_string_free(filetype);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool pcsg_settings_save(const PCSGSettings* settings) {
    furi_assert(settings);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    if(flipper_format_file_open_always(ff, PCSG_SETTINGS_PATH)) {
        if(flipper_format_write_header_cstr(
               ff, PCSG_SETTINGS_FILE_TYPE, PCSG_STORAGE_FILE_VERSION)) {
            uint32_t freq = settings->frequency;
            uint32_t ric = settings->ric;
            uint32_t baud = (uint32_t)settings->baud;
            uint32_t msg_type = (uint32_t)settings->msg_type;
            uint32_t inv = settings->inverted ? 1U : 0U;

            ok = flipper_format_write_uint32(ff, "Frequency", &freq, 1) &&
                 flipper_format_write_uint32(ff, "RIC", &ric, 1) &&
                 flipper_format_write_uint32(ff, "Baud", &baud, 1) &&
                 flipper_format_write_uint32(ff, "MsgType", &msg_type, 1) &&
                 flipper_format_write_uint32(ff, "Inverted", &inv, 1) &&
                 flipper_format_write_string_cstr(ff, "Message", settings->message);
        }
    }

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
