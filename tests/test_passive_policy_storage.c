#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <furi.h>
#include <storage/storage.h>

#include "mf_passive_policy.h"

struct Storage {
    uint8_t unused;
};

struct File {
    const char* path;
};

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t mode;
    uint8_t length;
    uint8_t lesson;
    uint16_t dit_ms;
    uint8_t farnsworth_wpm;
    uint8_t vibrate;
    uint8_t answer_delay_s;
    uint8_t repeat_after_answer;
    uint8_t selected_row;
    uint8_t reserved;
} SavedRecord;

_Static_assert(sizeof(SavedRecord) == 16U, "saved record test layout changed");

static Storage storage;
static File file;
static unsigned record_opens;
static unsigned record_closes;
static unsigned allocs;
static unsigned frees;
static unsigned read_opens;
static unsigned write_opens;
static unsigned writes;
static unsigned closes;
static unsigned removes;
static unsigned renames;
static bool write_open_result;
static size_t write_result;
static bool close_results[4];
static FS_Error remove_results[3];
static FS_Error rename_result;
static bool final_present;
static bool temp_present;
static SavedRecord final_record;
static SavedRecord temp_record;
static SavedRecord written_record;
static char removed_paths[3][32];
static char renamed_from[32];
static char renamed_to[32];

void* furi_record_open(const char* name) {
    assert(strcmp(name, RECORD_STORAGE) == 0);
    record_opens++;
    return &storage;
}

void furi_record_close(const char* name) {
    assert(strcmp(name, RECORD_STORAGE) == 0);
    record_closes++;
}

File* storage_file_alloc(Storage* value) {
    assert(value == &storage);
    allocs++;
    return &file;
}

void storage_file_free(File* value) {
    assert(value == &file);
    frees++;
}

bool storage_file_open(
    File* value,
    const char* path,
    FS_AccessMode access,
    FS_OpenMode mode) {
    assert(value == &file);
    value->path = path;
    if(access == FSAM_READ) {
        assert(mode == FSOM_OPEN_EXISTING);
        read_opens++;
        if(strcmp(path, "/data/passive.bin") == 0) return final_present;
        assert(strcmp(path, "/data/passive.tmp") == 0);
        return temp_present;
    }
    assert(access == FSAM_WRITE);
    assert(mode == FSOM_CREATE_ALWAYS);
    assert(strcmp(path, "/data/passive.tmp") == 0);
    write_opens++;
    return write_open_result;
}

size_t storage_file_read(File* value, void* buffer, size_t size) {
    const SavedRecord* record;

    assert(value == &file);
    assert(size == sizeof(SavedRecord));
    record = strcmp(value->path, "/data/passive.bin") == 0 ? &final_record : &temp_record;
    memcpy(buffer, record, sizeof(*record));
    return sizeof(*record);
}

size_t storage_file_write(File* value, const void* buffer, size_t size) {
    assert(value == &file);
    assert(strcmp(value->path, "/data/passive.tmp") == 0);
    assert(size == sizeof(written_record));
    memcpy(&written_record, buffer, sizeof(written_record));
    writes++;
    return write_result;
}

uint64_t storage_file_size(File* value) {
    assert(value == &file);
    return sizeof(SavedRecord);
}

bool storage_file_close(File* value) {
    assert(value == &file);
    assert(closes < 4U);
    return close_results[closes++];
}

FS_Error storage_common_remove(Storage* value, const char* path) {
    assert(value == &storage);
    assert(removes < 3U);
    snprintf(removed_paths[removes], sizeof(removed_paths[removes]), "%s", path);
    return remove_results[removes++];
}

FS_Error storage_common_rename(Storage* value, const char* old_path, const char* new_path) {
    assert(value == &storage);
    snprintf(renamed_from, sizeof(renamed_from), "%s", old_path);
    snprintf(renamed_to, sizeof(renamed_to), "%s", new_path);
    renames++;
    return rename_result;
}

static SavedRecord valid_record(uint8_t selected_row) {
    return (SavedRecord){
        .magic = 0x4D465053UL,
        .version = 1U,
        .mode = 1U,
        .length = 5U,
        .lesson = 2U,
        .dit_ms = 80U,
        .farnsworth_wpm = 12U,
        .vibrate = 1U,
        .answer_delay_s = 4U,
        .repeat_after_answer = 1U,
        .selected_row = selected_row,
    };
}

static void reset_storage(void) {
    record_opens = 0U;
    record_closes = 0U;
    allocs = 0U;
    frees = 0U;
    read_opens = 0U;
    write_opens = 0U;
    writes = 0U;
    closes = 0U;
    removes = 0U;
    renames = 0U;
    write_open_result = true;
    write_result = sizeof(SavedRecord);
    for(unsigned i = 0U; i < 4U; i++) close_results[i] = true;
    for(unsigned i = 0U; i < 3U; i++) remove_results[i] = FSE_OK;
    rename_result = FSE_OK;
    final_present = false;
    temp_present = false;
    final_record = valid_record(1U);
    temp_record = valid_record(2U);
    memset(&written_record, 0, sizeof(written_record));
    memset(removed_paths, 0, sizeof(removed_paths));
    memset(renamed_from, 0, sizeof(renamed_from));
    memset(renamed_to, 0, sizeof(renamed_to));
}

