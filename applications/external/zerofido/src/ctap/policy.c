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

#include "policy.h"

#include <string.h>

#include "extensions/cred_protect.h"
#include "parse.h"
#include "../zerofido_app_i.h"
#include "../zerofido_crypto.h"
#include "../zerofido_pin.h"
#include "../zerofido_store.h"

bool zf_ctap_request_uses_allow_list(const ZfGetAssertionRequest *request) {
    return request->allow_list.count > 0;
}

uint8_t zf_ctap_validate_pin_auth_protocol(bool has_pin_auth, bool has_pin_protocol,
                                           uint64_t pin_protocol, bool allow_protocol2) {
    if (!has_pin_auth) {
        return ZF_CTAP_SUCCESS;
    }
    if (!has_pin_protocol) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    if (pin_protocol != ZF_PIN_PROTOCOL_V1 &&
        (pin_protocol != ZF_PIN_PROTOCOL_V2 || !allow_protocol2)) {
        return ZF_CTAP_ERR_INVALID_PARAMETER;
    }
    return ZF_CTAP_SUCCESS;
}

bool zf_ctap_effective_uv_requested(bool has_pin_auth, bool has_uv, bool uv) {
    return !has_pin_auth && has_uv && uv;
}

uint8_t zf_ctap_require_empty_payload(size_t request_len) {
    return request_len == 1 ? ZF_CTAP_SUCCESS : ZF_CTAP_ERR_INVALID_LENGTH;
}

bool zf_ctap_local_maintenance_busy(ZerofidoApp *app) {
    bool busy = false;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    busy = app->maintenance_busy;
    furi_mutex_release(app->ui_mutex);
    return busy;
}

bool zf_ctap_pin_is_set(ZerofidoApp *app) {
    bool pin_is_set = false;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    pin_is_set = zerofido_pin_is_set(&app->pin_state);
    furi_mutex_release(app->ui_mutex);
    return pin_is_set;
}

bool zf_ctap_store_entry_matches_descriptor_list(const ZfCredentialIndexEntry *entry,
                                                 const void *context) {
    const ZfCredentialDescriptorList *list = context;

    return entry && zf_ctap_descriptor_list_contains_id(list, entry->credential_id,
                                                        entry->credential_id_len);
}

/*
 * Exclude-list checks obey credProtect visibility. A matching UV_REQUIRED
 * credential is hidden from makeCredential exclusion unless UV has already been
 * verified, so the authenticator does not leak protected credential existence.
 */
bool zf_ctap_exclude_list_has_visible_match(Storage *storage, const ZfCredentialStore *store,
                                            const char *rp_id,
                                            const ZfCredentialDescriptorList *exclude_list,
                                            bool uv_verified, uint8_t *buffer, size_t buffer_size) {
    uint16_t matches[ZF_MAX_CREDENTIALS];
    size_t match_count = 0;

    if (!store || !store->records || !rp_id || !exclude_list || exclude_list->count == 0) {
        return false;
    }
    if (storage && (!buffer || buffer_size < ZF_STORE_RECORD_IO_SIZE)) {
        return false;
    }

    match_count = zf_store_find_by_rp_filtered(storage, store, rp_id,
                                               zf_ctap_store_entry_matches_descriptor_list,
                                               exclude_list, matches, ZF_MAX_CREDENTIALS);
    for (size_t i = 0; i < match_count; ++i) {
        const ZfCredentialIndexEntry *entry = &store->records[matches[i]];

        if (!uv_verified && entry->cred_protect == ZF_CRED_PROTECT_UV_REQUIRED) {
            continue;
        }
        if (storage) {
            ZfCredentialRecord record = {0};
            bool loaded =
                zf_store_load_record_with_buffer(storage, entry, &record, buffer, buffer_size);
            bool exact_match = loaded && strcmp(record.rp_id, rp_id) == 0;
            zf_crypto_secure_zero(&record, sizeof(record));
            if (loaded && !exact_match) {
                continue;
            }
        }
        return true;
    }

    return false;
}

static bool zf_ctap_credential_is_allowed_by_cred_protect(const ZfGetAssertionRequest *request,
                                                          const ZfCredentialIndexEntry *record,
                                                          bool uv_verified) {
    if (!record) {
        return false;
    }

    return zf_ctap_cred_protect_allows_assertion(record->cred_protect, uv_verified,
                                                 zf_ctap_request_uses_allow_list(request));
}

/*
 * Resolve candidate credentials, then apply credProtect visibility. allowList
 * narrows candidates by descriptor digest; otherwise all RP matches are
 * considered. UV_REQUIRED credentials are hidden unless UV is verified.
 */
size_t zf_ctap_resolve_assertion_matches(Storage *storage, ZfCredentialStore *store,
                                         const ZfGetAssertionRequest *request, bool uv_verified,
                                         uint16_t *match_indices) {
    uint16_t resolved[ZF_MAX_CREDENTIALS];
    size_t resolved_count = 0;
    size_t filtered_count = 0;

    if (zf_ctap_request_uses_allow_list(request)) {
        resolved_count = zf_store_find_by_rp_filtered(
            storage, store, request->assertion.rp_id, zf_ctap_store_entry_matches_descriptor_list,
            &request->allow_list, resolved, ZF_MAX_CREDENTIALS);
    } else {
        resolved_count = zf_store_find_by_rp(storage, store, request->assertion.rp_id, resolved,
                                             ZF_MAX_CREDENTIALS);
    }

    for (size_t i = 0; i < resolved_count; ++i) {
        if (zf_ctap_credential_is_allowed_by_cred_protect(request, &store->records[resolved[i]],
                                                          uv_verified)) {
            match_indices[filtered_count++] = resolved[i];
        }
    }

    return filtered_count;
}
