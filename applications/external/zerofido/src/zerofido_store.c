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

#include "zerofido_store.h"

#include <furi_hal_random.h>
#include <furi_hal_rtc.h>
#include <stdlib.h>
#include <string.h>

#include "ctap/extensions/cred_protect.h"
#include "store/bootstrap.h"
#include "store/record_format.h"
#include "store/recovery.h"
#include "zerofido_crypto.h"

static void zf_store_index_entry_file_name(const ZfCredentialIndexEntry *entry, char *out) {
#ifdef ZF_HOST_TEST
    strncpy(out, entry->file_name, (ZF_CREDENTIAL_ID_LEN * 2) + 1);
    out[ZF_CREDENTIAL_ID_LEN * 2] = '\0';
#else
    zf_store_record_format_hex_encode(entry->credential_id, entry->credential_id_len, out);
#endif
}

/*
 * Production index entries deliberately avoid storing the RP ID in plaintext.
 * Host tests keep plaintext metadata to make protocol expectations readable.
 */
static void zf_store_index_entry_set_rp_id(ZfCredentialIndexEntry *entry, const char *rp_id) {
#ifdef ZF_HOST_TEST
    strncpy(entry->rp_id, rp_id, sizeof(entry->rp_id) - 1);
    entry->rp_id[sizeof(entry->rp_id) - 1] = '\0';
#else
    zf_crypto_sha256((const uint8_t *)rp_id, strlen(rp_id), entry->rp_id_hash);
#endif
}

typedef struct {
#ifdef ZF_HOST_TEST
    const char *rp_id;
#else
    uint8_t rp_id_hash[32];
#endif
} ZfStoreRpMatcher;

/*
 * RP matching is centralized so lookup, deletion, and exclusion checks all use
 * the same hash-vs-plaintext behavior.
 */
static void zf_store_rp_matcher_init(ZfStoreRpMatcher *matcher, const char *rp_id) {
#ifdef ZF_HOST_TEST
    matcher->rp_id = rp_id;
#else
    zf_crypto_sha256((const uint8_t *)rp_id, strlen(rp_id), matcher->rp_id_hash);
#endif
}

static void zf_store_rp_matcher_clear(ZfStoreRpMatcher *matcher) {
#ifndef ZF_HOST_TEST
    zf_crypto_secure_zero(matcher->rp_id_hash, sizeof(matcher->rp_id_hash));
#else
    (void)matcher;
#endif
}

static bool zf_store_index_entry_rp_hash_matches(const ZfCredentialIndexEntry *entry,
                                                 const ZfStoreRpMatcher *matcher) {
#ifdef ZF_HOST_TEST
    return entry && matcher && matcher->rp_id && strcmp(entry->rp_id, matcher->rp_id) == 0;
#else
    return entry && matcher &&
           zf_crypto_constant_time_equal(entry->rp_id_hash, matcher->rp_id_hash,
                                         sizeof(matcher->rp_id_hash));
#endif
}

static bool zf_store_index_entry_matches_rp(Storage *storage, const ZfCredentialStore *store,
                                            const ZfCredentialIndexEntry *entry, const char *rp_id,
                                            const ZfStoreRpMatcher *matcher) {
    (void)storage;
    (void)store;
    (void)rp_id;
    return zf_store_index_entry_rp_hash_matches(entry, matcher);
}

static bool zf_store_index_is_newer(const ZfCredentialIndexEntry *candidate,
                                    const ZfCredentialIndexEntry *other) {
    if (candidate->created_at != other->created_at) {
        return candidate->created_at > other->created_at;
    }

    return memcmp(candidate->credential_id, other->credential_id, ZF_CREDENTIAL_ID_LEN) > 0;
}

static void zf_store_insert_sorted_index(uint16_t *out_indices, size_t *count, size_t max_out,
                                         const ZfCredentialStore *store, uint16_t index) {
    size_t insert_at = *count;

    if (*count >= max_out || !store || !store->records || index >= store->count) {
        return;
    }

    while (insert_at > 0 && zf_store_index_is_newer(&store->records[index],
                                                    &store->records[out_indices[insert_at - 1]])) {
        out_indices[insert_at] = out_indices[insert_at - 1];
        insert_at--;
    }
    out_indices[insert_at] = index;
    (*count)++;
}

