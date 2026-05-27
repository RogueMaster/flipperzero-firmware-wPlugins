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

#include "response.h"

#include <furi.h>
#include <string.h>

#include "extensions/cred_protect.h"
#include "extensions/hmac_secret.h"
#include "../zerofido_attestation.h"
#include "../zerofido_cbor.h"
#include "../zerofido_crypto.h"
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS
#define ZF_CTAP_RESPONSE_DIAG(text) FURI_LOG_I("ZeroFIDO:CTAP", "MC response %s", (text))
#else
#define ZF_CTAP_RESPONSE_DIAG(text)                                                                \
    do {                                                                                           \
        (void)(text);                                                                              \
    } while (false)
#endif

static void zf_write_be16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)value;
}

static void zf_write_be32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static bool zf_encode_cose_key(const ZfCredentialRecord *record, uint8_t *out, size_t out_capacity,
                               size_t *out_len) {
    ZfCborEncoder enc;
    if (!zf_cbor_encoder_init(&enc, out, out_capacity)) {
        return false;
    }

    bool ok = zf_cbor_encode_p256_cose_key(&enc, -7, record->public_x, sizeof(record->public_x),
                                           record->public_y, sizeof(record->public_y));
    if (!ok) {
        return false;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    return true;
}

static bool zf_encode_make_credential_extensions(uint8_t cred_protect, bool include_cred_protect,
                                                 bool include_hmac_secret, bool hmac_secret_created,
                                                 uint8_t *out, size_t out_capacity,
                                                 size_t *out_len) {
    ZfCborEncoder enc;
    size_t pairs = (include_cred_protect ? 1U : 0U) + (include_hmac_secret ? 1U : 0U);

    if (!zf_cbor_encoder_init(&enc, out, out_capacity)) {
        return false;
    }
    if (!zf_cbor_encode_map(&enc, pairs)) {
        return false;
    }
    if (include_cred_protect &&
        !zf_ctap_cred_protect_encode_make_credential_output(&enc, cred_protect)) {
        return false;
    }
    if (include_hmac_secret &&
        !zf_ctap_hmac_secret_encode_make_credential_output(&enc, hmac_secret_created)) {
        return false;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    return true;
}

/*
 * Builds WebAuthn authenticatorData:
 *   SHA256(rpId) || flags || signCount || attestedCredentialData? || extensions?
 *
 * Flags are derived from UP/UV plus AT/ED inclusion. The caller supplies the
 * sign counter value that will be committed if the response succeeds.
 */
static size_t zf_build_auth_data(const char *rp_id, bool user_present, bool user_verified,
                                 bool include_attested_data, bool include_extension_data,
                                 const ZfCredentialRecord *record, uint32_t sign_count,
                                 const uint8_t *extension_data, size_t extension_data_len,
                                 const uint8_t *cose, size_t cose_len, uint8_t *out,
                                 size_t out_capacity) {
    uint8_t flags = user_present ? 0x01 : 0x00;
    uint8_t rp_hash[32];
    size_t offset = 0;
    const uint8_t *aaguid = zf_attestation_get_aaguid();

    zf_crypto_sha256((const uint8_t *)rp_id, strlen(rp_id), rp_hash);
    if (user_verified) {
        flags |= 0x04;
    }
    if (include_attested_data) {
        flags |= 0x40;
    }
    if (include_extension_data) {
        flags |= 0x80;
    }
    if (out_capacity < 37) {
        return 0;
    }

    memcpy(&out[offset], rp_hash, sizeof(rp_hash));
    offset += sizeof(rp_hash);
    out[offset++] = flags;
    zf_write_be32(&out[offset], sign_count);
    offset += 4;

    if (include_attested_data) {
        if (!record || !cose || cose_len == 0U) {
            return 0;
        }
        if (offset + ZF_AAGUID_LEN + 2 + record->credential_id_len + cose_len > out_capacity) {
            return 0;
        }

        memcpy(&out[offset], aaguid, ZF_AAGUID_LEN);
        offset += ZF_AAGUID_LEN;
        zf_write_be16(&out[offset], (uint16_t)record->credential_id_len);
        offset += 2;
        memcpy(&out[offset], record->credential_id, record->credential_id_len);
        offset += record->credential_id_len;
        memcpy(&out[offset], cose, cose_len);
        offset += cose_len;
    }

    if (include_extension_data) {
        if (!extension_data || offset + extension_data_len > out_capacity) {
            return 0;
        }
        memcpy(&out[offset], extension_data, extension_data_len);
        offset += extension_data_len;
    }

    return offset;
}

static bool zf_make_credential_auth_data_size(const ZfCredentialRecord *record, size_t cose_len,
                                              size_t extension_data_len, size_t *out_len) {
    size_t len = 37U + ZF_AAGUID_LEN + 2U;

    if (!record || !out_len || record->credential_id_len > UINT16_MAX || cose_len == 0U) {
        return false;
    }
    len += record->credential_id_len + cose_len + extension_data_len;
    *out_len = len;
    return true;
}

uint8_t zf_ctap_build_get_info_response(const ZfResolvedCapabilities *capabilities,
                                        bool client_pin_set, uint8_t *out, size_t out_capacity,
                                        size_t *out_len) {
    ZfCborEncoder enc;
    const uint8_t *aaguid = zf_attestation_get_aaguid();
    size_t versions_count = 1;
    size_t options_count = 4;
#if !defined(ZF_NFC_ONLY) && !defined(ZF_USB_ONLY)
    size_t transports_count = 0;
#endif
    bool include_algorithms = false;
    bool include_ctap21_info_fields = false;
#if defined(ZF_NFC_ONLY)
    size_t get_info_pairs = 7;
#else
    size_t get_info_pairs = 6;
#endif

    if (!capabilities || !zf_cbor_encoder_init(&enc, out, out_capacity)) {
        return ZF_CTAP_ERR_OTHER;
    }

    include_ctap21_info_fields = capabilities->advertise_fido_2_1;
    /*
     * Keep CTAP 2.0 NFC GetInfo compact enough for conservative ISO-DEP single
     * frame replies. Full CTAP 2.1 profiles still advertise algorithms.
     */
    include_algorithms = include_ctap21_info_fields;
#if defined(ZF_USB_ONLY)
    if (include_ctap21_info_fields) {
        get_info_pairs++;
    }
#elif !defined(ZF_NFC_ONLY)
    if (include_ctap21_info_fields || capabilities->advertise_nfc_transport) {
        get_info_pairs++;
    }
#endif
    if (include_algorithms) {
        get_info_pairs++;
    }
    if (include_ctap21_info_fields) {
        get_info_pairs += 2;
    }
    if (capabilities->advertise_fido_2_1) {
        versions_count++;
    }
    if (capabilities->advertise_u2f_v2) {
        versions_count++;
    }
    if (capabilities->pin_uv_auth_token_enabled) {
        options_count++;
    }
    if (capabilities->make_cred_uv_not_required) {
        options_count++;
    }
#if !defined(ZF_NFC_ONLY) && !defined(ZF_USB_ONLY)
    if (capabilities->advertise_usb_transport) {
        transports_count++;
    }
    if (capabilities->advertise_nfc_transport) {
        transports_count++;
    }
#endif

    bool ok =
        zf_cbor_encode_map(&enc, get_info_pairs) && zf_cbor_encode_uint(&enc, 1) &&
        zf_cbor_encode_array(&enc, versions_count) &&
        (!capabilities->advertise_fido_2_1 || zf_cbor_encode_text(&enc, "FIDO_2_1")) &&
        zf_cbor_encode_text(&enc, "FIDO_2_0") &&
        (!capabilities->advertise_u2f_v2 || zf_cbor_encode_text(&enc, "U2F_V2")) &&
        zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_array(&enc, 2) &&
        zf_cbor_encode_text(&enc, "credProtect") && zf_cbor_encode_text(&enc, "hmac-secret") &&
        zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_bytes(&enc, aaguid, ZF_AAGUID_LEN) &&
        zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_map(&enc, options_count) &&
        zf_cbor_encode_text(&enc, "rk") && zf_cbor_encode_bool(&enc, true) &&
        zf_cbor_encode_text(&enc, "up") && zf_cbor_encode_bool(&enc, true) &&
        zf_cbor_encode_text(&enc, "plat") && zf_cbor_encode_bool(&enc, false) &&
        zf_cbor_encode_text(&enc, "clientPin") && zf_cbor_encode_bool(&enc, client_pin_set) &&
        (!capabilities->pin_uv_auth_token_enabled ||
         (zf_cbor_encode_text(&enc, "pinUvAuthToken") && zf_cbor_encode_bool(&enc, true))) &&
        (!capabilities->make_cred_uv_not_required ||
         (zf_cbor_encode_text(&enc, "makeCredUvNotRqd") && zf_cbor_encode_bool(&enc, true))) &&
        zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_uint(&enc, ZF_MAX_MSG_SIZE) &&
        zf_cbor_encode_uint(&enc, 6) &&
        zf_cbor_encode_array(&enc, capabilities->pin_uv_auth_protocol_2_enabled ? 2 : 1) &&
        (!capabilities->pin_uv_auth_protocol_2_enabled || zf_cbor_encode_uint(&enc, 2)) &&
        zf_cbor_encode_uint(&enc, 1);

#if defined(ZF_NFC_ONLY)
    if (ok) {
        ok = zf_cbor_encode_uint(&enc, 9) && zf_cbor_encode_array(&enc, 1) &&
             zf_cbor_encode_text(&enc, "nfc");
    }
#elif defined(ZF_USB_ONLY)
    if (ok && include_ctap21_info_fields) {
        ok = zf_cbor_encode_uint(&enc, 9) && zf_cbor_encode_array(&enc, 1) &&
             zf_cbor_encode_text(&enc, "usb");
    }
#else
    if (ok && (include_ctap21_info_fields || capabilities->advertise_nfc_transport)) {
        ok = zf_cbor_encode_uint(&enc, 9) && zf_cbor_encode_array(&enc, transports_count) &&
             (!capabilities->advertise_usb_transport || zf_cbor_encode_text(&enc, "usb")) &&
             (!capabilities->advertise_nfc_transport || zf_cbor_encode_text(&enc, "nfc"));
    }
#endif

    if (ok && include_algorithms) {
        ok = zf_cbor_encode_uint(&enc, 10) && zf_cbor_encode_array(&enc, 1) &&
             zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
             zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
             zf_cbor_encode_text(&enc, "public-key");
    }

    if (ok && include_ctap21_info_fields) {
        ok = zf_cbor_encode_uint(&enc, 13) && zf_cbor_encode_uint(&enc, ZF_MIN_PIN_LENGTH) &&
             zf_cbor_encode_uint(&enc, 14) && zf_cbor_encode_uint(&enc, ZF_FIRMWARE_VERSION);
    }

    if (!ok) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    return ZF_CTAP_SUCCESS;
}

uint8_t zf_ctap_build_none_make_credential_response_with_scratch(
    ZfMakeCredentialResponseScratch *scratch, const char *rp_id, const ZfCredentialRecord *record,
    bool user_verified, bool include_cred_protect, bool include_hmac_secret, uint8_t *out,
    size_t out_capacity, size_t *out_len) {
    uint8_t status = ZF_CTAP_ERR_OTHER;
    size_t extension_data_len = 0;
    size_t cose_len = 0;
    size_t auth_data_len = 0;
    uint8_t *auth_data = NULL;

    if (!scratch) {
        return ZF_CTAP_ERR_OTHER;
    }
    memset(scratch, 0, sizeof(*scratch));

    if ((include_cred_protect || include_hmac_secret) &&
        !zf_encode_make_credential_extensions(
            record->cred_protect, include_cred_protect, include_hmac_secret, record->hmac_secret,
            scratch->extension_data, sizeof(scratch->extension_data), &extension_data_len)) {
        ZF_CTAP_RESPONSE_DIAG("extensions failed");
        goto cleanup;
    }

    if (!zf_encode_cose_key(record, scratch->cose, sizeof(scratch->cose), &cose_len) ||
        !zf_make_credential_auth_data_size(record, cose_len, extension_data_len, &auth_data_len)) {
        ZF_CTAP_RESPONSE_DIAG("authData failed");
        goto cleanup;
    }

    ZfCborEncoder enc;
    if (!zf_cbor_encoder_init(&enc, out, out_capacity)) {
        goto cleanup;
    }

    bool ok = zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
              zf_cbor_encode_text(&enc, "none") && zf_cbor_encode_uint(&enc, 2) &&
              zf_cbor_reserve_bytes(&enc, auth_data_len, &auth_data) &&
              zf_build_auth_data(
                  rp_id, true, user_verified, true, include_cred_protect || include_hmac_secret,
                  record, record->sign_count, scratch->extension_data, extension_data_len,
                  scratch->cose, cose_len, auth_data, auth_data_len) == auth_data_len &&
              zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 0);
    if (!ok) {
        goto cleanup;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    status = ZF_CTAP_SUCCESS;

cleanup:
    zf_crypto_secure_zero(scratch, sizeof(*scratch));
    return status;
}

#if ZF_PACKED_ATTESTATION
uint8_t zf_ctap_build_packed_make_credential_response_with_scratch(
    ZfMakeCredentialResponseScratch *scratch, const char *rp_id, const ZfCredentialRecord *record,
    const uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN], bool user_verified,
    bool include_cred_protect, bool include_hmac_secret, uint8_t *out, size_t out_capacity,
    size_t *out_len) {
    uint8_t status = ZF_CTAP_ERR_OTHER;
    size_t extension_data_len = 0;
    size_t signature_len = 0;
    size_t cert_len = 0;
    size_t loaded_cert_len = 0;
    size_t cose_len = 0;
    size_t auth_data_len = 0;
    uint8_t *auth_data = NULL;
    uint8_t *cert_out = NULL;

    if (!scratch) {
        return ZF_CTAP_ERR_OTHER;
    }
    memset(scratch, 0, sizeof(*scratch));

    if ((include_cred_protect || include_hmac_secret) &&
        !zf_encode_make_credential_extensions(
            record->cred_protect, include_cred_protect, include_hmac_secret, record->hmac_secret,
            scratch->extension_data, sizeof(scratch->extension_data), &extension_data_len)) {
        ZF_CTAP_RESPONSE_DIAG("extensions failed");
        goto cleanup;
    }

    if (!zf_encode_cose_key(record, scratch->cose, sizeof(scratch->cose), &cose_len) ||
        !zf_make_credential_auth_data_size(record, cose_len, extension_data_len, &auth_data_len)) {
        ZF_CTAP_RESPONSE_DIAG("authData failed");
        goto cleanup;
    }

    ZfCborEncoder enc;
    if (!zf_cbor_encoder_init(&enc, out, out_capacity)) {
        goto cleanup;
    }

    bool ok = zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
              zf_cbor_encode_text(&enc, "packed") && zf_cbor_encode_uint(&enc, 2) &&
              zf_cbor_reserve_bytes(&enc, auth_data_len, &auth_data) &&
              zf_build_auth_data(
                  rp_id, true, user_verified, true, include_cred_protect || include_hmac_secret,
                  record, record->sign_count, scratch->extension_data, extension_data_len,
                  scratch->cose, cose_len, auth_data, auth_data_len) == auth_data_len;
    if (!ok) {
        ZF_CTAP_RESPONSE_DIAG("authData failed");
        goto cleanup;
    }

    if (!zf_attestation_ensure_ready() || !zf_attestation_get_leaf_cert_der_len(&cert_len) ||
        !zf_attestation_sign_parts(auth_data, auth_data_len, client_data_hash,
                                   ZF_CLIENT_DATA_HASH_LEN, scratch->signature,
                                   sizeof(scratch->signature), &signature_len)) {
        ZF_CTAP_RESPONSE_DIAG("packed required unavailable");
        zf_crypto_secure_zero(scratch->signature, sizeof(scratch->signature));
        goto cleanup;
    }

    ok = zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 3) &&
         zf_cbor_encode_text(&enc, "alg") && zf_cbor_encode_int(&enc, -7) &&
         zf_cbor_encode_text(&enc, "sig") &&
         zf_cbor_encode_bytes(&enc, scratch->signature, signature_len) &&
         zf_cbor_encode_text(&enc, "x5c") && zf_cbor_encode_array(&enc, 1) &&
         zf_cbor_reserve_bytes(&enc, cert_len, &cert_out) &&
         zf_attestation_load_leaf_cert_der(cert_out, cert_len, &loaded_cert_len) &&
         loaded_cert_len == cert_len;
    if (!ok) {
        goto cleanup;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    status = ZF_CTAP_SUCCESS;

cleanup:
    zf_crypto_secure_zero(scratch, sizeof(*scratch));
    return status;
}
#endif

/*
 * Assertion signatures cover SHA256(authenticatorData || clientDataHash). The
 * optional user, numberOfCredentials, and userSelected fields are controlled by
 * the getAssertion branch that selected the credential.
 */
uint8_t zf_ctap_build_assertion_response_with_scratch(
    ZfAssertionResponseScratch *scratch, const ZfAssertionRequestData *request,
    const ZfCredentialRecord *record, bool user_present, bool user_verified, uint32_t sign_count,
    bool include_user_details, bool include_count, size_t match_count, bool include_user_selected,
    bool user_selected, const uint8_t *extension_data, size_t extension_data_len, uint8_t *out,
    size_t out_capacity, size_t *out_len) {
    uint8_t status = ZF_CTAP_ERR_OTHER;
    size_t signature_len = 0;

    if (!scratch) {
        return ZF_CTAP_ERR_OTHER;
    }
    size_t auth_data_len = zf_build_auth_data(request->rp_id, user_present, user_verified, false,
                                              extension_data_len > 0U, NULL, sign_count,
                                              extension_data, extension_data_len, NULL, 0,
                                              scratch->auth_data, sizeof(scratch->auth_data));
    if (auth_data_len == 0) {
        goto cleanup;
    }

    zf_crypto_sha256_concat(scratch->auth_data, auth_data_len, request->client_data_hash,
                            sizeof(request->client_data_hash), scratch->sign_hash);

    if (!zf_crypto_sign_hash(record, scratch->sign_hash, scratch->signature,
                             sizeof(scratch->signature), &signature_len)) {
        goto cleanup;
    }

    ZfCborEncoder enc;
    if (!zf_cbor_encoder_init(&enc, out, out_capacity)) {
        goto cleanup;
    }

    bool include_user_name = include_user_details && record->user_name[0] != '\0';
    bool include_display_name = include_user_details && record->user_display_name[0] != '\0';
    bool emit_user_selected = include_user_selected && user_selected;
    size_t user_pairs = 1;
    if (include_user_name) {
        user_pairs++;
    }
    if (include_display_name) {
        user_pairs++;
    }

    size_t pairs = 4;
    if (include_count) {
        pairs++;
    }
    if (emit_user_selected) {
        pairs++;
    }
    bool ok = zf_cbor_encode_map(&enc, pairs) && zf_cbor_encode_uint(&enc, 1) &&
              zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
              zf_cbor_encode_bytes(&enc, record->credential_id, record->credential_id_len) &&
              zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key") &&
              zf_cbor_encode_uint(&enc, 2) &&
              zf_cbor_encode_bytes(&enc, scratch->auth_data, auth_data_len) &&
              zf_cbor_encode_uint(&enc, 3) &&
              zf_cbor_encode_bytes(&enc, scratch->signature, signature_len) &&
              zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_map(&enc, user_pairs) &&
              zf_cbor_encode_text(&enc, "id") &&
              zf_cbor_encode_bytes(&enc, record->user_id, record->user_id_len);
    if (!ok) {
        goto cleanup;
    }
    if (include_user_name) {
        ok = zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, record->user_name);
    }
    if (ok && include_display_name) {
        ok = zf_cbor_encode_text(&enc, "displayName") &&
             zf_cbor_encode_text(&enc, record->user_display_name);
    }
    if (include_count) {
        ok = zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_uint(&enc, match_count);
    }
    if (ok && emit_user_selected) {
        ok = zf_cbor_encode_uint(&enc, 6) && zf_cbor_encode_bool(&enc, true);
    }
    if (!ok) {
        goto cleanup;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    status = ZF_CTAP_SUCCESS;

cleanup:
    zf_crypto_secure_zero(scratch, sizeof(*scratch));
    return status;
}
