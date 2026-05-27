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

#include "response_encode.h"

#include <stddef.h>
#include <string.h>

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>

#include "apdu_internal.h"
#include "session_internal.h"
#include "persistence.h"
#include "../zerofido_crypto.h"

#define TAG "U2f"
#define ZF_U2F_DERIVE_PRIVATE_KEY_MAX_ATTEMPTS 8U

#if !ZF_RELEASE_DIAGNOSTICS
#undef FURI_LOG_D
#undef FURI_LOG_W
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
#endif

static bool zf_u2f_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t size) {
    uint8_t diff = 0;

    if (!left || !right) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        diff |= left[i] ^ right[i];
    }
    return diff == 0;
}

/*
 * U2F key handles are stateless. The credential private key is deterministically
 * derived from the device secret, appId, and handle nonce, then rejected unless
 * it is a valid P-256 private scalar.
 */
static bool zf_u2f_derive_private_key(U2fData *instance, const uint8_t app_id[U2F_APP_ID_SIZE],
                                      const uint8_t nonce[U2F_NONCE_SIZE],
                                      uint8_t private_key[U2F_EC_KEY_SIZE]) {
    ZfHmacSha256Scratch hmac_scratch;

    if (!zf_crypto_hmac_sha256_parts_with_scratch(
            &hmac_scratch, instance->device_key, sizeof(instance->device_key), app_id,
            U2F_APP_ID_SIZE, nonce, U2F_NONCE_SIZE, private_key)) {
        zf_crypto_secure_zero(private_key, U2F_EC_KEY_SIZE);
        return false;
    }
    if (!zf_crypto_p256_private_key_valid(private_key)) {
        zf_crypto_secure_zero(private_key, U2F_EC_KEY_SIZE);
        return false;
    }
    return true;
}

static bool zf_u2f_compute_handle_mac(U2fData *instance, const uint8_t private_key[U2F_EC_KEY_SIZE],
                                      const uint8_t app_id[U2F_APP_ID_SIZE],
                                      uint8_t mac[U2F_HASH_SIZE]) {
    ZfHmacSha256Scratch hmac_scratch;

    return zf_crypto_hmac_sha256_parts_with_scratch(&hmac_scratch, instance->device_key,
                                                    sizeof(instance->device_key), private_key,
                                                    U2F_EC_KEY_SIZE, app_id, U2F_APP_ID_SIZE, mac);
}

static inline uint32_t zf_u2f_to_big_endian(uint32_t value) {
    return __builtin_bswap32(value);
}

/*
 * Builds the U2F registration response. The key handle is {MAC, nonce}; the
 * registration signature covers the U2F-defined app/challenge/key-handle/public
 * key base string and requires ready attestation assets.
 */
