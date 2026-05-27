#include "logger.h"
#include "wmbus_app_i.h"
#include "protocol/wmbus_manuf.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

static void write_str(File* f, const char* s) {
    storage_file_write(f, s, strlen(s));
}

bool wmbus_log_telegram(
    Storage* storage,
    const uint8_t* frame, size_t len,
    int8_t rssi,
    const WmbusLinkFrame* link,
    const char* parsed_text) {
    storage_simply_mkdir(storage, WMBUS_APP_DATA_DIR);
    storage_simply_mkdir(storage, WMBUS_APP_LOG_DIR);

    /* One file per power-on session. */
    static char path[96];
    static bool init;
    if(!init) {
        snprintf(path, sizeof(path), "%s/session_%lu.log",
                 WMBUS_APP_LOG_DIR, (unsigned long)furi_get_tick());
        init = true;
    }

    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(f); return false;
    }

    /* Line 1: raw hex */
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "RAW %lu %d ",
             (unsigned long)furi_get_tick(), (int)rssi);
    write_str(f, hdr);
    char hx[3];
    for(size_t i = 0; i < len; i++) {
        snprintf(hx, sizeof(hx), "%02X", frame[i]);
        storage_file_write(f, hx, 2);
    }
    write_str(f, "\n");

    /* Line 2: JSON header */
    char m[4]; wmbus_manuf_decode(link->M, m);
    char json[160];
    int n = snprintf(json, sizeof(json),
        "JSON {\"manuf\":\"%s\",\"id\":\"%08lX\",\"ver\":%u,\"med\":%u,"
        "\"c\":%u,\"ci\":%u,\"enc\":%u,\"rssi\":%d}\n",
        m, (unsigned long)link->id, link->version, link->medium,
        link->C, link->ci, link->enc_mode, (int)rssi);
    storage_file_write(f, json, (uint16_t)n);

    /* Line 3+: human readable */
    if(parsed_text && *parsed_text) {
        write_str(f, "TXT ");
        write_str(f, parsed_text);
        if(parsed_text[strlen(parsed_text) - 1] != '\n') write_str(f, "\n");
    }

    storage_file_close(f); storage_file_free(f);
    return true;
}