/* Keeps the index dense after deletions; callers remove durable files first. */
static void zf_store_compact_after_delete(ZfCredentialStore *store, size_t index) {
    for (size_t j = index + 1; j < store->count; ++j) {
        store->records[j - 1] = store->records[j];
    }
    store->count--;
    memset(&store->records[store->count], 0, sizeof(store->records[store->count]));
}

static size_t zf_store_effective_capacity(const ZfCredentialStore *store) {
    if (!store || !store->records) {
        return 0;
    }
    return store->capacity > 0U ? store->capacity : ZF_MAX_CREDENTIALS;
}

bool zf_store_ensure_capacity(ZfCredentialStore *store, size_t min_capacity) {
    size_t old_capacity = 0U;
    size_t new_capacity = 0U;
    ZfCredentialIndexEntry *records = NULL;

    if (!store || min_capacity > ZF_MAX_CREDENTIALS) {
        return false;
    }
    old_capacity = zf_store_effective_capacity(store);
    if (old_capacity > ZF_MAX_CREDENTIALS) {
        return false;
    }
    if (min_capacity <= old_capacity) {
        return true;
    }
    if (store->records && store->capacity == 0U) {
        return false;
    }

    new_capacity = old_capacity > 0U ? old_capacity : 1U;
    while (new_capacity < min_capacity && new_capacity < ZF_MAX_CREDENTIALS) {
        new_capacity *= 2U;
    }
    if (new_capacity > ZF_MAX_CREDENTIALS) {
        new_capacity = ZF_MAX_CREDENTIALS;
    }
    if (new_capacity < min_capacity) {
        return false;
    }

    records = realloc(store->records, new_capacity * sizeof(*records));
    if (!records) {
        return false;
    }
    memset(&records[old_capacity], 0, (new_capacity - old_capacity) * sizeof(records[0]));
    store->records = records;
    store->capacity = new_capacity;
    return true;
}

void zf_store_index_entry_from_record(const ZfCredentialRecord *record,
                                      ZfCredentialIndexEntry *entry) {
    if (!record || !entry) {
        return;
    }

    memset(entry, 0, sizeof(*entry));
    entry->in_use = record->in_use;
    entry->resident_key = record->resident_key;
#ifdef ZF_HOST_TEST
    memcpy(entry->file_name, record->file_name, sizeof(entry->file_name));
#endif
    memcpy(entry->credential_id, record->credential_id, sizeof(entry->credential_id));
    entry->credential_id_len = record->credential_id_len;
    zf_store_index_entry_set_rp_id(entry, record->rp_id);
#ifdef ZF_HOST_TEST
    memcpy(entry->user_id, record->user_id, sizeof(entry->user_id));
    entry->user_id_len = record->user_id_len;
    memcpy(entry->user_name, record->user_name, sizeof(entry->user_name));
    memcpy(entry->user_display_name, record->user_display_name, sizeof(entry->user_display_name));
#endif
    entry->sign_count = record->sign_count;
    entry->counter_high_water = record->sign_count;
    entry->created_at = record->created_at;
    entry->cred_protect = zf_ctap_cred_protect_effective(record->cred_protect);
}

bool zf_store_init_with_buffer(Storage *storage, ZfCredentialStore *store, uint8_t *buffer,
                               size_t buffer_size) {
    if (!buffer || buffer_size < ZF_STORE_RECORD_IO_SIZE) {
        return false;
    }

    return zf_store_bootstrap_init_with_buffer(storage, store, buffer, buffer_size);
}

void zf_store_deinit(ZfCredentialStore *store) {
    zf_store_clear(store);
}

void zf_store_clear(ZfCredentialStore *store) {
    size_t capacity = zf_store_effective_capacity(store);

    if (!store || !store->records) {
        if (store) {
            store->count = 0;
        }
        return;
    }

    memset(store->records, 0, sizeof(store->records[0]) * capacity);
    store->count = 0;
}