static void check_resources(void) {
    assert(record_opens == 1U && record_closes == 1U);
    assert(allocs == 1U && frees == 1U);
}

static void check_removed_temp(unsigned expected) {
    assert(removes == expected);
    for(unsigned i = 0U; i < removes; i++)
        assert(strcmp(removed_paths[i], "/data/passive.tmp") == 0);
}

static void test_load_prefers_valid_final(void) {
    MfPassiveSettingsModel model;

    reset_storage();
    final_present = true;
    temp_present = true;
    mf_passive_settings_load(&model);
    check_resources();
    assert(read_opens == 1U && closes == 1U);
    assert(model.selected_row == 1U && model.dit_ms == 80U);
}

static void test_load_recovers_from_temp(void) {
    MfPassiveSettingsModel model;

    reset_storage();
    final_present = true;
    temp_present = true;
    final_record.magic = 0U;
    mf_passive_settings_load(&model);
    check_resources();
    assert(read_opens == 2U && closes == 2U);
    assert(model.selected_row == 2U && model.lesson == 2U);

    reset_storage();
    temp_present = true;
    mf_passive_settings_load(&model);
    check_resources();
    assert(read_opens == 2U && closes == 2U);
    assert(model.selected_row == 2U);
}

static void test_load_close_failure_uses_temp(void) {
    MfPassiveSettingsModel model;

    reset_storage();
    final_present = true;
    temp_present = true;
    close_results[0] = false;
    mf_passive_settings_load(&model);
    check_resources();
    assert(read_opens == 2U && closes == 2U);
    assert(model.selected_row == 2U);
}

static void check_failed_save(unsigned expected_writes, unsigned expected_renames) {
    MfPassiveSettingsModel model = {0};

    assert(!mf_passive_settings_save(&model));
    check_resources();
    assert(write_opens == 1U && closes == 1U);
    assert(writes == expected_writes && renames == expected_renames);
}

static void test_save_success(void) {
    MfPassiveSettingsModel model = {
        .mode = 1U,
        .length = 6U,
        .lesson = 2U,
        .dit_ms = 60U,
        .farnsworth_wpm = 15U,
        .vibrate = 1U,
        .answer_delay_s = 5U,
        .repeat_after_answer = 1U,
        .selected_row = 7U,
    };

    reset_storage();
    assert(mf_passive_settings_save(&model));
    check_resources();
    check_removed_temp(1U);
    assert(write_opens == 1U && writes == 1U && closes == 1U && renames == 1U);
    assert(strcmp(renamed_from, "/data/passive.tmp") == 0);
    assert(strcmp(renamed_to, "/data/passive.bin") == 0);
    assert(written_record.magic == 0x4D465053UL && written_record.version == 1U);
    assert(written_record.mode == model.mode && written_record.length == model.length);
    assert(written_record.dit_ms == model.dit_ms);
    assert(written_record.selected_row == model.selected_row);
}

static void test_save_pre_rename_failures_clean_temp(void) {
    reset_storage();
    write_open_result = false;
    check_failed_save(0U, 0U);
    check_removed_temp(2U);

    reset_storage();
    write_result = sizeof(SavedRecord) - 1U;
    check_failed_save(1U, 0U);
    check_removed_temp(2U);

    reset_storage();
    close_results[0] = false;
    check_failed_save(1U, 0U);
    check_removed_temp(2U);
}

static void test_rename_failure_retains_temp(void) {
    reset_storage();
    rename_result = FSE_INTERNAL;
    check_failed_save(1U, 1U);
    check_removed_temp(1U);
}

static void test_remove_failures_are_bounded(void) {
    reset_storage();
    write_open_result = false;
    remove_results[0] = FSE_DENIED;
    remove_results[1] = FSE_NOT_READY;
    check_failed_save(0U, 0U);
    check_removed_temp(2U);
}

static void test_null_model(void) {
    reset_storage();
    assert(!mf_passive_settings_save(NULL));
    assert(record_opens == 0U && allocs == 0U && write_opens == 0U);
}

int main(void) {
    test_load_prefers_valid_final();
    test_load_recovers_from_temp();
    test_load_close_failure_uses_temp();
    test_save_success();
    test_save_pre_rename_failures_clean_temp();
    test_rename_failure_retains_temp();
    test_remove_failures_are_bounded();
    test_null_model();
    printf("test_passive_policy_storage: passed\n");
    return 0;
}