uint16_t zf_u2f_encode_register_response(U2fData *instance, uint8_t *buf, uint16_t request_len,
                                         uint16_t response_capacity) {
    U2fParsedApdu apdu = {0};
    U2fRegisterResp *resp = (U2fRegisterResp *)buf;
    U2fKeyHandle handle;
    uint8_t private_key[U2F_EC_KEY_SIZE];
    U2fPubKey public_key;
    uint8_t hash[U2F_HASH_SIZE];
    size_t response_base_len = offsetof(U2fRegisterResp, cert);
    size_t cert_capacity =
        response_capacity > response_base_len ? response_capacity - response_base_len : 0;
    uint16_t cert_len = 0;
    size_t signature_len = 0;
    bool derived_key = false;
    uint16_t response_len = 0;

    if (!u2f_parse_apdu_header(buf, request_len, false, &apdu)) {
        return zf_u2f_write_status(buf, ZF_U2F_SW_WRONG_LENGTH);
    }
    if (apdu.lc != (U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE)) {
        return zf_u2f_write_status(buf, ZF_U2F_SW_WRONG_LENGTH);
    }
    const uint8_t *challenge = apdu.data;
    const uint8_t *app_id = apdu.data + U2F_CHALLENGE_SIZE;

    if (instance->callback != NULL) {
        instance->callback(U2fNotifyRegister, instance->context);
    }
    if (!instance->cert_ready) {
        return zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
    }
    if (!u2f_consume_user_present(instance)) {
        return zf_u2f_write_status(buf, ZF_U2F_SW_CONDITIONS_NOT_SATISFIED);
    }

    handle.len = U2F_HASH_SIZE * 2;
    for (size_t attempt = 0; attempt < ZF_U2F_DERIVE_PRIVATE_KEY_MAX_ATTEMPTS; ++attempt) {
        furi_hal_random_fill_buf(handle.nonce, sizeof(handle.nonce));
        if (zf_u2f_derive_private_key(instance, app_id, handle.nonce, private_key)) {
            derived_key = true;
            break;
        }
    }

    if (!derived_key || !zf_u2f_compute_handle_mac(instance, private_key, app_id, handle.hash)) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }

    public_key.format = 0x04;
    if (!zf_crypto_compute_public_key_from_private(private_key, public_key.xy,
                                                   public_key.xy + U2F_EC_KEY_SIZE)) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }

    {
        uint8_t reserved_byte = 0;
        uint8_t preimage[1U + U2F_APP_ID_SIZE + U2F_CHALLENGE_SIZE + (U2F_HASH_SIZE * 2U) +
                         sizeof(U2fPubKey)];
        size_t offset = 0;

        memcpy(preimage + offset, &reserved_byte, sizeof(reserved_byte));
        offset += sizeof(reserved_byte);
        memcpy(preimage + offset, app_id, U2F_APP_ID_SIZE);
        offset += U2F_APP_ID_SIZE;
        memcpy(preimage + offset, challenge, U2F_CHALLENGE_SIZE);
        offset += U2F_CHALLENGE_SIZE;
        memcpy(preimage + offset, handle.hash, handle.len);
        offset += handle.len;
        memcpy(preimage + offset, &public_key, sizeof(public_key));
        offset += sizeof(public_key);
        zf_crypto_sha256(preimage, offset, hash);
        zf_crypto_secure_zero(preimage, sizeof(preimage));
    }

    cert_len = (uint16_t)u2f_data_cert_load(buf + response_base_len, cert_capacity);
    if (cert_len == 0 || cert_len > cert_capacity) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }

    if (!zf_crypto_sign_hash_with_private_key(instance->cert_key, hash,
                                              buf + response_base_len + cert_len,
                                              cert_capacity - cert_len, &signature_len)) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }
    if (cert_len + signature_len + ZF_U2F_STATUS_SIZE > cert_capacity) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }

    resp->reserved = 0x05;
    memcpy(&resp->pub_key, &public_key, sizeof(U2fPubKey));
    memcpy(&resp->key_handle, &handle, sizeof(U2fKeyHandle));
    zf_u2f_write_status(resp->cert + cert_len + signature_len, ZF_U2F_SW_NO_ERROR);

    response_len = sizeof(U2fRegisterResp) + cert_len + signature_len + ZF_U2F_STATUS_SIZE;

cleanup:
    zf_crypto_secure_zero(hash, sizeof(hash));
    zf_crypto_secure_zero(private_key, sizeof(private_key));
    zf_crypto_secure_zero(&handle, sizeof(handle));
    zf_crypto_secure_zero(&public_key, sizeof(public_key));
    return response_len;
}

/*
 * Builds the U2F authentication response. The handle MAC is verified before
 * deriving the key, check-only requests return before signing, user presence is
 * consumed once, and the counter is reserved durably before committing in memory.
 */