bool zf_store_wipe_app_data(Storage *storage) {
    return zf_store_bootstrap_wipe_app_data(storage);
}

bool zf_store_prepare_credential(ZfCredentialRecord *record, const char *rp_id,
                                 const uint8_t *user_id, size_t user_id_len, const char *user_name,
                                 const char *user_display_name, bool resident_key) {
    if (user_id_len > ZF_MAX_USER_ID_LEN) {
        return false;
    }

    memset(record, 0, sizeof(*record));
    furi_hal_random_fill_buf(record->credential_id, ZF_CREDENTIAL_ID_LEN);
    record->storage_version = ZF_STORE_FORMAT_VERSION;
    record->credential_id_len = ZF_CREDENTIAL_ID_LEN;
    zf_store_record_format_hex_encode(record->credential_id, record->credential_id_len,
                                      record->file_name);

    strncpy(record->rp_id, rp_id, sizeof(record->rp_id) - 1);
    memcpy(record->user_id, user_id, user_id_len);
    record->user_id_len = user_id_len;
    if (user_name) {
        strncpy(record->user_name, user_name, sizeof(record->user_name) - 1);
    }
    if (user_display_name) {
        strncpy(record->user_display_name, user_display_name,
                sizeof(record->user_display_name) - 1);
    }

    record->created_at = furi_hal_rtc_get_timestamp();
    record->resident_key = resident_key;
    record->cred_protect = ZF_CRED_PROTECT_UV_OPTIONAL;
    record->hmac_secret = true;
    furi_hal_random_fill_buf(record->hmac_secret_without_uv,
                             sizeof(record->hmac_secret_without_uv));
    furi_hal_random_fill_buf(record->hmac_secret_with_uv, sizeof(record->hmac_secret_with_uv));
    record->in_use = true;
    return true;
}

/*
 * Write-before-publish protects the live index from pointing at a missing or
 * partially written credential file.
 */
bool zf_store_add_record_with_buffer(Storage *storage, ZfCredentialStore *store,
                                     const ZfCredentialRecord *record, uint8_t *buffer,
                                     size_t buffer_size) {
    if (!store || !record || !buffer || buffer_size < ZF_STORE_RECORD_IO_SIZE ||
        store->count >= ZF_MAX_CREDENTIALS) {
        return false;
    }
    if (!zf_store_ensure_capacity(store, store->count + 1U)) {
        return false;
    }
    if (!zf_store_record_format_write_record_with_buffer(storage, record, buffer, buffer_size)) {
        return false;
    }

    zf_store_index_entry_from_record(record, &store->records[store->count]);
    store->count++;
    return true;
}

bool zf_store_write_record_file_with_buffer(Storage *storage, const ZfCredentialRecord *record,
                                            uint8_t *buffer, size_t buffer_size) {
    return zf_store_record_format_write_record_with_buffer(storage, record, buffer, buffer_size);
}

bool zf_store_remove_record_file(Storage *storage, const ZfCredentialRecord *record) {
    char file_name[ZF_CREDENTIAL_ID_LEN * 2 + 1];

    if (!record || !record->in_use) {
        return false;
    }
    if (record->file_name[0] != '\0') {
        strncpy(file_name, record->file_name, sizeof(file_name) - 1);
        file_name[sizeof(file_name) - 1] = '\0';
    } else {
        zf_store_record_format_hex_encode(record->credential_id, record->credential_id_len,
                                          file_name);
    }
    return zf_store_recovery_remove_record_paths(storage, file_name);
}

bool zf_store_publish_added_record(ZfCredentialStore *store, const ZfCredentialRecord *record) {
    if (!store || !record || store->count >= ZF_MAX_CREDENTIALS) {
        return false;
    }
    if (!zf_store_ensure_capacity(store, store->count + 1U)) {
        return false;
    }

    zf_store_index_entry_from_record(record, &store->records[store->count]);
    store->count++;
    return true;
}

