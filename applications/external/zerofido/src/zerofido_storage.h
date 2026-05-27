/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

#include "zerofido_types.h"

typedef bool (*ZfStorageFormatWriter)(FlipperFormat *format, void *context);
typedef bool (*ZfStorageDirEntryVisitor)(const char *name, const FileInfo *info, void *context);
typedef bool (*ZfStorageEncryptedBlobMacWriter)(const uint8_t *plaintext, size_t plaintext_len,
                                                const uint8_t iv[16], const uint8_t *encrypted,
                                                size_t encrypted_len, uint8_t *mac,
                                                size_t mac_capacity, size_t *mac_len,
                                                void *context);
typedef bool (*ZfStorageEncryptedBlobMacVerifier)(const uint8_t *plaintext, size_t plaintext_len,
                                                  const uint8_t iv[16], const uint8_t *encrypted,
                                                  size_t encrypted_len, const uint8_t *mac,
                                                  size_t mac_len, void *context);
typedef struct {
    const char *file_type;
    uint32_t version;
    bool has_type;
    uint32_t type;
    uint8_t key_slot;
    const uint8_t *plaintext;
    size_t plaintext_len;
    size_t encrypted_len;
    ZfStorageEncryptedBlobMacWriter write_mac;
    void *callback_context;
} ZfStorageEncryptedBlobWriteSpec;

typedef struct {
    const char *file_type;
    const uint32_t *accepted_versions;
    size_t accepted_version_count;
    bool has_type;
    bool accept_any_type;
    uint32_t expected_type;
    uint8_t key_slot;
    uint8_t *plaintext;
    size_t plaintext_len;
    size_t encrypted_len;
    bool has_mac;
    size_t mac_len;
    ZfStorageEncryptedBlobMacVerifier verify_mac;
    void *callback_context;
    uint32_t *out_version;
    uint32_t *out_type;
} ZfStorageEncryptedBlobReadSpec;

bool zf_storage_ensure_dir(Storage *storage, const char *path);
bool zf_storage_ensure_app_data_dir(Storage *storage);
bool zf_storage_remove_optional(Storage *storage, const char *path);
bool zf_storage_remove_optional_paths(Storage *storage, const char *const *paths, size_t count);
bool zf_storage_build_child_path(const char *dir_path, const char *name, char *path,
                                 size_t path_size);
bool zf_storage_read_file(Storage *storage, const char *path, uint8_t *data, size_t capacity,
                          size_t *out_size);
bool zf_storage_for_each_dir_entry(Storage *storage, const char *dir_path, char *name_buffer,
                                   size_t name_buffer_size, ZfStorageDirEntryVisitor visitor,
                                   void *context);
bool zf_storage_remove_dir_entries_with_suffix(Storage *storage, const char *dir_path,
                                               const char *suffix, char *name_buffer,
                                               size_t name_buffer_size, char *path_buffer,
                                               size_t path_buffer_size);
bool zf_storage_recover_atomic_file(Storage *storage, const char *path, const char *temp_path);
bool zf_storage_remove_atomic_file(Storage *storage, const char *path, const char *temp_path);
bool zf_storage_write_file_atomic(Storage *storage, const char *path, const char *temp_path,
                                  const uint8_t *data, size_t size);
bool zf_storage_write_format_atomic(Storage *storage, const char *path, const char *temp_path,
                                    ZfStorageFormatWriter writer, void *context);
bool zf_storage_write_encrypted_blob_atomic(Storage *storage, const char *path,
                                            const char *temp_path,
                                            const ZfStorageEncryptedBlobWriteSpec *spec);
bool zf_storage_read_encrypted_blob(Storage *storage, const char *path,
                                    const ZfStorageEncryptedBlobReadSpec *spec);
