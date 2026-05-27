/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "zerofido_storage.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <string.h>

#include "zerofido_crypto.h"

#define ZF_STORAGE_BLOB_MAX_DATA_SIZE 64U
#define ZF_STORAGE_BLOB_MAX_MAC_SIZE 64U
#define ZF_STORAGE_PATH_BUFFER_SIZE 192U
#define ZF_STORAGE_TEMP_SUFFIX ".tmp"
#define ZF_STORAGE_BACKUP_SUFFIX ".bak"

typedef struct {
    const ZfStorageEncryptedBlobWriteSpec *spec;
    const uint8_t *iv;
    const uint8_t *encrypted;
    const uint8_t *mac;
    size_t mac_len;
} ZfStorageEncryptedBlobWriteContext;

bool zf_storage_ensure_dir(Storage *storage, const char *path) {
    if (!storage || !path) {
        return false;
    }
    return storage_dir_exists(storage, path) || storage_simply_mkdir(storage, path);
}

bool zf_storage_ensure_app_data_dir(Storage *storage) {
    return zf_storage_ensure_dir(storage, ZF_APP_DATA_ROOT) &&
           zf_storage_ensure_dir(storage, ZF_APP_DATA_DIR);
}

bool zf_storage_remove_optional(Storage *storage, const char *path) {
    FS_Error result;

    if (!path) {
        return false;
    }
    result = storage_common_remove(storage, path);
    return result == FSE_OK || result == FSE_NOT_EXIST;
}

bool zf_storage_remove_optional_paths(Storage *storage, const char *const *paths, size_t count) {
    if (!paths && count > 0U) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!zf_storage_remove_optional(storage, paths[i])) {
            return false;
        }
    }
    return true;
}

bool zf_storage_build_child_path(const char *dir_path, const char *name, char *path,
                                 size_t path_size) {
    size_t dir_len = 0U;
    size_t name_len = 0U;
    bool needs_separator = false;
    size_t offset = 0U;

    if (!dir_path || !name || !path || path_size == 0U) {
        return false;
    }

    dir_len = strlen(dir_path);
    name_len = strlen(name);
    needs_separator = dir_len > 0U && dir_path[dir_len - 1U] != '/';
    if (dir_len + (needs_separator ? 1U : 0U) + name_len + 1U > path_size) {
        path[0] = '\0';
        return false;
    }

    memcpy(path, dir_path, dir_len);
    offset = dir_len;
    if (needs_separator) {
        path[offset++] = '/';
    }
    memcpy(path + offset, name, name_len);
    path[offset + name_len] = '\0';
    return true;
}

