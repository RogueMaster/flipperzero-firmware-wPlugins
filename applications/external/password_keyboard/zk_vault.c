#include "zk_vault.h"

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <string.h>

#define ZK_VAULT_MAGIC     0x44424B5AU
#define ZK_VAULT_VERSION   2
#define ZK_VAULT_PATH      APP_DATA_PATH("vault.bin")
#define ZK_VAULT_TEMP_PATH APP_DATA_PATH("vault.tmp")
#define ZK_IMPORT_PATH     APP_DATA_PATH("import.txt")

static uint32_t zk_random_bounded(uint32_t bound) {
    if(bound < 2) return 0;
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % bound);
    uint32_t value;
    do {
        value = furi_hal_random_get();
    } while(value >= limit);
    return value % bound;
}

bool zk_password_generate(char* output, uint8_t length, uint8_t classes) {
    static const char lower[] = "abcdefghijklmnopqrstuvwxyz";
    static const char upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char numbers[] = "0123456789";
    static const char special[] = "!@#$%^&*()-_=+[]{};:,.?/";
    const char* sets[4] = {lower, upper, numbers, special};
    const uint8_t bits[4] = {ZkClassLower, ZkClassUpper, ZkClassNumber, ZkClassSpecial};
    if(!output || !classes || length < 4 || length > ZK_MAX_PASSWORD_LENGTH) return false;

    uint8_t enabled_count = 0;
    for(uint8_t i = 0; i < 4; i++) {
        if(classes & bits[i]) {
            const size_t set_length = strlen(sets[i]);
            output[enabled_count++] = sets[i][zk_random_bounded(set_length)];
        }
    }
    if(length < enabled_count) return false;
    for(uint8_t i = enabled_count; i < length; i++) {
        uint8_t selected;
        do {
            selected = zk_random_bounded(4);
        } while(!(classes & bits[selected]));
        const size_t set_length = strlen(sets[selected]);
        output[i] = sets[selected][zk_random_bounded(set_length)];
    }
    for(uint8_t i = length - 1; i > 0; i--) {
        const uint8_t swap = zk_random_bounded(i + 1);
        const char temp = output[i];
        output[i] = output[swap];
        output[swap] = temp;
    }
    output[length] = '\0';
    return true;
}

void zk_vault_init(ZkVault* vault) {
    memset(vault, 0, sizeof(*vault));
    vault->magic = ZK_VAULT_MAGIC;
    vault->version = ZK_VAULT_VERSION;
}

