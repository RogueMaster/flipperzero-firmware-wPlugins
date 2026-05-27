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

#include "bootstrap.h"

#include <string.h>

#include "../zerofido_crypto.h"
#include "../zerofido_storage.h"
#include "../zerofido_store.h"
#include "internal.h"
#include "record_format.h"
#include "recovery.h"

/* Creates the shared application data directories used by store, PIN, and U2F. */
bool zf_store_bootstrap_ensure_app_data_dir(Storage *storage) {
    return zf_storage_ensure_app_data_dir(storage);
}

typedef struct {
    Storage *storage;
    ZfCredentialStore *store;
    uint8_t *buffer;
    size_t buffer_size;
} ZfStoreBootstrapIndexContext;

static bool zf_store_bootstrap_index_visitor(const char *name, const FileInfo *info,
                                             void *context) {
    ZfStoreBootstrapIndexContext *index_context = context;

    if (!name || !info || !index_context || !index_context->store) {
        return false;
    }
    if (file_info_is_dir(info) || !zf_store_record_format_is_record_name(name)) {
        return true;
    }
    if (index_context->store->count >= ZF_MAX_CREDENTIALS) {
        return true;
    }
    if (!zf_store_ensure_capacity(index_context->store, index_context->store->count + 1U)) {
        return false;
    }
    if (zf_store_record_format_load_index_with_buffer(
            index_context->storage, name,
            &index_context->store->records[index_context->store->count], index_context->buffer,
            index_context->buffer_size)) {
        index_context->store->count++;
    }
    return true;
}

/*
 * Rebuilds the volatile credential index from record files. Broken or
 * non-record files are skipped so one corrupt entry does not hide all others.
 */
bool zf_store_bootstrap_init_with_buffer(Storage *storage, ZfCredentialStore *store,
                                         uint8_t *buffer, size_t buffer_size) {
    char name[96];
    bool ok = false;
    ZfStoreBootstrapIndexContext context = {
        .storage = storage,
        .store = store,
        .buffer = buffer,
        .buffer_size = buffer_size,
    };

    if (!storage || !store || !buffer || buffer_size < ZF_STORE_RECORD_MAX_SIZE) {
        return false;
    }

    zf_store_clear(store);
    if (!zf_store_bootstrap_ensure_app_data_dir(storage)) {
        goto cleanup;
    }

    zf_store_recovery_cleanup_temp_files_with_buffer(storage, buffer, buffer_size);
    if (!zf_storage_for_each_dir_entry(storage, ZF_APP_DATA_DIR, name, sizeof(name),
                                       zf_store_bootstrap_index_visitor, &context)) {
        goto cleanup;
    }

    ok = true;

cleanup:
    zf_crypto_secure_zero(buffer, ZF_STORE_RECORD_MAX_SIZE);
    return ok;
}

typedef struct {
    Storage *storage;
} ZfStoreBootstrapWipeContext;

static bool zf_store_bootstrap_wipe_visitor(const char *name, const FileInfo *info, void *context) {
    ZfStoreBootstrapWipeContext *wipe_context = context;
    char path[128];

    if (!name || !info || !wipe_context) {
        return false;
    }
    if (file_info_is_dir(info)) {
        return true;
    }
    if (!zf_storage_build_child_path(ZF_APP_DATA_DIR, name, path, sizeof(path))) {
        return false;
    }
    return zf_storage_remove_optional(wipe_context->storage, path);
}

/*
 * Startup reset removes every credential-like file plus PIN state. U2F files
 * live under their own subtree and are managed by the U2F persistence layer.
 */
bool zf_store_bootstrap_wipe_app_data(Storage *storage) {
    char name[96];
    const char *const pin_paths[] = {
        ZF_APP_DATA_DIR "/client_pin.bin",
        ZF_APP_DATA_DIR "/client_pin.tmp",
        ZF_APP_DATA_DIR "/client_pin_v2.bin",
        ZF_APP_DATA_DIR "/client_pin_v2.tmp",
    };
    ZfStoreBootstrapWipeContext context = {.storage = storage};

    if (!zf_store_bootstrap_ensure_app_data_dir(storage)) {
        return false;
    }

    if (!zf_storage_remove_optional_paths(storage, pin_paths,
                                          sizeof(pin_paths) / sizeof(pin_paths[0]))) {
        return false;
    }

    return zf_storage_for_each_dir_entry(storage, ZF_APP_DATA_DIR, name, sizeof(name),
                                         zf_store_bootstrap_wipe_visitor, &context);
}