uint16_t zf_u2f_encode_authenticate_response(U2fData *instance, uint8_t *buf, uint16_t request_len,
                                             uint16_t response_capacity) {
    U2fParsedApdu apdu = {0};
    U2fAuthResp *resp = (U2fAuthResp *)buf;
    uint8_t private_key[U2F_EC_KEY_SIZE];
    uint8_t mac_control[32];
    uint8_t flags = 0;
    uint8_t hash[U2F_HASH_SIZE];
    uint32_t next_counter = 0;
    uint32_t be_u2f_counter = 0;
    bool user_present = false;
    uint16_t response_len = 0;
    bool notify_success = false;

    if (!u2f_parse_apdu_header(buf, request_len, false, &apdu)) {
        return zf_u2f_write_status(buf, ZF_U2F_SW_WRONG_LENGTH);
    }
    if (apdu.lc < (U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1)) {
        return zf_u2f_write_status(buf, ZF_U2F_SW_WRONG_LENGTH);
    }

    const uint8_t *challenge = apdu.data;
    const uint8_t *app_id = apdu.data + U2F_CHALLENGE_SIZE;
    uint8_t key_handle_len = apdu.data[U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE];
    const uint8_t *key_handle = apdu.data + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1;

    if (instance->callback != NULL) {
        instance->callback(U2fNotifyAuth, instance->context);
    }
    if (instance->counter == UINT32_MAX) {
        return zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
    }

    next_counter = instance->counter + 1;
    be_u2f_counter = zf_u2f_to_big_endian(next_counter);

    if (key_handle_len != (U2F_HASH_SIZE * 2)) {
        if (apdu.p1 == U2fEnforce) {
            u2f_clear_user_present(instance);
        }
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_WRONG_DATA);
        goto cleanup;
    }

    if (!zf_u2f_derive_private_key(instance, app_id, key_handle + U2F_HASH_SIZE, private_key) ||
        !zf_u2f_compute_handle_mac(instance, private_key, app_id, mac_control)) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }

    if (!zf_u2f_constant_time_equal(key_handle, mac_control, sizeof(mac_control))) {
        FURI_LOG_W(TAG, "Wrong handle!");
        if (apdu.p1 == U2fEnforce) {
            u2f_clear_user_present(instance);
        }
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_WRONG_DATA);
        goto cleanup;
    }
    if (apdu.p1 == U2fCheckOnly) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_CONDITIONS_NOT_SATISFIED);
        goto cleanup;
    }

    user_present = u2f_consume_user_present(instance);
    if (user_present) {
        flags |= 1;
    } else if (apdu.p1 == U2fEnforce) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_CONDITIONS_NOT_SATISFIED);
        goto cleanup;
    }

    {
        uint8_t preimage[U2F_APP_ID_SIZE + 1U + sizeof(be_u2f_counter) + U2F_CHALLENGE_SIZE];
        size_t offset = 0;

        memcpy(preimage + offset, app_id, U2F_APP_ID_SIZE);
        offset += U2F_APP_ID_SIZE;
        memcpy(preimage + offset, &flags, sizeof(flags));
        offset += sizeof(flags);
        memcpy(preimage + offset, &be_u2f_counter, sizeof(be_u2f_counter));
        offset += sizeof(be_u2f_counter);
        memcpy(preimage + offset, challenge, U2F_CHALLENGE_SIZE);
        offset += U2F_CHALLENGE_SIZE;
        zf_crypto_sha256(preimage, offset, hash);
        zf_crypto_secure_zero(preimage, sizeof(preimage));
    }

    if (response_capacity <= sizeof(U2fAuthResp)) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }

    resp->user_present = flags;
    resp->counter = be_u2f_counter;
    size_t signature_len = 0;
    if (!zf_crypto_sign_hash_with_private_key(private_key, hash, resp->signature,
                                              response_capacity - sizeof(U2fAuthResp),
                                              &signature_len)) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }
    if (sizeof(U2fAuthResp) + signature_len + ZF_U2F_STATUS_SIZE > response_capacity) {
        response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
        goto cleanup;
    }
    zf_u2f_write_status(resp->signature + signature_len, ZF_U2F_SW_NO_ERROR);

    if (next_counter > instance->counter_high_water) {
        uint32_t high_water = 0;
        if (!u2f_data_cnt_reserve(next_counter, &high_water)) {
            response_len = zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
            goto cleanup;
        }
        instance->counter_high_water = high_water;
    }
    instance->counter = next_counter;
    FURI_LOG_D(TAG, "Counter: %lu", (unsigned long)instance->counter);

    response_len = sizeof(U2fAuthResp) + signature_len + ZF_U2F_STATUS_SIZE;
    notify_success = true;

cleanup:
    zf_crypto_secure_zero(mac_control, sizeof(mac_control));
    zf_crypto_secure_zero(private_key, sizeof(private_key));
    zf_crypto_secure_zero(hash, sizeof(hash));
    if (instance->callback != NULL) {
        if (notify_success) {
            instance->callback(U2fNotifyAuthSuccess, instance->context);
        }
    }

    return response_len;
}