bool zf_store_update_record_with_buffer(Storage *storage, ZfCredentialStore *store,
                                        const ZfCredentialRecord *record, uint8_t *buffer,
                                        size_t buffer_size) {
    ZfCredentialRecord record_to_write;

    if (!store || !store->records || !record || !buffer || buffer_size < ZF_STORE_RECORD_IO_SIZE) {
        return false;
    }

    for (size_t i = 0; i < store->count; ++i) {
        if (!store->records[i].in_use) {
            continue;
        }
        if (store->records[i].credential_id_len != record->credential_id_len ||
            memcmp(store->records[i].credential_id, record->credential_id,
                   record->credential_id_len) != 0) {
            continue;
        }
        uint32_t counter_high_water = store->records[i].counter_high_water;
        if (record->sign_count < store->records[i].sign_count) {
            return false;
        }
        if (record->sign_count > counter_high_water &&
            !zf_store_record_format_reserve_counter_with_buffer(storage, record, buffer,
                                                                buffer_size, &counter_high_water)) {
            return false;
        }
        record_to_write = *record;
        if (counter_high_water > record_to_write.sign_count) {
            record_to_write.sign_count = counter_high_water;
        }
        if (!zf_store_record_format_write_record_with_buffer(storage, &record_to_write, buffer,
                                                             buffer_size)) {
            zf_crypto_secure_zero(&record_to_write, sizeof(record_to_write));
            return false;
        }
        zf_crypto_secure_zero(&record_to_write, sizeof(record_to_write));

        zf_store_index_entry_from_record(record, &store->records[i]);
        store->records[i].counter_high_water = counter_high_water;
        return true;
    }

    return false;
}

typedef bool (*ZfStoreRecordLoader)(Storage *storage, const char *file_name,
                                    ZfCredentialRecord *out_record, uint8_t *buffer,
                                    size_t buffer_size);

/*
 * Full-record loads share the same index-to-file lookup. Host tests can fall
 * back to index data because their fake storage often exercises policy without
 * serializing every record fixture.
 */
static bool zf_store_load_record_internal_with_buffer(Storage *storage,
                                                      const ZfCredentialIndexEntry *entry,
                                                      ZfCredentialRecord *out_record,
                                                      uint8_t *buffer, size_t buffer_size,
                                                      ZfStoreRecordLoader loader) {
    char file_name[ZF_CREDENTIAL_ID_LEN * 2 + 1];

    if (!entry || !entry->in_use || !out_record || !buffer || !loader ||
        buffer_size < ZF_STORE_RECORD_IO_SIZE) {
        return false;
    }

    zf_store_index_entry_file_name(entry, file_name);
    bool loaded = loader(storage, file_name, out_record, buffer, buffer_size);
    if (loaded) {
        return true;
    }

#ifdef ZF_HOST_TEST
    memset(out_record, 0, sizeof(*out_record));
    out_record->in_use = entry->in_use;
    out_record->resident_key = entry->resident_key;
    strncpy(out_record->file_name, file_name, sizeof(out_record->file_name) - 1);
    out_record->file_name[sizeof(out_record->file_name) - 1] = '\0';
    memcpy(out_record->credential_id, entry->credential_id, sizeof(out_record->credential_id));
    out_record->credential_id_len = entry->credential_id_len;
    memcpy(out_record->rp_id, entry->rp_id, sizeof(out_record->rp_id));
    memcpy(out_record->user_id, entry->user_id, sizeof(out_record->user_id));
    out_record->user_id_len = entry->user_id_len;
    memcpy(out_record->user_name, entry->user_name, sizeof(out_record->user_name));
    memcpy(out_record->user_display_name, entry->user_display_name,
           sizeof(out_record->user_display_name));
    out_record->sign_count = entry->sign_count;
    out_record->created_at = entry->created_at;
    out_record->cred_protect = entry->cred_protect;
    return true;
#else
    return false;
#endif
}

bool zf_store_load_record_with_buffer(Storage *storage, const ZfCredentialIndexEntry *entry,
                                      ZfCredentialRecord *out_record, uint8_t *buffer,
                                      size_t buffer_size) {
    return zf_store_load_record_internal_with_buffer(
        storage, entry, out_record, buffer, buffer_size,
        zf_store_record_format_load_record_with_buffer);
}