bool zf_storage_read_file(Storage *storage, const char *path, uint8_t *data, size_t capacity,
                          size_t *out_size) {
    File *file = NULL;
    size_t size = 0U;
    bool ok = false;

    if (out_size) {
        *out_size = 0U;
    }
    if (!storage || !path || !data || capacity == 0U || !out_size) {
        return false;
    }

    file = storage_file_alloc(storage);
    if (!file) {
        return false;
    }
    if (!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    size = storage_file_size(file);
    if (size > 0U && size <= capacity && storage_file_read(file, data, size) == size) {
        *out_size = size;
        ok = true;
    }

    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool zf_storage_for_each_dir_entry(Storage *storage, const char *dir_path, char *name_buffer,
                                   size_t name_buffer_size, ZfStorageDirEntryVisitor visitor,
                                   void *context) {
    File *dir = NULL;
    FileInfo info;
    bool ok = false;
    bool opened = false;

    if (!dir_path || !name_buffer || name_buffer_size == 0U || !visitor) {
        return false;
    }

    dir = storage_file_alloc(storage);
    if (!dir) {
        return false;
    }
    if (!storage_dir_open(dir, dir_path)) {
        goto cleanup;
    }
    opened = true;

    ok = true;
    while (storage_dir_read(dir, &info, name_buffer, name_buffer_size)) {
        if (!visitor(name_buffer, &info, context)) {
            ok = false;
            break;
        }
    }

cleanup:
    if (opened) {
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    return ok;
}

typedef struct {
    Storage *storage;
    const char *dir_path;
    const char *suffix;
    char *path_buffer;
    size_t path_buffer_size;
} ZfStorageRemoveSuffixContext;

static bool zf_storage_remove_suffix_visitor(const char *name, const FileInfo *info,
                                             void *context) {
    ZfStorageRemoveSuffixContext *remove_context = context;
    size_t name_len = 0U;
    size_t suffix_len = 0U;

    if (!remove_context || !name || !info) {
        return false;
    }
    if (file_info_is_dir(info)) {
        return true;
    }
    name_len = strlen(name);
    suffix_len = strlen(remove_context->suffix);
    if (name_len < suffix_len ||
        memcmp(name + name_len - suffix_len, remove_context->suffix, suffix_len) != 0) {
        return true;
    }
    if (!zf_storage_build_child_path(remove_context->dir_path, name, remove_context->path_buffer,
                                     remove_context->path_buffer_size)) {
        return false;
    }
    return zf_storage_remove_optional(remove_context->storage, remove_context->path_buffer);
}

bool zf_storage_remove_dir_entries_with_suffix(Storage *storage, const char *dir_path,
                                               const char *suffix, char *name_buffer,
                                               size_t name_buffer_size, char *path_buffer,
                                               size_t path_buffer_size) {
    ZfStorageRemoveSuffixContext context = {
        .storage = storage,
        .dir_path = dir_path,
        .suffix = suffix,
        .path_buffer = path_buffer,
        .path_buffer_size = path_buffer_size,
    };

    if (!suffix || suffix[0] == '\0' || !path_buffer || path_buffer_size == 0U) {
        return false;
    }
    return zf_storage_for_each_dir_entry(storage, dir_path, name_buffer, name_buffer_size,
                                         zf_storage_remove_suffix_visitor, &context);
}

static bool zf_storage_build_suffixed_path(const char *path, const char *suffix, char *out,
                                           size_t out_size) {
    size_t path_len = 0U;
    size_t suffix_len = 0U;

    if (!path || !suffix || !out || out_size == 0U) {
        return false;
    }
    path_len = strlen(path);
    suffix_len = strlen(suffix);
    if (path_len == 0U || suffix_len >= out_size || path_len > out_size - suffix_len - 1U) {
        return false;
    }
    memcpy(out, path, path_len);
    memcpy(out + path_len, suffix, suffix_len);
    out[path_len + suffix_len] = '\0';
    return true;
}

bool zf_storage_recover_atomic_file(Storage *storage, const char *path, const char *temp_path) {
    char backup_path[ZF_STORAGE_PATH_BUFFER_SIZE];
    bool primary_exists = false;
    bool backup_exists = false;
    bool ok = true;

    if (!storage || !path || !temp_path ||
        !zf_storage_build_suffixed_path(path, ZF_STORAGE_BACKUP_SUFFIX, backup_path,
                                        sizeof(backup_path))) {
        return false;
    }

    primary_exists = storage_file_exists(storage, path);
    backup_exists = storage_file_exists(storage, backup_path);
    if (!primary_exists && backup_exists) {
        if (storage_common_rename(storage, backup_path, path) != FSE_OK) {
            zf_storage_remove_optional(storage, temp_path);
            return false;
        }
        primary_exists = true;
    }
    if (primary_exists) {
        zf_storage_remove_optional(storage, backup_path);
    }
    zf_storage_remove_optional(storage, temp_path);
    return ok;
}

bool zf_storage_remove_atomic_file(Storage *storage, const char *path, const char *temp_path) {
    char backup_path[ZF_STORAGE_PATH_BUFFER_SIZE];

    if (!storage || !path || !temp_path ||
        !zf_storage_build_suffixed_path(path, ZF_STORAGE_BACKUP_SUFFIX, backup_path,
                                        sizeof(backup_path))) {
        return false;
    }
    return zf_storage_remove_optional(storage, temp_path) &&
           zf_storage_remove_optional(storage, backup_path) &&
           zf_storage_remove_optional(storage, path);
}

static bool zf_storage_prepare_atomic_backup(Storage *storage, const char *path,
                                             const char *backup_path, bool *backup_active) {
    if (!storage || !path || !backup_path || !backup_active) {
        return false;
    }
    *backup_active = false;
    if (!storage_file_exists(storage, path)) {
        return true;
    }
    if (!zf_storage_remove_optional(storage, backup_path)) {
        return false;
    }
    if (storage_common_copy(storage, path, backup_path) != FSE_OK) {
        return false;
    }
    *backup_active = true;
    return true;
}

static bool zf_storage_publish_atomic_temp(Storage *storage, const char *path,
                                           const char *temp_path) {
    char backup_path[ZF_STORAGE_PATH_BUFFER_SIZE];
    bool backup_active = false;
    bool ok = false;

    if (!zf_storage_build_suffixed_path(path, ZF_STORAGE_BACKUP_SUFFIX, backup_path,
                                        sizeof(backup_path))) {
        zf_storage_remove_optional(storage, temp_path);
        return false;
    }
    if (!zf_storage_prepare_atomic_backup(storage, path, backup_path, &backup_active)) {
        zf_storage_remove_optional(storage, temp_path);
        return false;
    }

    ok = storage_common_rename(storage, temp_path, path) == FSE_OK;
    if (ok) {
        if (backup_active) {
            zf_storage_remove_optional(storage, backup_path);
        }
        storage_common_remove(storage, temp_path);
        return ok;
    }

    zf_storage_recover_atomic_file(storage, path, temp_path);
    return false;
}

bool zf_storage_write_file_atomic(Storage *storage, const char *path, const char *temp_path,
                                  const uint8_t *data, size_t size) {
    bool ok = false;
    File *file = NULL;

    if (!storage || !path || !temp_path || !data || size == 0U) {
        return false;
    }
    if (!zf_storage_recover_atomic_file(storage, path, temp_path)) {
        return false;
    }

    file = storage_file_alloc(storage);
    if (!file) {
        return false;
    }

    storage_common_remove(storage, temp_path);
    if (storage_file_open(file, temp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t written = storage_file_write(file, data, size);
        storage_file_close(file);
        if (written == size) {
            ok = zf_storage_publish_atomic_temp(storage, path, temp_path);
        }
    }
    storage_common_remove(storage, temp_path);
    storage_file_free(file);
    return ok;
}

bool zf_storage_write_format_atomic(Storage *storage, const char *path, const char *temp_path,
                                    ZfStorageFormatWriter writer, void *context) {
    bool ok = false;
    bool opened = false;
    bool wrote = false;
    FlipperFormat *format = NULL;

    if (!storage || !path || !temp_path || !writer) {
        return false;
    }
    if (!zf_storage_recover_atomic_file(storage, path, temp_path)) {
        return false;
    }

    format = flipper_format_file_alloc(storage);
    if (!format) {
        return false;
    }

    storage_common_remove(storage, temp_path);
    if (flipper_format_file_open_always(format, temp_path)) {
        opened = true;
        wrote = writer(format, context);
    }
    if (opened && !flipper_format_file_close(format)) {
        wrote = false;
    }
    flipper_format_free(format);

    if (wrote) {
        ok = zf_storage_publish_atomic_temp(storage, path, temp_path);
    }
    storage_common_remove(storage, temp_path);
    return ok;
}

static bool zf_storage_version_allowed(const ZfStorageEncryptedBlobReadSpec *spec,
                                       uint32_t version) {
    if (!spec || !spec->accepted_versions || spec->accepted_version_count == 0U) {
        return false;
    }
    for (size_t i = 0; i < spec->accepted_version_count; ++i) {
        if (spec->accepted_versions[i] == version) {
            return true;
        }
    }
    return false;
}

static bool zf_storage_write_encrypted_blob_fields(FlipperFormat *format, void *context) {
    ZfStorageEncryptedBlobWriteContext *writer = context;
    const ZfStorageEncryptedBlobWriteSpec *spec = writer ? writer->spec : NULL;

    if (!format || !writer || !spec || !writer->iv || !writer->encrypted) {
        return false;
    }

    bool ok = flipper_format_write_header_cstr(format, spec->file_type, spec->version);
    if (ok && spec->has_type) {
        ok = flipper_format_write_uint32(format, "Type", &spec->type, 1U);
    }
    ok = ok && flipper_format_write_hex(format, "IV", writer->iv, 16U) &&
         flipper_format_write_hex(format, "Data", writer->encrypted, spec->encrypted_len);
    if (ok && writer->mac) {
        ok = flipper_format_write_hex(format, "Mac", writer->mac, writer->mac_len);
    }
    return ok;
}

bool zf_storage_write_encrypted_blob_atomic(Storage *storage, const char *path,
                                            const char *temp_path,
                                            const ZfStorageEncryptedBlobWriteSpec *spec) {
    bool ok = false;
    bool key_loaded = false;
    uint8_t iv[16];
    uint8_t encrypted[ZF_STORAGE_BLOB_MAX_DATA_SIZE];
    uint8_t mac[ZF_STORAGE_BLOB_MAX_MAC_SIZE];
    size_t mac_len = 0U;

    if (!storage || !path || !temp_path || !spec || !spec->file_type || !spec->plaintext ||
        spec->plaintext_len == 0U || spec->plaintext_len > spec->encrypted_len ||
        spec->encrypted_len > sizeof(encrypted) ||
        !furi_hal_crypto_enclave_ensure_key(spec->key_slot)) {
        return false;
    }

    furi_hal_random_fill_buf(iv, sizeof(iv));
    memset(encrypted, 0, sizeof(encrypted));
    memset(mac, 0, sizeof(mac));

    if (!furi_hal_crypto_enclave_load_key(spec->key_slot, iv)) {
        goto cleanup;
    }
    key_loaded = true;
    if (!furi_hal_crypto_encrypt(spec->plaintext, encrypted, spec->plaintext_len)) {
        goto cleanup;
    }
    furi_hal_crypto_enclave_unload_key(spec->key_slot);
    key_loaded = false;

    if (spec->write_mac) {
        if (!spec->write_mac(spec->plaintext, spec->plaintext_len, iv, encrypted,
                             spec->encrypted_len, mac, sizeof(mac), &mac_len,
                             spec->callback_context) ||
            mac_len == 0U || mac_len > sizeof(mac)) {
            goto cleanup;
        }
    }

    ZfStorageEncryptedBlobWriteContext context = {
        .spec = spec,
        .iv = iv,
        .encrypted = encrypted,
        .mac = spec->write_mac ? mac : NULL,
        .mac_len = mac_len,
    };
    ok = zf_storage_write_format_atomic(storage, path, temp_path,
                                        zf_storage_write_encrypted_blob_fields, &context);

cleanup:
    if (key_loaded) {
        furi_hal_crypto_enclave_unload_key(spec->key_slot);
    }
    zf_crypto_secure_zero(iv, sizeof(iv));
    zf_crypto_secure_zero(encrypted, sizeof(encrypted));
    zf_crypto_secure_zero(mac, sizeof(mac));
    return ok;
}

bool zf_storage_read_encrypted_blob(Storage *storage, const char *path,
                                    const ZfStorageEncryptedBlobReadSpec *spec) {
    bool ok = false;
    bool opened = false;
    bool key_loaded = false;
    uint8_t iv[16];
    uint8_t encrypted[ZF_STORAGE_BLOB_MAX_DATA_SIZE];
    uint8_t mac[ZF_STORAGE_BLOB_MAX_MAC_SIZE];
    uint32_t version = 0U;
    uint32_t type = 0U;
    char temp_path[ZF_STORAGE_PATH_BUFFER_SIZE];
    FlipperFormat *format = NULL;
    FuriString *filetype = NULL;

    if (!storage || !path || !spec || !spec->file_type || !spec->accepted_versions ||
        spec->accepted_version_count == 0U || !spec->plaintext || spec->plaintext_len == 0U ||
        spec->plaintext_len > spec->encrypted_len || spec->encrypted_len > sizeof(encrypted) ||
        (spec->has_mac && (spec->mac_len == 0U || spec->mac_len > sizeof(mac))) ||
        !furi_hal_crypto_enclave_ensure_key(spec->key_slot)) {
        return false;
    }

    zf_crypto_secure_zero(spec->plaintext, spec->plaintext_len);
    memset(iv, 0, sizeof(iv));
    memset(encrypted, 0, sizeof(encrypted));
    memset(mac, 0, sizeof(mac));
    if (!zf_storage_build_suffixed_path(path, ZF_STORAGE_TEMP_SUFFIX, temp_path,
                                        sizeof(temp_path)) ||
        !zf_storage_recover_atomic_file(storage, path, temp_path)) {
        goto cleanup;
    }

    format = flipper_format_file_alloc(storage);
    filetype = furi_string_alloc();
    if (!format || !filetype || !flipper_format_file_open_existing(format, path)) {
        goto cleanup;
    }
    opened = true;

    if (!flipper_format_read_header(format, filetype, &version) ||
        strcmp(furi_string_get_cstr(filetype), spec->file_type) != 0 ||
        !zf_storage_version_allowed(spec, version)) {
        goto cleanup;
    }
    if (spec->has_type) {
        if (!flipper_format_read_uint32(format, "Type", &type, 1U)) {
            goto cleanup;
        }
        if (!spec->accept_any_type && type != spec->expected_type) {
            goto cleanup;
        }
    }
    if (!flipper_format_read_hex(format, "IV", iv, sizeof(iv)) ||
        !flipper_format_read_hex(format, "Data", encrypted, spec->encrypted_len) ||
        (spec->has_mac && !flipper_format_read_hex(format, "Mac", mac, spec->mac_len))) {
        goto cleanup;
    }
    if (!furi_hal_crypto_enclave_load_key(spec->key_slot, iv)) {
        goto cleanup;
    }
    key_loaded = true;
    if (!furi_hal_crypto_decrypt(encrypted, spec->plaintext, spec->plaintext_len)) {
        goto cleanup;
    }
    furi_hal_crypto_enclave_unload_key(spec->key_slot);
    key_loaded = false;

    if (spec->verify_mac &&
        !spec->verify_mac(spec->plaintext, spec->plaintext_len, iv, encrypted, spec->encrypted_len,
                          spec->has_mac ? mac : NULL, spec->has_mac ? spec->mac_len : 0U,
                          spec->callback_context)) {
        goto cleanup;
    }
    if (spec->out_version) {
        *spec->out_version = version;
    }
    if (spec->out_type) {
        *spec->out_type = type;
    }
    ok = true;

cleanup:
    if (key_loaded) {
        furi_hal_crypto_enclave_unload_key(spec->key_slot);
    }
    if (format) {
        if (opened) {
            flipper_format_file_close(format);
        }
        flipper_format_free(format);
    }
    if (filetype) {
        furi_string_free(filetype);
    }
    if (!ok) {
        zf_crypto_secure_zero(spec->plaintext, spec->plaintext_len);
    }
    zf_crypto_secure_zero(iv, sizeof(iv));
    zf_crypto_secure_zero(encrypted, sizeof(encrypted));
    zf_crypto_secure_zero(mac, sizeof(mac));
    return ok;
}
