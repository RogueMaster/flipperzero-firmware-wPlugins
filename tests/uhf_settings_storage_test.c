/* Compile the real settings and storage implementations against a fault-injected
 * filesystem. See the command reported with this change for host compilation. */
#include "uhf_settings.h"
#include <storage/storage.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct Storage { unsigned unused; };
typedef struct {
    const char* path;
    uint8_t data[64];
    size_t size;
    bool exists;
} Blob;
struct File { Blob* blob; bool open; };
static struct Storage backend;
static struct File handle;
static Blob blobs[] = {
    {.path = "/current/settings.bin"},
    {.path = "/current/settings.tmp"},
    {.path = "/legacy/uhf_expansion/settings.bin"},
};
static bool fail_write, fail_sync, fail_close, fail_rename, fail_open;
static unsigned references, rename_calls, remove_calls, sync_calls, close_calls;

static Blob* lookup(const char* path) {
    for(size_t i = 0; i < sizeof(blobs) / sizeof(blobs[0]); ++i)
        if(strcmp(blobs[i].path, path) == 0) return &blobs[i];
    return NULL;
}

void* furi_record_open(const char* name) {
    assert(strcmp(name, RECORD_STORAGE) == 0);
    references++;
    return &backend;
}
void furi_record_close(const char* name) {
    assert(strcmp(name, RECORD_STORAGE) == 0 && references > 0);
    references--;
}
File* storage_file_alloc(Storage* storage) {
    assert(storage == &backend && !handle.open);
    return &handle;
}
void storage_file_free(File* file) { assert(file == &handle && !file->open); }
bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode) {
    assert(file == &handle);
    Blob* blob = lookup(path);
    if(!blob || fail_open) return false;
    if(mode == FSOM_OPEN_EXISTING && !blob->exists) return false;
    if(mode == FSOM_CREATE_ALWAYS) {
        /* The old destination must never be opened for writing. */
        assert(access == FSAM_WRITE && strcmp(path, "/current/settings.tmp") == 0);
        blob->exists = true;
        blob->size = 0;
    }
    file->blob = blob;
    file->open = true;
    return true;
}
bool storage_file_close(File* file) {
    assert(file == &handle);
    if(file->open) close_calls++;
    file->open = false;
    return !fail_close;
}
size_t storage_file_read(File* file, void* data, size_t size) {
    assert(file->open);
    const size_t count = size < file->blob->size ? size : file->blob->size;
    memcpy(data, file->blob->data, count);
    return count;
}
size_t storage_file_write(File* file, const void* data, size_t size) {
    assert(file->open && size <= sizeof(file->blob->data));
    const size_t count = fail_write ? size / 2U : size;
    memcpy(file->blob->data, data, count);
    file->blob->size = count;
    return count;
}
bool storage_file_sync(File* file) {
    assert(file->open);
    sync_calls++;
    return !fail_sync;
}
FS_Error storage_common_mkdir(Storage* storage, const char* path) {
    assert(storage == &backend && strcmp(path, "/current") == 0);
    return FSE_EXIST;
}
FS_Error storage_common_rename(Storage* storage, const char* from, const char* to) {
    assert(storage == &backend && !handle.open);
    Blob* source = lookup(from);
    Blob* target = lookup(to);
    assert(source && target && source->exists && sync_calls && close_calls);
    rename_calls++;
    if(fail_rename) return FSE_INTERNAL;
    memcpy(target->data, source->data, source->size);
    target->size = source->size;
    target->exists = true;
    source->exists = false;
    return FSE_OK;
}
FS_Error storage_common_remove(Storage* storage, const char* path) {
    assert(storage == &backend && strcmp(path, "/current/settings.tmp") == 0);
    remove_calls++;
    Blob* blob = lookup(path);
    blob->exists = false;
    blob->size = 0;
    return FSE_OK;
}

static const UhfSettingsData original = {
    .magic = UHF_SETTINGS_MAGIC, .sound_enabled = 1, .rf_power_dbm = 10,
    .startup_app = UhfStartupInventory, .epc_display = UhfEpcDisplayHex,
    .action_confirm = 1, .reader_buzzer_enabled = 1, .reader_buzzer_marker = 0xA5,
};
static const UhfSettingsData replacement = {
    .magic = UHF_SETTINGS_MAGIC, .sound_enabled = 0, .rf_power_dbm = 20,
    .startup_app = UhfStartupRadar, .epc_display = UhfEpcDisplayAscii,
    .action_confirm = 0, .reader_buzzer_enabled = 0, .reader_buzzer_marker = 0xA5,
};