bool zf_store_load_record_for_display_with_buffer(Storage *storage,
                                                  const ZfCredentialIndexEntry *entry,
                                                  ZfCredentialRecord *out_record, uint8_t *buffer,
                                                  size_t buffer_size) {
    return zf_store_load_record_internal_with_buffer(
        storage, entry, out_record, buffer, buffer_size,
        zf_store_record_format_load_record_for_display_with_buffer);
}

bool zf_store_load_record_by_index_with_buffer(Storage *storage, const ZfCredentialStore *store,
                                               size_t index, ZfCredentialRecord *out_record,
                                               uint8_t *buffer, size_t buffer_size) {
    if (!store || !store->records || index >= store->count) {
        return false;
    }

    if (!zf_store_load_record_with_buffer(storage, &store->records[index], out_record, buffer,
                                          buffer_size)) {
        return false;
    }
    out_record->sign_count = store->records[index].sign_count;
    return true;
}

bool zf_store_load_record_by_index_for_rp_with_buffer(Storage *storage,
                                                      const ZfCredentialStore *store, size_t index,
                                                      const char *rp_id,
                                                      ZfCredentialRecord *out_record,
                                                      uint8_t *buffer, size_t buffer_size) {
    if (!rp_id || !zf_store_load_record_by_index_with_buffer(storage, store, index, out_record,
                                                             buffer, buffer_size)) {
        return false;
    }
    if (strcmp(out_record->rp_id, rp_id) != 0) {
        zf_crypto_secure_zero(out_record, sizeof(*out_record));
        return false;
    }
    return true;
}

bool zf_store_advance_counter(Storage *storage, ZfCredentialStore *store,
                              const ZfCredentialRecord *record) {
    uint8_t buffer[ZF_STORE_RECORD_IO_SIZE];
    uint32_t counter_high_water = 0;
    bool ok = false;

    if (!store || !store->records || !record) {
        return false;
    }

    for (size_t i = 0; i < store->count; ++i) {
        ZfCredentialIndexEntry *entry = &store->records[i];

        if (!entry->in_use || entry->credential_id_len != record->credential_id_len ||
            memcmp(entry->credential_id, record->credential_id, record->credential_id_len) != 0) {
            continue;
        }

        if (!zf_store_prepare_counter_advance(storage, entry, record, buffer, sizeof(buffer),
                                              &counter_high_water)) {
            goto cleanup;
        }
        ok = zf_store_publish_counter_advance(store, record, counter_high_water);
        goto cleanup;
    }

cleanup:
    zf_crypto_secure_zero(buffer, sizeof(buffer));
    return ok;
}

bool zf_store_prepare_counter_advance(Storage *storage, const ZfCredentialIndexEntry *entry,
                                      const ZfCredentialRecord *record, uint8_t *buffer,
                                      size_t buffer_size, uint32_t *out_counter_high_water) {
    uint32_t counter_high_water = 0;

    if (!entry || !record || !buffer || buffer_size < ZF_STORE_RECORD_IO_SIZE ||
        !out_counter_high_water || record->sign_count < entry->sign_count) {
        return false;
    }
    counter_high_water = entry->counter_high_water;
    if (record->sign_count > counter_high_water) {
        if (!zf_store_record_format_reserve_counter_with_buffer(storage, record, buffer,
                                                                buffer_size, &counter_high_water)) {
            return false;
        }
    }

    *out_counter_high_water = counter_high_water;
    return true;
}

bool zf_store_publish_counter_advance(ZfCredentialStore *store, const ZfCredentialRecord *record,
                                      uint32_t counter_high_water) {
    if (!store || !store->records || !record || counter_high_water < record->sign_count) {
        return false;
    }

    for (size_t i = 0; i < store->count; ++i) {
        ZfCredentialIndexEntry *entry = &store->records[i];

        if (!entry->in_use || entry->credential_id_len != record->credential_id_len ||
            memcmp(entry->credential_id, record->credential_id, record->credential_id_len) != 0) {
            continue;
        }

        if (record->sign_count < entry->sign_count) {
            return false;
        }
        if (record->sign_count > entry->counter_high_water) {
            entry->counter_high_water = counter_high_water;
        }
        entry->sign_count = record->sign_count;
        return true;
    }

    return false;
}

