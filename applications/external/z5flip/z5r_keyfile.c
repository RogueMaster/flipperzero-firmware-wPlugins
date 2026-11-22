#include "z5r.h"
#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <one_wire/maxim_crc.h>

/*
 * Key <-> Flipper key files, chosen by the key's own format:
 *   6-byte key (DS1990A) <-> .ibtn files in /ext/ibutton  (Protocol DS1990, ROM = 01 + b0..b5 + crc8)
 *   3-byte key (Wiegand-26 / EM-Marine) <-> .rfid files in /ext/lfrfid  (H10301, Data = FC, number hi, lo = b2 b1 b0)
 * EM4100 (5 bytes) is stored by the controller as a 6-byte key, b0..b4 = Data reversed, b5 = 0
 * (seen in a real controller dump); it is the same record shape as a DS1990, so `em` picks the file kind.
 * Cyfral/Metakom keys are not supported.
 */
bool z5r_key_export(Storage* st, const Z5rKey* k, bool em, char* path, size_t n) {
    char s[13];
    z5r_key_str(k, s, sizeof(s));
    bool dallas = k->len == 6 && !em;
    storage_simply_mkdir(st, dallas ? "/ext/ibutton" : "/ext/lfrfid");
    snprintf(
        path, n, "/ext/%s/Z5R_%s.%s", dallas ? "ibutton" : "lfrfid", s, dallas ? "ibtn" : "rfid");
    FlipperFormat* f = flipper_format_file_alloc(st);
    bool ok = flipper_format_file_open_always(f, path);
    if(ok && dallas) {
        uint8_t rom[8] = {0x01};
        memcpy(rom + 1, k->b, 6);
        rom[7] = maxim_crc8(rom, 7, 0);
        ok = flipper_format_write_header_cstr(f, "Flipper iButton key", 2) &&
             flipper_format_write_string_cstr(f, "Protocol", "DS1990") &&
             flipper_format_write_hex(f, "Rom Data", rom, 8);
    } else if(ok && em) {
        uint8_t d[5] = {k->b[4], k->b[3], k->b[2], k->b[1], k->b[0]};
        ok = flipper_format_write_header_cstr(f, "Flipper RFID key", 1) &&
             flipper_format_write_string_cstr(f, "Key type", "EM4100") &&
             flipper_format_write_hex(f, "Data", d, 5);
    } else if(ok) {
        uint8_t d[3] = {k->b[2], k->b[1], k->b[0]};
        ok = flipper_format_write_header_cstr(f, "Flipper RFID key", 1) &&
             flipper_format_write_string_cstr(f, "Key type", "H10301") &&
             flipper_format_write_hex(f, "Data", d, 3);
    }
    flipper_format_free(f);
    return ok;
}

bool z5r_key_import(Storage* st, const char* path, Z5rKey* k) {
    FlipperFormat* f = flipper_format_file_alloc(st);
    FuriString* t = furi_string_alloc();
    uint8_t d[8];
    uint32_t ver;
    bool ok = false;
    memset(k, 0, sizeof(*k));
    k->attr = Z5rAttrNormal;
    if(flipper_format_file_open_existing(f, path) && flipper_format_read_header(f, t, &ver)) {
        if(furi_string_equal(t, "Flipper iButton key")) {
            /* v2: Protocol + Rom Data; v1: Key type Dallas + Data */
            bool v2 = flipper_format_rewind(f) && flipper_format_read_string(f, "Protocol", t);
            ok = (v2 ? furi_string_equal(t, "DS1990") :
                       flipper_format_rewind(f) && flipper_format_read_string(f, "Key type", t) &&
                           furi_string_equal(t, "Dallas")) &&
                 flipper_format_rewind(f) &&
                 flipper_format_read_hex(f, v2 ? "Rom Data" : "Data", d, 8) &&
                 maxim_crc8(d, 7, 0) == d[7];
            if(ok) {
                memcpy(k->b, d + 1, 6);
                k->b[5] &= 0x0F; /* top nibble of byte 6 holds flags in controller memory */
                k->len = 6;
            }
        } else if(furi_string_equal(t, "Flipper RFID key")) {
            ok = flipper_format_rewind(f) && flipper_format_read_string(f, "Key type", t);
            bool em = ok && furi_string_equal(t, "EM4100");
            ok = ok && (em || furi_string_equal(t, "H10301")) && flipper_format_rewind(f) &&
                 flipper_format_read_hex(f, "Data", d, em ? 5 : 3);
            if(ok) z5r_key_from_rfid(em, d, k);
        }
    }
    furi_string_free(t);
    flipper_format_free(f);
    return ok;
}
