#include "weebo_i.h"
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
    return weebo_file_list_scan(weebo->file_list, weebo->storage, directory);
}

void weebo_free_nfc_file_list(Weebo* weebo) {
    furi_assert(weebo);
    weebo_file_list_reset(weebo->file_list);
}

bool weebo_load_current_file(Weebo* weebo) {
    furi_assert(weebo);

    const char* path_cstr = weebo_file_list_get_current_path(weebo->file_list);
    if(!path_cstr) {
        return false;
    }

    furi_string_set(weebo->load_path, path_cstr);

    // Extract filename for display
    FuriString* filename = furi_string_alloc();
    path_extract_filename(weebo->load_path, filename, true);
    strncpy(weebo->file_name, furi_string_get_cstr(filename), WEEBO_FILE_NAME_MAX_LENGTH);
    furi_string_free(filename);

    bool result = weebo_load_figure(weebo, weebo->load_path, false);
    FURI_LOG_D(
        TAG,
        "Loaded file %zu/%zu: %s",
        weebo_file_list_get_current_index(weebo->file_list) + 1,
        weebo_file_list_get_count(weebo->file_list),
        weebo->file_name);
    return result;
}

size_t weebo_get_file_count(Weebo* weebo) {
    furi_assert(weebo);
    return weebo_file_list_get_count(weebo->file_list);
}

size_t weebo_get_current_file_index(Weebo* weebo) {
    furi_assert(weebo);
    return weebo_file_list_get_current_index(weebo->file_list);
}

bool weebo_set_current_file_path(Weebo* weebo, const char* path) {
    furi_assert(weebo);
    return weebo_file_list_set_current_path(weebo->file_list, path);
}

bool weebo_cycle_file(Weebo* weebo, WeeboCycleDirection direction) {
    furi_assert(weebo);

    size_t count = weebo_file_list_get_count(weebo->file_list);
    if(count == 0) {
        return false;
    }

    size_t original_index = weebo_file_list_get_current_index(weebo->file_list);
    size_t attempts = 0;

    do {
        weebo_file_list_cycle(weebo->file_list, direction);
        attempts++;

        if(weebo_load_current_file(weebo)) {
            return true; // Successfully loaded a valid file
        }

        FURI_LOG_W(TAG, "Skipping invalid file: %s", weebo->file_name);

    } while(weebo_file_list_get_current_index(weebo->file_list) != original_index &&
            attempts < count);

    return false;
}