bool zf_store_delete_resident_credentials_for_user_with_buffer(
    Storage *storage, ZfCredentialStore *store, const char *rp_id, const uint8_t *user_id,
    size_t user_id_len, size_t *deleted_count, uint8_t *buffer, size_t buffer_size) {
    size_t removed = 0;
    ZfCredentialRecord record;
    ZfStoreRpMatcher matcher;

    if (!store || !store->records || !rp_id || !buffer || buffer_size < ZF_STORE_RECORD_IO_SIZE) {
        if (deleted_count) {
            *deleted_count = 0;
        }
        return !store || !store->records || !rp_id;
    }

    zf_store_rp_matcher_init(&matcher, rp_id);

    for (size_t i = 0; i < store->count;) {
        const ZfCredentialIndexEntry *entry = &store->records[i];

        if (!entry->in_use || !entry->resident_key ||
            !zf_store_index_entry_rp_hash_matches(entry, &matcher) ||
            !zf_store_load_record_with_buffer(storage, entry, &record, buffer, buffer_size) ||
            strcmp(record.rp_id, rp_id) != 0 || record.user_id_len != user_id_len ||
            memcmp(record.user_id, user_id, user_id_len) != 0) {
            ++i;
            continue;
        }

        char file_name[ZF_CREDENTIAL_ID_LEN * 2 + 1];
        zf_store_index_entry_file_name(entry, file_name);
        if (!zf_store_recovery_remove_record_paths(storage, file_name)) {
            zf_store_rp_matcher_clear(&matcher);
            return false;
        }

        zf_store_compact_after_delete(store, i);
        ++removed;
    }

    if (deleted_count) {
        *deleted_count = removed;
    }
    zf_store_rp_matcher_clear(&matcher);
    return true;
}

bool zf_store_remove_resident_credential_files_for_user_with_buffer(
    Storage *storage, const ZfCredentialStore *store, const char *rp_id, const uint8_t *user_id,
    size_t user_id_len, uint16_t *deleted_indices, size_t max_deleted, size_t *deleted_count,
    uint8_t *buffer, size_t buffer_size) {
    size_t found_count = 0;

    if (!zf_store_find_resident_credential_indices_for_user_with_buffer(
            storage, store, rp_id, user_id, user_id_len, deleted_indices, max_deleted, &found_count,
            buffer, buffer_size)) {
        if (deleted_count) {
            *deleted_count = found_count;
        }
        return false;
    }
    return zf_store_remove_credential_files_by_indices(storage, store, deleted_indices, found_count,
                                                       deleted_count);
}

bool zf_store_find_resident_credential_indices_for_user_with_buffer(
    Storage *storage, const ZfCredentialStore *store, const char *rp_id, const uint8_t *user_id,
    size_t user_id_len, uint16_t *deleted_indices, size_t max_deleted, size_t *deleted_count,
    uint8_t *buffer, size_t buffer_size) {
    size_t removed = 0;
    ZfCredentialRecord record = {0};
    ZfStoreRpMatcher matcher;

    if (deleted_count) {
        *deleted_count = 0;
    }
    if (!store || !store->records || !rp_id || !deleted_indices || max_deleted == 0) {
        return true;
    }
    if (!buffer || buffer_size < ZF_STORE_RECORD_IO_SIZE) {
        return false;
    }

    zf_store_rp_matcher_init(&matcher, rp_id);

    for (size_t i = 0; i < store->count; ++i) {
        const ZfCredentialIndexEntry *entry = &store->records[i];
        bool matches = false;

        if (!entry->in_use || !entry->resident_key ||
            !zf_store_index_entry_rp_hash_matches(entry, &matcher)) {
            continue;
        }
        if (zf_store_load_record_with_buffer(storage, entry, &record, buffer, buffer_size)) {
            matches = strcmp(record.rp_id, rp_id) == 0 && record.user_id_len == user_id_len &&
                      memcmp(record.user_id, user_id, user_id_len) == 0;
        }
        zf_crypto_secure_zero(&record, sizeof(record));
        if (!matches) {
            continue;
        }
        if (removed >= max_deleted) {
            if (deleted_count) {
                *deleted_count = removed;
            }
            zf_store_rp_matcher_clear(&matcher);
            return false;
        }

        deleted_indices[removed++] = (uint16_t)i;
    }

    if (deleted_count) {
        *deleted_count = removed;
    }
    zf_store_rp_matcher_clear(&matcher);
    return true;
}

