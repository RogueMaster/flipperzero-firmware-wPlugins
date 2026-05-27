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

#include "make_credential.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

#include "../core/approval.h"
#include "../core/assertion_queue.h"
#include "../core/internal.h"
#include "../parse.h"
#include "../policy.h"
#include "../response.h"
#include "../../transport/adapter.h"
#include "../../zerofido_app_i.h"
#include "../../zerofido_crypto.h"
#include "../../zerofido_notify.h"
#include "../../zerofido_store.h"

#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS
#define ZF_CTAP_MC_DIAG(text) FURI_LOG_I("ZeroFIDO:CTAP", "MC %s", (text))
#else
#define ZF_CTAP_MC_DIAG(text)                                                                      \
    do {                                                                                           \
        (void)(text);                                                                              \
    } while (false)
#endif

/*
 * makeCredential is deliberately staged:
 * 1. parse and verify request/PIN state without mutating storage,
 * 2. check excludeList visibility under a maintenance snapshot,
 * 3. ask user approval and generate the credential key,
 * 4. build the CTAP response,
 * 5. publish storage/index changes only after every prior step succeeds.
 *
 * This keeps partially-created credentials out of the index and lets failures
 * before publish return without changing authenticator state.
 */
uint8_t zf_ctap_handle_make_credential(ZerofidoApp *app, ZfTransportSessionId session_id,
                                       const uint8_t *data, size_t data_len, uint8_t *out,
                                       size_t out_capacity, size_t *out_len) {
    typedef struct {
        ZfMakeCredentialRequest request;
        ZfCredentialRecord record;
        ZfClientPinState pin_state;
        char user_line[96];
        union {
            ZfCredentialDescriptor descriptors[ZF_MAX_ALLOW_LIST];
            ZfMakeCredentialResponseScratch response;
            struct {
                uint8_t store_io[ZF_STORE_RECORD_IO_SIZE];
                uint16_t deleted_indices[ZF_MAX_CREDENTIALS];
            } io;
        } work;
    } ZfMakeCredentialScratch;

    _Static_assert(sizeof(ZfMakeCredentialScratch) <= ZF_COMMAND_SCRATCH_SIZE,
                   "makeCredential scratch exceeds command arena");

    ZfMakeCredentialScratch *scratch = zf_ctap_command_scratch(app, sizeof(*scratch));
    bool uv_verified = false;
    bool resident_key = false;
    bool maintenance_acquired = false;
    size_t deleted_count = 0;
    ZfResolvedCapabilities capabilities;
    uint8_t status = ZF_CTAP_ERR_OTHER;

    ZF_CTAP_MC_DIAG("entry");
    if (!scratch) {
        return ZF_CTAP_ERR_OTHER;
    }

    ZF_CTAP_MC_DIAG("scratch");
    scratch->request.exclude_list.entries = scratch->work.descriptors;
    scratch->request.exclude_list.capacity =
        sizeof(scratch->work.descriptors) / sizeof(scratch->work.descriptors[0]);
    status = zf_ctap_parse_make_credential(data, data_len, &scratch->request);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    ZF_CTAP_MC_DIAG("parsed");
    zf_runtime_get_effective_capabilities(app, &capabilities);
    status = zf_ctap_validate_pin_auth_protocol(
        scratch->request.has_pin_auth, scratch->request.has_pin_protocol,
        scratch->request.pin_protocol, capabilities.pin_uv_auth_protocol_2_enabled);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    snprintf(scratch->user_line, sizeof(scratch->user_line), "User: %s",
             scratch->request.user_name[0] ? scratch->request.user_name : "(not provided)");
    if (scratch->request.has_pin_auth && scratch->request.pin_auth_len == 0) {
        status = zf_ctap_handle_empty_pin_auth_probe(app, session_id, "Register",
                                                     scratch->request.rp_id, scratch->user_line);
        goto cleanup;
    }
    if (zf_ctap_effective_uv_requested(scratch->request.has_pin_auth, scratch->request.has_uv,
                                       scratch->request.uv)) {
        status = ZF_CTAP_ERR_UNSUPPORTED_OPTION;
        goto cleanup;
    }
    resident_key = scratch->request.has_rk && scratch->request.rk;
    /*
     * Several CTAP 2.0 NFC clients recover from PIN_REQUIRED by obtaining a
     * PIN token, then retry non-resident MakeCredential without pinAuth. Keep
     * that U2F-like registration path interoperable, but still require UV for
     * resident credentials.
     *
     * TODO: Revisit this compatibility carveout once the default profile can
     * move to CTAP 2.1 makeCredUvNotRqd. That is the spec-level way to express
     * pinless non-resident MakeCredential; CTAP 2.0 strict mode would require
     * the client to send pinAuth on the retry.
     */
    if (!scratch->request.has_pin_auth && zf_ctap_pin_is_set(app) && resident_key) {
        status = ZF_CTAP_ERR_PIN_REQUIRED;
        goto cleanup;
    }
    status = zf_ctap_require_pin_auth_with_state(
        app, &scratch->pin_state, scratch->request.has_uv && scratch->request.uv,
        scratch->request.has_pin_auth, scratch->request.client_data_hash, scratch->request.pin_auth,
        scratch->request.pin_auth_len, scratch->request.has_pin_protocol,
        scratch->request.pin_protocol, scratch->request.rp_id, ZF_PIN_PERMISSION_MC, &uv_verified);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    ZF_CTAP_MC_DIAG("pin ok");
    status = zf_transport_poll_cbor_control(app, session_id);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    ZF_CTAP_MC_DIAG("exclude check");
    if (!zf_ctap_begin_maintenance(app)) {
        status = ZF_CTAP_ERR_NOT_ALLOWED;
        goto cleanup;
    }
    bool excluded = zf_ctap_exclude_list_has_visible_match(
        app->storage, &app->store, scratch->request.rp_id, &scratch->request.exclude_list,
        uv_verified, scratch->work.io.store_io, sizeof(scratch->work.io.store_io));
    zf_ctap_end_maintenance(app);
    if (excluded) {
        status = zf_ctap_request_approval(app, "Register", scratch->request.rp_id,
                                          scratch->user_line, session_id);
        status = status == ZF_CTAP_ERR_KEEPALIVE_CANCEL ? status : ZF_CTAP_ERR_CREDENTIAL_EXCLUDED;
        goto cleanup;
    }
    if (!zf_store_prepare_credential(&scratch->record, scratch->request.rp_id,
                                     scratch->request.user_id, scratch->request.user_id_len,
                                     scratch->request.user_name, scratch->request.user_display_name,
                                     resident_key)) {
        status = ZF_CTAP_ERR_INVALID_PARAMETER;
        goto cleanup;
    }
    ZF_CTAP_MC_DIAG("prepared");
    if (scratch->request.has_cred_protect) {
        scratch->record.cred_protect = scratch->request.cred_protect;
    }

    ZF_CTAP_MC_DIAG("approval");
    status = zf_ctap_request_approval(app, "Register", scratch->request.rp_id, scratch->user_line,
                                      session_id);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    ZF_CTAP_MC_DIAG("approved");
    status = zf_transport_poll_cbor_control(app, session_id);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }

    ZF_CTAP_MC_DIAG("keygen");
    if (!zf_crypto_generate_credential_keypair(&scratch->record)) {
        status = ZF_CTAP_ERR_KEY_STORE_FULL;
        goto cleanup;
    }
    ZF_CTAP_MC_DIAG("keygen done");
    ZF_CTAP_MC_DIAG("response");
    ZfAttestationMode attestation_mode = scratch->request.has_attestation_format_preference
                                             ? scratch->request.preferred_attestation_mode
                                             : capabilities.attestation_mode;
