#include "weebo_common.h"
#include <storage/storage.h>
#include <lib/toolbox/path.h>

#define TAG "WeeboCommon"

void weebo_calculate_pwd(uint8_t* uid, uint8_t* pwd) {
    pwd[0] = uid[1] ^ uid[3] ^ 0xAA;
    pwd[1] = uid[2] ^ uid[4] ^ 0x55;
    pwd[2] = uid[3] ^ uid[5] ^ 0xAA;
    pwd[3] = uid[4] ^ uid[6] ^ 0x55;
}

void weebo_remix(Weebo* weebo) {
    uint8_t PWD[4];
    uint8_t UID[8];
    uint8_t modified[NTAG215_SIZE];
    MfUltralightData* data = mf_ultralight_alloc();
    nfc_device_copy_data(weebo->nfc_device, NfcProtocolMfUltralight, data);

    //random uid
    FURI_LOG_D(TAG, "Generating random UID");
    UID[0] = 0x04;
    furi_hal_random_fill_buf(UID + 1, 6);
    UID[3] |= 0x01; // To avoid forbidden 0x88 value
    UID[7] = UID[3] ^ UID[4] ^ UID[5] ^ UID[6];
    memcpy(weebo->figure + NFC3D_UID_OFFSET, UID, 8);
    memcpy(data->iso14443_3a_data->uid, UID, 7);

    //pack
    nfc3d_amiibo_pack(&weebo->keys, weebo->figure, modified);

    //copy data in
    for(size_t i = 0; i < 130; i++) {
        memcpy(
            data->page[i].data, modified + i * MF_ULTRALIGHT_PAGE_SIZE, MF_ULTRALIGHT_PAGE_SIZE);
    }

    //new pwd
    weebo_calculate_pwd(data->iso14443_3a_data->uid, PWD);
    memcpy(data->page[133].data, PWD, sizeof(PWD));

    //set data
    nfc_device_set_data(weebo->nfc_device, NfcProtocolMfUltralight, data);

    mf_ultralight_free(data);
}

bool weebo_scan_nfc_files(Weebo* weebo, const char* directory) {
    furi_assert(weebo);
    furi_assert(directory);

    // Free existing file list
    weebo_free_nfc_file_list(weebo);

    Storage* storage = weebo->storage;
    File* file = storage_file_alloc(storage);
    FileInfo file_info;

    if(!storage_dir_open(file, directory)) {
        FURI_LOG_E(TAG, "Failed to open directory: %s", directory);
        storage_file_free(file);
        return false;
    }

    // Count .nfc files first
    size_t file_count = 0;
    char path[256];
    while(storage_dir_read(file, &file_info, path, sizeof(path))) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        size_t len = strlen(path);
        if(len > 4 && strcmp(path + len - 4, ".nfc") == 0) {
            file_count++;
        }
    }

    if(file_count == 0) {
        FURI_LOG_D(TAG, "No .nfc files found in directory");
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    // Allocate array for file paths
    weebo->nfc_file_list = malloc(sizeof(FuriString*) * file_count);
    weebo->nfc_file_count = 0;

    // Reset directory to read again
    storage_file_close(file);
    if(!storage_dir_open(file, directory)) {
        FURI_LOG_E(TAG, "Failed to reopen directory: %s", directory);
        storage_file_free(file);
        free(weebo->nfc_file_list);
        weebo->nfc_file_list = NULL;
        return false;
    }

    // Fill array with file paths
    while(storage_dir_read(file, &file_info, path, sizeof(path))) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        size_t len = strlen(path);
        if(len > 4 && strcmp(path + len - 4, ".nfc") == 0) {
            FuriString* full_path = furi_string_alloc();
            furi_string_printf(full_path, "%s/%s", directory, path);
            weebo->nfc_file_list[weebo->nfc_file_count] = full_path;
            weebo->nfc_file_count++;
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    // Sort the files alphabetically for consistent order
    for(size_t i = 0; i < weebo->nfc_file_count - 1; i++) {
        for(size_t j = i + 1; j < weebo->nfc_file_count; j++) {
            if(furi_string_cmp(weebo->nfc_file_list[i], weebo->nfc_file_list[j]) > 0) {
                FuriString* temp = weebo->nfc_file_list[i];
                weebo->nfc_file_list[i] = weebo->nfc_file_list[j];
                weebo->nfc_file_list[j] = temp;
            }
        }
    }

    FURI_LOG_D(TAG, "Found %zu .nfc files", weebo->nfc_file_count);
    return true;
}

void weebo_free_nfc_file_list(Weebo* weebo) {
    furi_assert(weebo);

    if(weebo->nfc_file_list) {
        for(size_t i = 0; i < weebo->nfc_file_count; i++) {
            furi_string_free(weebo->nfc_file_list[i]);
        }
        free(weebo->nfc_file_list);
        weebo->nfc_file_list = NULL;
    }
    weebo->nfc_file_count = 0;
    weebo->current_file_index = 0;
}

bool weebo_load_current_file(Weebo* weebo) {
    furi_assert(weebo);

    if(!weebo->nfc_file_list || weebo->nfc_file_count == 0) {
        return false;
    }

    if(weebo->current_file_index >= weebo->nfc_file_count) {
        weebo->current_file_index = 0;
    }

    FuriString* path = weebo->nfc_file_list[weebo->current_file_index];
    furi_string_set(weebo->load_path, path);

    // Extract filename for display
    FuriString* filename = furi_string_alloc();
    path_extract_filename(weebo->load_path, filename, true);
    strncpy(weebo->file_name, furi_string_get_cstr(filename), WEEBO_FILE_NAME_MAX_LENGTH);
    furi_string_free(filename);

    bool result = weebo_load_figure(weebo, weebo->load_path, false);
    FURI_LOG_D(
        TAG,
        "Loaded file %zu/%zu: %s",
        weebo->current_file_index + 1,
        weebo->nfc_file_count,
        weebo->file_name);
    return result;
}

bool weebo_cycle_to_next_file(Weebo* weebo) {
    furi_assert(weebo);

    if(!weebo->nfc_file_list || weebo->nfc_file_count == 0) {
        return false;
    }

    weebo->current_file_index = (weebo->current_file_index + 1) % weebo->nfc_file_count;
    return weebo_load_current_file(weebo);
}

bool weebo_cycle_to_prev_file(Weebo* weebo) {
    furi_assert(weebo);

    if(!weebo->nfc_file_list || weebo->nfc_file_count == 0) {
        return false;
    }

    if(weebo->current_file_index == 0) {
        weebo->current_file_index = weebo->nfc_file_count - 1;
    } else {
        weebo->current_file_index--;
    }
    return weebo_load_current_file(weebo);
}