/*
 * Removes files only; the in-memory array is left untouched until
 * zf_store_publish_deleted_indices compacts it.
 */
bool zf_store_remove_credential_files_by_indices(Storage *storage, const ZfCredentialStore *store,
                                                 const uint16_t *deleted_indices,
                                                 size_t deleted_count, size_t *removed_count) {
    size_t removed = 0;

    if (removed_count) {
        *removed_count = 0;
    }
    if (!store || !store->records || (!deleted_indices && deleted_count > 0)) {
        return deleted_count == 0;
    }

    for (size_t i = 0; i < deleted_count; ++i) {
        size_t index = deleted_indices[i];
        char file_name[ZF_CREDENTIAL_ID_LEN * 2 + 1];

        if (index >= store->count || !store->records[index].in_use) {
            if (removed_count) {
                *removed_count = removed;
            }
            return false;
        }
        zf_store_index_entry_file_name(&store->records[index], file_name);
        if (!zf_store_recovery_remove_record_paths(storage, file_name)) {
            if (removed_count) {
                *removed_count = removed;
            }
            return false;
        }
        removed++;
    }

    if (removed_count) {
        *removed_count = removed;
    }
    return true;
}

void zf_store_publish_deleted_indices(ZfCredentialStore *store, const uint16_t *deleted_indices,
                                      size_t deleted_count) {
    if (!store || !store->records || !deleted_indices) {
        return;
    }

    while (deleted_count > 0) {
        size_t index = deleted_indices[deleted_count - 1U];
        if (index < store->count) {
            zf_store_compact_after_delete(store, index);
        }
        deleted_count--;
    }
}

/* UI/manual deletion path: find by credential ID, remove files, then compact. */
ZfStoreDeleteResult zf_store_delete_record(Storage *storage, ZfCredentialStore *store,
                                           const uint8_t *credential_id, size_t credential_id_len) {
    if (!store || !store->records) {
        return ZfStoreDeleteNotFound;
    }

    for (size_t i = 0; i < store->count; ++i) {
        const ZfCredentialIndexEntry *entry = &store->records[i];

        if (!entry->in_use || entry->credential_id_len != credential_id_len ||
            memcmp(entry->credential_id, credential_id, credential_id_len) != 0) {
            continue;
        }
        char file_name[ZF_CREDENTIAL_ID_LEN * 2 + 1];
        zf_store_index_entry_file_name(entry, file_name);
        if (!zf_store_recovery_remove_record_paths(storage, file_name)) {
            return ZfStoreDeleteRemoveFailed;
        }

        zf_store_compact_after_delete(store, i);
        return ZfStoreDeleteOk;
    }

    return ZfStoreDeleteNotFound;
}

bool zf_store_find_index_by_id(const ZfCredentialStore *store, const uint8_t *credential_id,
                               size_t credential_id_len, size_t *out_index) {
    if (!store || !store->records) {
        return false;
    }

    for (size_t i = 0; i < store->count; ++i) {
        const ZfCredentialIndexEntry *entry = &store->records[i];

        if (!entry->in_use) {
            continue;
        }
        if (entry->credential_id_len == credential_id_len &&
            memcmp(entry->credential_id, credential_id, credential_id_len) == 0) {
            if (out_index) {
                *out_index = i;
            }
            return true;
        }
    }

    return false;
}

