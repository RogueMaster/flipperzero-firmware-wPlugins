#include "mac_changer.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define TAG "MACCHG"

void mac_changer_generate_random(uint8_t mac[6]) {
    furi_hal_random_fill_buf(mac, 6);
    /* Set locally administered bit (bit 1 of first byte) */
    mac[0] |= 0x02;
    /* Clear multicast bit (bit 0 of first byte) */
    mac[0] &= 0xFE;
}

bool mac_changer_save(const uint8_t mac[6]) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, MAC_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[18];
        snprintf(
            buf,
            sizeof(buf),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]);
        ok = storage_file_write(file, buf, 17) == 17;
        storage_file_close(file);
        FURI_LOG_I(TAG, "MAC saved: %s", buf);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool mac_changer_load(uint8_t mac[6]) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, MAC_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[18];
        memset(buf, 0, sizeof(buf));
        uint16_t read = storage_file_read(file, buf, 17);
        storage_file_close(file);

        if(read == 17) {
            unsigned int m[6];
            if(sscanf(
                   buf, "%02X:%02X:%02X:%02X:%02X:%02X", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) ==
               6) {
                for(int i = 0; i < 6; i++)
                    mac[i] = (uint8_t)m[i];
                ok = true;
                FURI_LOG_I(TAG, "MAC loaded: %s", buf);
            }
        }
    } else {
        FURI_LOG_I(TAG, "No MAC config file found");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool mac_changer_delete_config(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = storage_simply_remove(storage, MAC_CONFIG_PATH);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "MAC config deleted: %s", ok ? "ok" : "fail");
    return ok;
}