bool zk_vault_load(ZkVault* vault) {
    zk_vault_init(vault);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    const bool opened = storage_file_open(file, ZK_VAULT_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    bool valid = false;
    bool migrate = false;
    if(opened && storage_file_size(file) == sizeof(*vault)) {
        valid = storage_file_read(file, vault, sizeof(*vault)) == sizeof(*vault) &&
                vault->magic == ZK_VAULT_MAGIC &&
                (vault->version == 1 || vault->version == ZK_VAULT_VERSION) &&
                vault->count <= ZK_MAX_PASSWORDS;
        if(valid && vault->version == 1) {
            valid = vault->legacy_daily_limit > 0 && vault->legacy_daily_limit <= 9;
            if(valid) {
                for(uint8_t i = 0; i < vault->count; i++) {
                    if(vault->records[i].flags & ZkPasswordHidden) {
                        vault->records[i].daily_limit = vault->legacy_daily_limit;
                    }
                }
                vault->version = ZK_VAULT_VERSION;
                vault->reserved = 0;
                vault->legacy_daily_limit = 0;
                migrate = true;
            }
        } else if(valid) {
            for(uint8_t i = 0; i < vault->count; i++) {
                const ZkPasswordRecord* record = &vault->records[i];
                if((record->flags & ZkPasswordHidden) &&
                   (!record->daily_limit || record->daily_limit > 9)) {
                    valid = false;
                    break;
                }
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!valid)
        zk_vault_init(vault);
    else if(migrate)
        zk_vault_save(vault);
    return valid;
}

bool zk_vault_save(const ZkVault* vault) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    File* file = storage_file_alloc(storage);
    bool success = storage_file_open(file, ZK_VAULT_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        success = storage_file_write(file, vault, sizeof(*vault)) == sizeof(*vault) &&
                  storage_file_sync(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    if(success) {
        storage_common_remove(storage, ZK_VAULT_PATH);
        success = storage_common_rename(storage, ZK_VAULT_TEMP_PATH, ZK_VAULT_PATH) == FSE_OK;
    }
    furi_record_close(RECORD_STORAGE);
    return success;
}

uint32_t zk_vault_today(void) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    return (uint32_t)now.year * 10000U + (uint32_t)now.month * 100U + now.day;
}

static void zk_vault_refresh_record(ZkPasswordRecord* record) {
    const uint32_t today = zk_vault_today();
    if(record->usage_day != today) {
        record->usage_day = today;
        record->used_today = 0;
    }
}

uint8_t zk_vault_remaining(ZkVault* vault, uint8_t index) {
    if(index >= vault->count) return 0;
    ZkPasswordRecord* record = &vault->records[index];
    if(!(record->flags & ZkPasswordHidden)) return UINT8_MAX;
    zk_vault_refresh_record(record);
    return record->used_today >= record->daily_limit ? 0 :
                                                       record->daily_limit - record->used_today;
}

bool zk_vault_consume(ZkVault* vault, uint8_t index) {
    if(index >= vault->count) return false;
    ZkPasswordRecord* record = &vault->records[index];
    if(!(record->flags & ZkPasswordHidden)) return true;
    zk_vault_refresh_record(record);
    if(record->used_today >= record->daily_limit) return false;
    record->used_today++;
    return zk_vault_save(vault);
}

bool zk_vault_add(
    ZkVault* vault,
    const char* name,
    const char* password,
    bool hidden,
    uint8_t daily_limit,
    uint8_t already_used) {
    if(vault->count >= ZK_MAX_PASSWORDS || !name || !password) return false;
    const size_t length = strlen(password);
    if(!length || length > ZK_MAX_PASSWORD_LENGTH) return false;
    if(hidden && (!daily_limit || daily_limit > 9)) return false;
    ZkPasswordRecord* record = &vault->records[vault->count];
    memset(record, 0, sizeof(*record));
    strlcpy(record->name, name, sizeof(record->name));
    record->flags = hidden ? ZkPasswordHidden : 0;
    record->length = length;
    record->daily_limit = hidden ? daily_limit : 0;
    record->usage_day = zk_vault_today();
    record->used_today = hidden ? already_used : 0;
    furi_hal_random_fill_buf(record->nonce, sizeof(record->nonce));
    uint8_t key[ZK_KEY_SIZE];
    zk_crypto_derive_device_key(key);
    zk_crypto_seal(
        key, record->nonce, (const uint8_t*)password, record->length, record->cipher, record->tag);
    zk_crypto_wipe(key, sizeof(key));
    vault->count++;
    if(!zk_vault_save(vault)) {
        vault->count--;
        memset(record, 0, sizeof(*record));
        return false;
    }
    return true;
}

bool zk_vault_decrypt(const ZkPasswordRecord* record, char output[ZK_MAX_PASSWORD_LENGTH + 1]) {
    if(!record || !output || !record->length || record->length > ZK_MAX_PASSWORD_LENGTH)
        return false;
    uint8_t key[ZK_KEY_SIZE];
    zk_crypto_derive_device_key(key);
    const bool valid = zk_crypto_open(
        key, record->nonce, record->cipher, record->length, record->tag, (uint8_t*)output);
    zk_crypto_wipe(key, sizeof(key));
    if(valid)
        output[record->length] = '\0';
    else
        zk_crypto_wipe(output, ZK_MAX_PASSWORD_LENGTH + 1);
    return valid;
}

bool zk_vault_rename(ZkVault* vault, uint8_t index, const char* name) {
    if(index >= vault->count || !name || !name[0]) return false;
    char previous[ZK_NAME_LENGTH];
    strlcpy(previous, vault->records[index].name, sizeof(previous));
    strlcpy(vault->records[index].name, name, sizeof(vault->records[index].name));
    if(zk_vault_save(vault)) return true;
    strlcpy(vault->records[index].name, previous, sizeof(vault->records[index].name));
    return false;
}

bool zk_vault_delete(ZkVault* vault, uint8_t index) {
    if(index >= vault->count) return false;
    for(uint8_t i = index; i + 1 < vault->count; i++) {
        vault->records[i] = vault->records[i + 1];
    }
    memset(&vault->records[vault->count - 1], 0, sizeof(ZkPasswordRecord));
    vault->count--;
    return zk_vault_save(vault);
}

uint8_t zk_vault_import(ZkVault* vault) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, ZK_IMPORT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return 0;
    }
    char buffer[1025];
    size_t read = storage_file_read(file, buffer, sizeof(buffer) - 1);
    buffer[read] = '\0';
    storage_file_close(file);
    storage_file_free(file);

    uint8_t imported = 0;
    char* cursor = buffer;
    while(*cursor && vault->count < ZK_MAX_PASSWORDS) {
        char* end = strchr(cursor, '\n');
        if(end) *end = '\0';
        size_t line_length = strlen(cursor);
        if(line_length && cursor[line_length - 1] == '\r') cursor[line_length - 1] = '\0';
        if(*cursor && *cursor != '#') {
            char* separator = strchr(cursor, '|');
            if(separator) {
                *separator = '\0';
                const char* name = cursor;
                const char* password = separator + 1;
                if(*name && *password && strlen(password) <= ZK_MAX_PASSWORD_LENGTH &&
                   zk_vault_add(vault, name, password, false, 0, 0)) {
                    imported++;
                }
            }
        }
        if(!end) break;
        cursor = end + 1;
    }
    zk_crypto_wipe(buffer, sizeof(buffer));
    if(imported) storage_common_remove(storage, ZK_IMPORT_PATH);
    furi_record_close(RECORD_STORAGE);
    return imported;
}