static void reset(void) {
    assert(references == 0 && !handle.open);
    for(size_t i = 0; i < sizeof(blobs) / sizeof(blobs[0]); ++i) {
        blobs[i].exists = false;
        blobs[i].size = 0;
        memset(blobs[i].data, 0, sizeof(blobs[i].data));
    }
    memcpy(blobs[0].data, &original, sizeof(original));
    blobs[0].size = sizeof(original);
    blobs[0].exists = true;
    fail_write = fail_sync = fail_close = fail_rename = fail_open = false;
    rename_calls = remove_calls = sync_calls = close_calls = 0;
}
static void assert_previous_preserved(void) {
    assert(blobs[0].exists && blobs[0].size == sizeof(original));
    assert(memcmp(blobs[0].data, &original, sizeof(original)) == 0);
    assert(!blobs[1].exists && references == 0 && !handle.open);
}
static void test_success(void) {
    reset();
    assert(uhf_settings_save(&replacement));
    assert(rename_calls == 1 && sync_calls == 1 && close_calls == 1);
    assert(blobs[0].size == sizeof(replacement));
    assert(memcmp(blobs[0].data, &replacement, sizeof(replacement)) == 0);
    assert(!blobs[1].exists && references == 0);
    UhfSettingsData loaded;
    uhf_settings_load(&loaded);
    assert(loaded.magic == replacement.magic);
    assert(loaded.sound_enabled == replacement.sound_enabled);
    assert(loaded.rf_power_dbm == replacement.rf_power_dbm);
    assert(loaded.startup_app == replacement.startup_app);
    assert(loaded.epc_display == replacement.epc_display);
    assert(loaded.action_confirm == replacement.action_confirm);
    assert(loaded.reader_buzzer_enabled == replacement.reader_buzzer_enabled);
}
static void test_failures(void) {
    reset(); fail_write = true;
    assert(!uhf_settings_save(&replacement));
    assert(rename_calls == 0 && sync_calls == 0 && remove_calls == 1);
    assert_previous_preserved();

    reset(); fail_sync = true;
    assert(!uhf_settings_save(&replacement));
    assert(rename_calls == 0 && sync_calls == 1 && remove_calls == 1);
    assert_previous_preserved();

    reset(); fail_close = true;
    assert(!uhf_settings_save(&replacement));
    assert(rename_calls == 0 && sync_calls == 1 && close_calls == 1 && remove_calls == 1);
    assert_previous_preserved();
    /* Read semantics remain unchanged: a complete read is returned even when
       the underlying close reports a failure. */
    UhfSettingsData loaded;
    uhf_settings_load(&loaded);
    assert(loaded.magic == original.magic && loaded.rf_power_dbm == original.rf_power_dbm);
    assert(references == 0 && !handle.open);

    reset(); fail_rename = true;
    assert(!uhf_settings_save(&replacement));
    assert(rename_calls == 1 && remove_calls == 1);
    assert_previous_preserved();

    reset(); fail_open = true;
    assert(!uhf_settings_save(&replacement));
    assert(rename_calls == 0 && remove_calls == 1);
    assert_previous_preserved();

    reset();
    assert(!uhf_settings_save(NULL));
    assert(rename_calls == 0 && remove_calls == 0);
    assert_previous_preserved();
}
static void test_legacy_migration(void) {
    reset();
    blobs[0].exists = false;
    memcpy(blobs[2].data, &original, 8);
    blobs[2].size = 8;
    blobs[2].exists = true;
    UhfSettingsData loaded;
    uhf_settings_load(&loaded);
    assert(loaded.magic == original.magic && loaded.rf_power_dbm == original.rf_power_dbm);
    assert(loaded.action_confirm == 1 && loaded.reader_buzzer_enabled == 1);
    assert(blobs[0].exists && blobs[0].size == sizeof(UhfSettingsData));
    assert(rename_calls == 1 && !blobs[1].exists && references == 0);
}
int main(void) {
    test_success();
    test_failures();
    test_legacy_migration();
    puts("uhf_settings_storage: all atomic-save tests passed");
    return 0;
}
