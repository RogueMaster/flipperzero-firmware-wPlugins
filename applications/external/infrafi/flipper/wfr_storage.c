#include "wfr_storage.h"
#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <string.h>

#define WFR_FILE_TYPE    "InfraFi Credentials"
#define WFR_FILE_VERSION 1

/* Sanitize a string for use as a filename.
 * Replaces non-alphanumeric chars (except - and _) with underscores. */
static void sanitize_for_filename(const char* src, char* dst, size_t dst_size) {
    size_t j = 0;
    for(size_t i = 0; src[i] && j + 1 < dst_size; i++) {
        char c = src[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_') {
            dst[j++] = c;
        } else {
            dst[j++] = '_';
        }
    }
    dst[j] = '\0';

    /* Ensure non-empty */
    if(j == 0 && dst_size > 1) {
        dst[0] = '_';
        dst[1] = '\0';
    }
}

bool wfr_storage_save(Storage* storage, const WfrWifiCreds* creds) {
    if(!creds || creds->ssid[0] == '\0') return false;

    storage_simply_mkdir(storage, WFR_SAVE_DIR);

    char safe_name[48];
    sanitize_for_filename(creds->ssid, safe_name, sizeof(safe_name));

    char path[128];
    snprintf(path, sizeof(path), "%s/%s%s", WFR_SAVE_DIR, safe_name, WFR_SAVE_EXT);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, path)) break;
        if(!flipper_format_write_header_cstr(ff, WFR_FILE_TYPE, WFR_FILE_VERSION)) break;
        if(!flipper_format_write_string_cstr(ff, "SSID", creds->ssid)) break;
        uint32_t sec = creds->security;
        if(!flipper_format_write_uint32(ff, "Security", &sec, 1)) break;
        if(!flipper_format_write_string_cstr(ff, "Password", creds->password)) break;
        uint32_t hidden = creds->hidden ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "Hidden", &hidden, 1)) break;
        ok = true;
    } while(false);

    flipper_format_free(ff);
    return ok;
}

bool wfr_storage_load(Storage* storage, const char* path, WfrWifiCreds* creds) {
    if(!path || !creds) return false;

    memset(creds, 0, sizeof(WfrWifiCreds));

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    FuriString* tmp = furi_string_alloc();

    do {
        if(!flipper_format_file_open_existing(ff, path)) break;

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, tmp, &version)) break;
        if(furi_string_cmp_str(tmp, WFR_FILE_TYPE) != 0) break;
        if(version != WFR_FILE_VERSION) break;

        if(!flipper_format_read_string(ff, "SSID", tmp)) break;
        strncpy(creds->ssid, furi_string_get_cstr(tmp), WFR_SSID_MAX_LEN);

        uint32_t sec = 0;
        if(!flipper_format_read_uint32(ff, "Security", &sec, 1)) break;
        creds->security = (uint8_t)sec;

        if(!flipper_format_read_string(ff, "Password", tmp)) break;
        strncpy(creds->password, furi_string_get_cstr(tmp), WFR_PASS_MAX_LEN);

        uint32_t hidden = 0;
        if(flipper_format_read_uint32(ff, "Hidden", &hidden, 1)) {
            creds->hidden = (hidden != 0);
        }

        ok = true;
    } while(false);

    furi_string_free(tmp);
    flipper_format_free(ff);
    return ok;
}

uint8_t wfr_storage_list(
    Storage* storage,
    char ssids[][WFR_SSID_MAX_LEN + 1],
    char filenames[][WFR_FILENAME_MAX],
    uint8_t max_count) {
    uint8_t count = 0;

    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, WFR_SAVE_DIR)) {
        storage_file_free(dir);
        return 0;
    }

    FileInfo info;
    char name[128];

    while(count < max_count && storage_dir_read(dir, &info, name, sizeof(name))) {
        if(info.flags & FSF_DIRECTORY) continue;

        /* Check extension */
        size_t name_len = strlen(name);
        size_t ext_len = strlen(WFR_SAVE_EXT);
        if(name_len <= ext_len) continue;
        if(strcmp(&name[name_len - ext_len], WFR_SAVE_EXT) != 0) continue;

        /* Try loading SSID from the file */
        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s/%s", WFR_SAVE_DIR, name);

        WfrWifiCreds tmp_creds;
        if(wfr_storage_load(storage, full_path, &tmp_creds)) {
            strncpy(ssids[count], tmp_creds.ssid, WFR_SSID_MAX_LEN);
            ssids[count][WFR_SSID_MAX_LEN] = '\0';
            strncpy(filenames[count], name, WFR_FILENAME_MAX - 1);
            filenames[count][WFR_FILENAME_MAX - 1] = '\0';
            count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    return count;
}

bool wfr_storage_delete(Storage* storage, const char* filename) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", WFR_SAVE_DIR, filename);
    return storage_simply_remove(storage, path);
}

#define WFR_SETTINGS_TYPE    "InfraFi Settings"
#define WFR_SETTINGS_VERSION 1

bool wfr_storage_load_settings(Storage* storage, bool* ack_enabled, WfrIrProtocol* ir_protocol) {
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;
    FuriString* tmp = furi_string_alloc();

    do {
        if(!flipper_format_file_open_existing(ff, WFR_SETTINGS_FILE)) break;

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, tmp, &version)) break;
        if(furi_string_cmp_str(tmp, WFR_SETTINGS_TYPE) != 0) break;
        if(version != WFR_SETTINGS_VERSION) break;

        uint32_t ack = 0;
        if(flipper_format_read_uint32(ff, "WaitForACK", &ack, 1)) {
            *ack_enabled = (ack != 0);
        }
        uint32_t proto = 0;
        if(flipper_format_read_uint32(ff, "IrProtocol", &proto, 1)) {
            *ir_protocol = (proto <= WfrIrProtocolNEC) ? (WfrIrProtocol)proto : WfrIrProtocolRC6;
        }
        ok = true;
    } while(false);

    furi_string_free(tmp);
    flipper_format_free(ff);
    return ok;
}

bool wfr_storage_save_settings(Storage* storage, bool ack_enabled, WfrIrProtocol ir_protocol) {
    storage_simply_mkdir(storage, WFR_SAVE_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, WFR_SETTINGS_FILE)) break;
        if(!flipper_format_write_header_cstr(ff, WFR_SETTINGS_TYPE, WFR_SETTINGS_VERSION)) break;
        uint32_t ack = ack_enabled ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "WaitForACK", &ack, 1)) break;
        uint32_t proto = (uint32_t)ir_protocol;
        if(!flipper_format_write_uint32(ff, "IrProtocol", &proto, 1)) break;
        ok = true;
    } while(false);

    flipper_format_free(ff);
    return ok;
}
