/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "recovery.h"

#include <string.h>

#include "../zerofido_crypto.h"
#include "../zerofido_storage.h"
#include "internal.h"
#include "record_format.h"

typedef struct {
    Storage *storage;
    uint8_t *buffer;
    size_t buffer_size;
} ZfStoreRecoveryCleanupContext;

static bool zf_store_recovery_record_primary_is_valid(ZfStoreRecoveryCleanupContext *context,
                                                      const char *file_name) {
    ZfCredentialIndexEntry entry;
    bool valid = false;

    if (!context || !context->buffer || context->buffer_size == 0U || !file_name) {
        return false;
    }
    memset(&entry, 0, sizeof(entry));
    valid = zf_store_record_format_load_index_with_buffer(context->storage, file_name, &entry,
                                                          context->buffer, context->buffer_size);
    zf_crypto_secure_zero(&entry, sizeof(entry));
    return valid;
}

static bool zf_store_recovery_recover_record_backup(ZfStoreRecoveryCleanupContext *context,
                                                    const char *file_name,
                                                    const char *backup_path) {
    char record_path[128];

    if (!zf_storage_build_child_path(ZF_APP_DATA_DIR, file_name, record_path,
                                     sizeof(record_path))) {
        return false;
    }
    if (storage_file_exists(context->storage, record_path) &&
        zf_store_recovery_record_primary_is_valid(context, file_name)) {
        return zf_storage_remove_optional(context->storage, backup_path);
    }
    if (!zf_storage_remove_optional(context->storage, record_path)) {
        return false;
    }
    return storage_common_rename(context->storage, backup_path, record_path) == FSE_OK;
}

static bool zf_store_recovery_recover_counter_backup(Storage *storage, const char *file_name) {
    char counter_path[160];
    char counter_temp_path[160];

    zf_store_build_counter_floor_path(file_name, counter_path, sizeof(counter_path));
    zf_store_build_counter_floor_temp_path(file_name, counter_temp_path, sizeof(counter_temp_path));
    return zf_storage_recover_atomic_file(storage, counter_path, counter_temp_path);
}

static bool zf_store_recovery_cleanup_visitor(const char *name, const FileInfo *info,
                                              void *context) {
    ZfStoreRecoveryCleanupContext *cleanup_context = context;

    if (!name || !info || !cleanup_context) {
        return false;
    }
    if (file_info_is_dir(info)) {
        return true;
    }

    if (zf_store_has_suffix(name, ".bak")) {
        char file_name[96];
        char backup_path[128];
        size_t base_len = strlen(name) - 4;

        if (base_len == 0U || base_len >= sizeof(file_name)) {
            return true;
        }
        memcpy(file_name, name, base_len);
        file_name[base_len] = '\0';
        if (!zf_storage_build_child_path(ZF_APP_DATA_DIR, name, backup_path, sizeof(backup_path))) {
            return false;
        }
        if (zf_store_record_format_is_record_name(file_name)) {
            return zf_store_recovery_recover_record_backup(cleanup_context, file_name, backup_path);
        }
        if (zf_store_has_suffix(file_name, ".counter")) {
            size_t record_name_len = strlen(file_name) - strlen(".counter");

            if (record_name_len > 0U && record_name_len < sizeof(file_name)) {
                file_name[record_name_len] = '\0';
                if (zf_store_record_format_is_record_name(file_name)) {
                    return zf_store_recovery_recover_counter_backup(cleanup_context->storage,
                                                                    file_name);
                }
            }
        }
    }

    return true;
}

/*
 * Startup recovery is conservative: temp files are discarded, while backup
 * files are restored if the primary record is missing or cannot be decoded.
 */
void zf_store_recovery_cleanup_temp_files_with_buffer(Storage *storage, uint8_t *buffer,
                                                      size_t buffer_size) {
    char name[96];
    char path[128];
    ZfStoreRecoveryCleanupContext context = {
        .storage = storage,
        .buffer = buffer,
        .buffer_size = buffer_size,
    };

    zf_storage_remove_dir_entries_with_suffix(storage, ZF_APP_DATA_DIR, ".tmp", name, sizeof(name),
                                              path, sizeof(path));
    zf_storage_for_each_dir_entry(storage, ZF_APP_DATA_DIR, name, sizeof(name),
                                  zf_store_recovery_cleanup_visitor, &context);
}

void zf_store_recovery_cleanup_temp_files(Storage *storage) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];

    zf_store_recovery_cleanup_temp_files_with_buffer(storage, buffer, sizeof(buffer));
    zf_crypto_secure_zero(buffer, sizeof(buffer));
}

bool zf_store_recovery_remove_record_paths(Storage *storage, const char *file_name) {
    char record_path[128];
    char temp_path[128];
    char counter_path[160];
    char counter_temp_path[160];

    zf_store_build_record_path(file_name, record_path, sizeof(record_path));
    zf_store_build_temp_path(file_name, temp_path, sizeof(temp_path));
    zf_store_build_counter_floor_path(file_name, counter_path, sizeof(counter_path));
    zf_store_build_counter_floor_temp_path(file_name, counter_temp_path, sizeof(counter_temp_path));
    return zf_storage_remove_atomic_file(storage, record_path, temp_path) &&
           zf_storage_remove_atomic_file(storage, counter_path, counter_temp_path);
}