size_t zf_store_count_saved(const ZfCredentialStore *store) {
    size_t count = 0;

    if (!store || !store->records) {
        return 0;
    }

    for (size_t i = 0; i < store->count; ++i) {
        if (store->records[i].in_use) {
            ++count;
        }
    }

    return count;
}

size_t zf_store_count_resident(const ZfCredentialStore *store) {
    size_t count = 0;

    if (!store || !store->records) {
        return 0;
    }

    for (size_t i = 0; i < store->count; ++i) {
        if (store->records[i].in_use && store->records[i].resident_key) {
            ++count;
        }
    }

    return count;
}

size_t zf_store_find_by_rp(Storage *storage, const ZfCredentialStore *store, const char *rp_id,
                           uint16_t *out_indices, size_t max_out) {
    size_t count = 0;
    ZfStoreRpMatcher matcher;

    if (!store || !store->records || !rp_id) {
        return 0;
    }

    zf_store_rp_matcher_init(&matcher, rp_id);

    for (size_t i = 0; i < store->count; ++i) {
        const ZfCredentialIndexEntry *entry = &store->records[i];

        if (entry->in_use && entry->resident_key &&
            zf_store_index_entry_matches_rp(storage, store, entry, rp_id, &matcher)) {
            zf_store_insert_sorted_index(out_indices, &count, max_out, store, (uint16_t)i);
        }
    }

    zf_store_rp_matcher_clear(&matcher);
    return count;
}

size_t zf_store_find_by_rp_filtered(Storage *storage, const ZfCredentialStore *store,
                                    const char *rp_id, ZfStoreCredentialFilter filter,
                                    const void *filter_context, uint16_t *out_indices,
                                    size_t max_out) {
    size_t count = 0;
    ZfStoreRpMatcher matcher;

    if (!store || !store->records || !rp_id) {
        return 0;
    }

    zf_store_rp_matcher_init(&matcher, rp_id);

    for (size_t i = 0; i < store->count; ++i) {
        const ZfCredentialIndexEntry *entry = &store->records[i];

        if (!entry->in_use ||
            !zf_store_index_entry_matches_rp(storage, store, entry, rp_id, &matcher) ||
            (filter && !filter(entry, filter_context))) {
            continue;
        }

        zf_store_insert_sorted_index(out_indices, &count, max_out, store, (uint16_t)i);
    }

    zf_store_rp_matcher_clear(&matcher);
    return count;
}

/*
 * Fast existence check for makeCredential exclude-list and assertion policy.
 * When storage is available it reloads candidate records to confirm the RP ID
 * string, not only the index hash.
 */
bool zf_store_has_matching_credential_with_buffer(Storage *storage, const ZfCredentialStore *store,
                                                  const char *rp_id, ZfStoreCredentialFilter filter,
                                                  const void *filter_context, uint8_t *buffer,
                                                  size_t buffer_size) {
    ZfStoreRpMatcher matcher;

    if (!store || !store->records || !rp_id ||
        (storage && (!buffer || buffer_size < ZF_STORE_RECORD_IO_SIZE))) {
        return false;
    }

    zf_store_rp_matcher_init(&matcher, rp_id);

    for (size_t i = 0; i < store->count; ++i) {
        const ZfCredentialIndexEntry *entry = &store->records[i];

        if (!entry->in_use ||
            !zf_store_index_entry_matches_rp(storage, store, entry, rp_id, &matcher) ||
            (filter && !filter(entry, filter_context))) {
            continue;
        }

        if (storage) {
            ZfCredentialRecord record = {0};
            bool loaded =
                zf_store_load_record_with_buffer(storage, entry, &record, buffer, buffer_size);
            if (loaded) {
                bool exact_match = strcmp(record.rp_id, rp_id) == 0;
                zf_crypto_secure_zero(&record, sizeof(record));
                if (!exact_match) {
                    continue;
                }
            }
        }
        zf_store_rp_matcher_clear(&matcher);
        return true;
    }

    zf_store_rp_matcher_clear(&matcher);
    return false;
}
