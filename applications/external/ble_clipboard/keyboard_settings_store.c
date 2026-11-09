#include "keyboard_settings_store.h"
#include "keyboard_layout.h"
#include "transfer_protocol.h"
#include <string.h>

#define SETTINGS_PATH APP_DATA_PATH(".keyboard-v1")
#define SETTINGS_TEMP SETTINGS_PATH ".tmp"
#define SETTINGS_BACKUP SETTINGS_PATH ".bak"
#define SETTINGS_SIZE 16

static bool encode(const ClipboardKeyboardSettings* settings, uint8_t bytes[SETTINGS_SIZE]) {
    if(settings->layout >= CLIPBOARD_LAYOUT_COUNT || settings->target > 2 || !settings->revision) return false;
    memset(bytes, 0, SETTINGS_SIZE);
    memcpy(bytes, "BCK1", 4);
    bytes[4] = settings->layout; bytes[5] = settings->target; bytes[6] = settings->override_saved;
    clipboard_write_u32(bytes + 8, settings->revision);
    clipboard_write_u32(bytes + 12, clipboard_crc_update(0xFFFFFFFFU, bytes, 12) ^ 0xFFFFFFFFU);
    return true;
}

static bool read_settings(Storage* storage, const char* path, ClipboardKeyboardSettings* settings) {
    File* file = storage_file_alloc(storage);
    uint8_t bytes[SETTINGS_SIZE];
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              !storage_file_is_dir(file) && storage_file_size(file) == sizeof(bytes) &&
              storage_file_read(file, bytes, sizeof(bytes)) == sizeof(bytes);
    storage_file_close(file); storage_file_free(file);
    if(!ok || memcmp(bytes, "BCK1", 4) || bytes[4] >= CLIPBOARD_LAYOUT_COUNT ||
       bytes[5] > 2 || bytes[6] > 1 || bytes[7] || !clipboard_read_u32(bytes + 8) ||
       clipboard_read_u32(bytes + 12) != (clipboard_crc_update(0xFFFFFFFFU, bytes, 12) ^ 0xFFFFFFFFU)) return false;
    *settings = (ClipboardKeyboardSettings){bytes[4], bytes[5], bytes[6] != 0, clipboard_read_u32(bytes + 8)};
    return true;
}

static bool verify_settings(Storage* storage, const char* path, const ClipboardKeyboardSettings* expected) {
    ClipboardKeyboardSettings actual;
    return read_settings(storage, path, &actual) && actual.layout == expected->layout &&
           actual.target == expected->target && actual.override_saved == expected->override_saved &&
           actual.revision == expected->revision;
}

bool clipboard_keyboard_settings_load(Storage* storage, ClipboardKeyboardSettings* settings) {
    *settings = (ClipboardKeyboardSettings){0};
    if(read_settings(storage, SETTINGS_PATH, settings)) {
        storage_simply_remove(storage, SETTINGS_BACKUP);
        return true;
    }
    if(!read_settings(storage, SETTINGS_BACKUP, settings)) return false;
    if(storage_file_exists(storage, SETTINGS_PATH)) storage_simply_remove(storage, SETTINGS_PATH);
    storage_common_rename(storage, SETTINGS_BACKUP, SETTINGS_PATH);
    return true;
}

bool clipboard_keyboard_settings_save(Storage* storage, const ClipboardKeyboardSettings* settings) {
    uint8_t bytes[SETTINGS_SIZE];
    if(!encode(settings, bytes) || !storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX)) return false;
    if(storage_file_exists(storage, SETTINGS_BACKUP)) return false;
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, SETTINGS_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(file, bytes, sizeof(bytes)) == sizeof(bytes) && storage_file_sync(file);
    storage_file_close(file); storage_file_free(file);
    if(!ok || !verify_settings(storage, SETTINGS_TEMP, settings)) return false;
    bool previous = storage_file_exists(storage, SETTINGS_PATH);
    if(previous && storage_common_rename(storage, SETTINGS_PATH, SETTINGS_BACKUP) != FSE_OK) return false;
    ok = storage_common_rename(storage, SETTINGS_TEMP, SETTINGS_PATH) == FSE_OK &&
         verify_settings(storage, SETTINGS_PATH, settings);
    if(!ok) {
        if(previous) {
            storage_simply_remove(storage, SETTINGS_PATH);
            storage_common_rename(storage, SETTINGS_BACKUP, SETTINGS_PATH);
        }
        return false;
    }
    if(previous) storage_simply_remove(storage, SETTINGS_BACKUP);
    return true;
}
