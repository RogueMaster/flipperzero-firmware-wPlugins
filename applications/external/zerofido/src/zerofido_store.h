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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

#include "zerofido_types.h"

typedef enum {
    ZfStoreDeleteOk = 0,
    ZfStoreDeleteNotFound,
    ZfStoreDeleteRemoveFailed,
} ZfStoreDeleteResult;

typedef bool (*ZfStoreCredentialFilter)(const ZfCredentialIndexEntry *entry, const void *context);

#define ZF_STORE_RECORD_IO_SIZE 800

/*
 * The store keeps a compact in-memory index and loads full credential records
 * from storage only when an operation needs private keys, user IDs, or exact RP
 * strings. All APIs that touch record files accept caller-owned scratch buffers
 * so CTAP handlers can stay within fixed stack and arena limits.
 */
bool zf_store_init_with_buffer(Storage *storage, ZfCredentialStore *store, uint8_t *buffer,
                               size_t buffer_size);
bool zf_store_ensure_capacity(ZfCredentialStore *store, size_t min_capacity);
void zf_store_deinit(ZfCredentialStore *store);
void zf_store_clear(ZfCredentialStore *store);
void zf_store_index_entry_from_record(const ZfCredentialRecord *record,
                                      ZfCredentialIndexEntry *entry);
bool zf_store_wipe_app_data(Storage *storage);

/*
 * Credential creation is split into preparation, durable write, and index
 * publication. That lets makeCredential finish file I/O before exposing the
 * record in the live index.
 */
bool zf_store_prepare_credential(ZfCredentialRecord *record, const char *rp_id,
                                 const uint8_t *user_id, size_t user_id_len, const char *user_name,
                                 const char *user_display_name, bool resident_key);
bool zf_store_add_record_with_buffer(Storage *storage, ZfCredentialStore *store,
                                     const ZfCredentialRecord *record, uint8_t *buffer,
                                     size_t buffer_size);
bool zf_store_write_record_file_with_buffer(Storage *storage, const ZfCredentialRecord *record,
                                            uint8_t *buffer, size_t buffer_size);
bool zf_store_remove_record_file(Storage *storage, const ZfCredentialRecord *record);
bool zf_store_publish_added_record(ZfCredentialStore *store, const ZfCredentialRecord *record);
bool zf_store_update_record_with_buffer(Storage *storage, ZfCredentialStore *store,
                                        const ZfCredentialRecord *record, uint8_t *buffer,
                                        size_t buffer_size);
bool zf_store_load_record_with_buffer(Storage *storage, const ZfCredentialIndexEntry *entry,
                                      ZfCredentialRecord *out_record, uint8_t *buffer,
                                      size_t buffer_size);
bool zf_store_load_record_for_display_with_buffer(Storage *storage,
                                                  const ZfCredentialIndexEntry *entry,
                                                  ZfCredentialRecord *out_record, uint8_t *buffer,
                                                  size_t buffer_size);
bool zf_store_load_record_by_index_with_buffer(Storage *storage, const ZfCredentialStore *store,
                                               size_t index, ZfCredentialRecord *out_record,
                                               uint8_t *buffer, size_t buffer_size);
bool zf_store_load_record_by_index_for_rp_with_buffer(Storage *storage,
                                                      const ZfCredentialStore *store, size_t index,
                                                      const char *rp_id,
                                                      ZfCredentialRecord *out_record,
                                                      uint8_t *buffer, size_t buffer_size);

/*
 * Signature counters are monotonic across power loss. The high-water value is
 * reserved in the record file before the CTAP/U2F response is published, then
 * mirrored into the index after the response path succeeds.
 */
bool zf_store_advance_counter(Storage *storage, ZfCredentialStore *store,
                              const ZfCredentialRecord *record);
bool zf_store_prepare_counter_advance(Storage *storage, const ZfCredentialIndexEntry *entry,
                                      const ZfCredentialRecord *record, uint8_t *buffer,
                                      size_t buffer_size, uint32_t *out_counter_high_water);
bool zf_store_publish_counter_advance(ZfCredentialStore *store, const ZfCredentialRecord *record,
                                      uint32_t counter_high_water);

/*
 * Deletion helpers separate file removal from index compaction. Callers that
 * need all-or-nothing behavior first collect indices, remove files, then publish
 * deletions in descending index order.
 */
bool zf_store_delete_resident_credentials_for_user_with_buffer(
    Storage *storage, ZfCredentialStore *store, const char *rp_id, const uint8_t *user_id,
    size_t user_id_len, size_t *deleted_count, uint8_t *buffer, size_t buffer_size);
bool zf_store_remove_resident_credential_files_for_user_with_buffer(
    Storage *storage, const ZfCredentialStore *store, const char *rp_id, const uint8_t *user_id,
    size_t user_id_len, uint16_t *deleted_indices, size_t max_deleted, size_t *deleted_count,
    uint8_t *buffer, size_t buffer_size);
bool zf_store_find_resident_credential_indices_for_user_with_buffer(
    Storage *storage, const ZfCredentialStore *store, const char *rp_id, const uint8_t *user_id,
    size_t user_id_len, uint16_t *deleted_indices, size_t max_deleted, size_t *deleted_count,
    uint8_t *buffer, size_t buffer_size);
bool zf_store_remove_credential_files_by_indices(Storage *storage, const ZfCredentialStore *store,
                                                 const uint16_t *deleted_indices,
                                                 size_t deleted_count, size_t *removed_count);
void zf_store_publish_deleted_indices(ZfCredentialStore *store, const uint16_t *deleted_indices,
                                      size_t deleted_count);
ZfStoreDeleteResult zf_store_delete_record(Storage *storage, ZfCredentialStore *store,
                                           const uint8_t *credential_id, size_t credential_id_len);
bool zf_store_find_index_by_id(const ZfCredentialStore *store, const uint8_t *credential_id,
                               size_t credential_id_len, size_t *out_index);
size_t zf_store_count_saved(const ZfCredentialStore *store);
size_t zf_store_count_resident(const ZfCredentialStore *store);

/*
 * RP lookup returns newest credentials first. Production builds compare RP ID
 * hashes from the index and reload full records only when an exact-string check
 * is needed to rule out hash or stale-index ambiguity.
 */
size_t zf_store_find_by_rp(Storage *storage, const ZfCredentialStore *store, const char *rp_id,
                           uint16_t *out_indices, size_t max_out);
size_t zf_store_find_by_rp_filtered(Storage *storage, const ZfCredentialStore *store,
                                    const char *rp_id, ZfStoreCredentialFilter filter,
                                    const void *filter_context, uint16_t *out_indices,
                                    size_t max_out);
bool zf_store_has_matching_credential_with_buffer(Storage *storage, const ZfCredentialStore *store,
                                                  const char *rp_id, ZfStoreCredentialFilter filter,
                                                  const void *filter_context, uint8_t *buffer,
                                                  size_t buffer_size);