#if ZF_PACKED_ATTESTATION
    if (attestation_mode == ZfAttestationModeNone) {
        status = zf_ctap_build_none_make_credential_response_with_scratch(
            &scratch->work.response, scratch->request.rp_id, &scratch->record, uv_verified,
            scratch->request.has_cred_protect, scratch->request.hmac_secret_requested, out,
            out_capacity, out_len);
    } else {
        status = zf_ctap_build_packed_make_credential_response_with_scratch(
            &scratch->work.response, scratch->request.rp_id, &scratch->record,
            scratch->request.client_data_hash, uv_verified, scratch->request.has_cred_protect,
            scratch->request.hmac_secret_requested, out, out_capacity, out_len);
    }
#else
    (void)attestation_mode;
    status = zf_ctap_build_none_make_credential_response_with_scratch(
        &scratch->work.response, scratch->request.rp_id, &scratch->record, uv_verified,
        scratch->request.has_cred_protect, scratch->request.hmac_secret_requested, out, out_capacity,
        out_len);
#endif
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    ZF_CTAP_MC_DIAG("response done");
    status = zf_transport_poll_cbor_control(app, session_id);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }

    if (!zf_ctap_begin_maintenance(app)) {
        status = ZF_CTAP_ERR_NOT_ALLOWED;
        goto cleanup;
    }
    maintenance_acquired = true;

    if (resident_key) {
        if (!zf_store_find_resident_credential_indices_for_user_with_buffer(
                app->storage, &app->store, scratch->request.rp_id, scratch->request.user_id,
                scratch->request.user_id_len, scratch->work.io.deleted_indices,
                sizeof(scratch->work.io.deleted_indices) /
                    sizeof(scratch->work.io.deleted_indices[0]),
                &deleted_count, scratch->work.io.store_io, sizeof(scratch->work.io.store_io))) {
            status = ZF_CTAP_ERR_OTHER;
            goto cleanup;
        }
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    size_t effective_count =
        app->store.count >= deleted_count ? app->store.count - deleted_count : app->store.count;
    bool has_capacity = effective_count < ZF_MAX_CREDENTIALS;
    furi_mutex_release(app->ui_mutex);
    if (!has_capacity) {
        status = ZF_CTAP_ERR_KEY_STORE_FULL;
        goto cleanup;
    }

    ZF_CTAP_MC_DIAG("store write");
    bool wrote = zf_store_write_record_file_with_buffer(app->storage, &scratch->record,
                                                        scratch->work.io.store_io,
                                                        sizeof(scratch->work.io.store_io));
    if (!wrote) {
        status = ZF_CTAP_ERR_KEY_STORE_FULL;
        goto cleanup;
    }

    if (resident_key && deleted_count > 0) {
        size_t removed_count = 0;

        if (!zf_store_remove_credential_files_by_indices(app->storage, &app->store,
                                                         scratch->work.io.deleted_indices,
                                                         deleted_count, &removed_count)) {
            if (removed_count > 0) {
                furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
                zf_store_publish_deleted_indices(&app->store, scratch->work.io.deleted_indices,
                                                 removed_count);
                zf_ctap_assertion_queue_clear(app);
                furi_mutex_release(app->ui_mutex);
            }
            (void)zf_store_remove_record_file(app->storage, &scratch->record);
            status = ZF_CTAP_ERR_OTHER;
            goto cleanup;
        }
        deleted_count = removed_count;
    }

    ZF_CTAP_MC_DIAG("store publish");
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (deleted_count > 0) {
        zf_store_publish_deleted_indices(&app->store, scratch->work.io.deleted_indices,
                                         deleted_count);
    }
    bool added = zf_store_publish_added_record(&app->store, &scratch->record);
    if (added) {
        if (app->store.capacity > 0U) {
            app->store_records_owned = true;
        }
        zf_ctap_assertion_queue_clear(app);
    }
    furi_mutex_release(app->ui_mutex);
    if (!added) {
        (void)zf_store_remove_record_file(app->storage, &scratch->record);
        status = ZF_CTAP_ERR_KEY_STORE_FULL;
        goto cleanup;
    }
    zerofido_notify_success(app);
    status = ZF_CTAP_SUCCESS;
    ZF_CTAP_MC_DIAG("success");

cleanup:
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS
    if (status != ZF_CTAP_SUCCESS) {
        FURI_LOG_I("ZeroFIDO:CTAP", "MC error=%02X", status);
    }
#endif
    if (maintenance_acquired) {
        zf_ctap_end_maintenance(app);
    }
    zf_crypto_secure_zero(scratch, sizeof(*scratch));
    zf_app_command_scratch_release(app);
    return status;
}
