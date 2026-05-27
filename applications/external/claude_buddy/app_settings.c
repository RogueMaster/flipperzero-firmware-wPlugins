#include "app_settings.h"

#include <furi.h>
#include <storage/storage.h>

#define TAG "AppSettings"

/* See app_settings.h note: /data/ alias doesn't auto-create the per-app
 * directory when written through from a FAP, so the absolute path +
 * explicit mkdir is the reliable pattern. */
#define SETTINGS_PARENT "/ext/apps_data"
#define SETTINGS_DIR    "/ext/apps_data/claude_buddy"
#define SETTINGS_FILE   "/ext/apps_data/claude_buddy/settings.bin"

/* V1 was just [version, ble_mode].  V2 adds owner_name + device_name.
 * V1 readers continue to work when we read a V2 file (they only look
 * at bytes 0-1); V2 readers upgrade V1 files by defaulting new fields. */
#define SETTINGS_VERSION 2

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t ble_mode;
    /* NUL-terminated within the allotted buffer; unused tail bytes zero. */
    char owner_name[APP_SETTINGS_NAME_MAX];
    char device_name[APP_SETTINGS_DEVNAME_MAX];
} AppSettingsFile;

static void zero_out(AppSettingsFile* s) {
    memset(s, 0, sizeof(*s));
    s->version = SETTINGS_VERSION;
    s->ble_mode = (uint8_t)BleModeBridge;
}

static void read_all(AppSettingsFile* out) {
    zero_out(out);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        /* Peek the version byte first so V1 files upgrade gracefully. */
        uint8_t ver = 0;
        if(storage_file_read(f, &ver, 1) == 1) {
            if(ver == 1) {
                uint8_t mode = 0;
                if(storage_file_read(f, &mode, 1) == 1 && mode <= BleModeDesktop) {
                    out->ble_mode = mode;
                }
            } else if(ver == 2) {
                /* Rewind to the start and read the full struct. */
                storage_file_seek(f, 0, true);
                storage_file_read(f, out, sizeof(*out));
                if(out->ble_mode > BleModeDesktop) out->ble_mode = BleModeBridge;
                out->owner_name[APP_SETTINGS_NAME_MAX - 1] = '\0';
                out->device_name[APP_SETTINGS_DEVNAME_MAX - 1] = '\0';
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void write_all(const AppSettingsFile* in) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SETTINGS_PARENT);
    storage_simply_mkdir(storage, SETTINGS_DIR);

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        if(storage_file_write(f, in, sizeof(*in)) != sizeof(*in)) {
            FURI_LOG_E(TAG, "short write");
        }
    } else {
        FURI_LOG_E(TAG, "open for write failed: %s", SETTINGS_FILE);
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

/* ── public API ─────────────────────────────────────────────── */

BleMode app_settings_get_ble_mode(void) {
    AppSettingsFile s;
    read_all(&s);
    return (BleMode)s.ble_mode;
}

bool app_settings_set_ble_mode(BleMode mode) {
    AppSettingsFile s;
    read_all(&s);
    s.ble_mode = (uint8_t)mode;
    s.version = SETTINGS_VERSION;
    write_all(&s);
    return true;
}

bool app_settings_get_owner_name(char* out, int out_size) {
    if(!out || out_size <= 0) return false;
    AppSettingsFile s;
    read_all(&s);
    strlcpy(out, s.owner_name, (size_t)out_size);
    return out[0] != '\0';
}

void app_settings_set_owner_name(const char* name) {
    AppSettingsFile s;
    read_all(&s);
    memset(s.owner_name, 0, sizeof(s.owner_name));
    if(name) strlcpy(s.owner_name, name, sizeof(s.owner_name));
    s.version = SETTINGS_VERSION;
    write_all(&s);
}

bool app_settings_get_device_name(char* out, int out_size) {
    if(!out || out_size <= 0) return false;
    AppSettingsFile s;
    read_all(&s);
    strlcpy(out, s.device_name, (size_t)out_size);
    return out[0] != '\0';
}

void app_settings_set_device_name(const char* name) {
    AppSettingsFile s;
    read_all(&s);
    memset(s.device_name, 0, sizeof(s.device_name));
    if(name) strlcpy(s.device_name, name, sizeof(s.device_name));
    s.version = SETTINGS_VERSION;
    write_all(&s);
}
