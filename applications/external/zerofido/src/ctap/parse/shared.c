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

#include "internal.h"

#include <string.h>

#include "../extensions/cred_protect.h"
#include "../extensions/hmac_secret.h"
#include "../../zerofido_crypto.h"

ZfCtapTextKey zf_ctap_classify_text_key(const uint8_t *ptr, size_t size) {
    typedef struct {
        uint8_t size;
        uint8_t key;
        char text[11];
    } ZfCtapTextKeyEntry;

    static const ZfCtapTextKeyEntry keys[] = {
        {3, ZfCtapTextKeyAlg, "alg"},
        {11, ZfCtapTextKeyCredProtect, {'c', 'r', 'e', 'd', 'P', 'r', 'o', 't', 'e', 'c', 't'}},
        {11, ZfCtapTextKeyDisplayName, {'d', 'i', 's', 'p', 'l', 'a', 'y', 'N', 'a', 'm', 'e'}},
        {11, ZfCtapTextKeyHmacSecret, {'h', 'm', 'a', 'c', '-', 's', 'e', 'c', 'r', 'e', 't'}},
        {4, ZfCtapTextKeyIcon, "icon"},
        {2, ZfCtapTextKeyId, "id"},
        {4, ZfCtapTextKeyName, "name"},
        {10, ZfCtapTextKeyPublicKey, "public-key"},
        {2, ZfCtapTextKeyRk, "rk"},
        {4, ZfCtapTextKeyType, "type"},
        {2, ZfCtapTextKeyUp, "up"},
        {2, ZfCtapTextKeyUv, "uv"},
    };

    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        if (size == keys[i].size && memcmp(ptr, keys[i].text, size) == 0) {
            return (ZfCtapTextKey)keys[i].key;
        }
    }

    return ZfCtapTextKeyUnknown;
}

bool zf_ctap_mark_seen_key(uint16_t *seen_keys, uint64_t key) {
    if (key >= 16) {
        return true;
    }

    uint16_t mask = (uint16_t)(1U << key);
    if ((*seen_keys & mask) != 0) {
        return false;
    }

    *seen_keys |= mask;
    return true;
}

bool zf_ctap_cbor_read_text_copy(ZfCborCursor *cursor, char *out, size_t out_size) {
    const uint8_t *ptr = NULL;
    size_t size = 0;

    if (!zf_cbor_read_text_ptr(cursor, &ptr, &size) || size >= out_size ||
        memchr(ptr, '\0', size) != NULL) {
        return false;
    }

    memcpy(out, ptr, size);
    out[size] = '\0';
    return true;
}

bool zf_ctap_cbor_read_text_discard(ZfCborCursor *cursor) {
    const uint8_t *ptr = NULL;
    size_t size = 0;

    return zf_cbor_read_text_ptr(cursor, &ptr, &size) && memchr(ptr, '\0', size) == NULL;
}

bool zf_ctap_cbor_read_bytes_copy(ZfCborCursor *cursor, uint8_t *out, size_t out_capacity,
                                  size_t *out_size) {
    const uint8_t *ptr = NULL;
    size_t size = 0;

    if (!zf_cbor_read_bytes_ptr(cursor, &ptr, &size) || size > out_capacity) {
        return false;
    }

    memcpy(out, ptr, size);
    *out_size = size;
    return true;
}

bool zf_ctap_parse_cose_p256_key_agreement(ZfCborCursor *cursor, uint8_t x[ZF_PUBLIC_KEY_LEN],
                                           uint8_t y[ZF_PUBLIC_KEY_LEN]) {
    size_t pairs = 0;
    bool saw_kty = false;
    bool saw_alg = false;
    bool saw_crv = false;
    bool saw_x = false;
    bool saw_y = false;

    if (!zf_cbor_read_map_start(cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        int64_t key = 0;
        if (!zf_cbor_read_int(cursor, &key)) {
            return false;
        }

        switch (key) {
        case 1: {
            int64_t kty = 0;
            if (saw_kty || !zf_cbor_read_int(cursor, &kty) || kty != 2) {
                return false;
            }
            saw_kty = true;
            break;
        }
        case 3: {
            int64_t alg = 0;
            if (saw_alg || !zf_cbor_read_int(cursor, &alg) || alg != -25) {
                return false;
            }
            saw_alg = true;
            break;
        }
        case -1: {
            int64_t crv = 0;
            if (saw_crv || !zf_cbor_read_int(cursor, &crv) || crv != 1) {
                return false;
            }
            saw_crv = true;
            break;
        }
        case -2: {
            size_t size = 0;
            if (saw_x || !zf_ctap_cbor_read_bytes_copy(cursor, x, ZF_PUBLIC_KEY_LEN, &size) ||
                size != ZF_PUBLIC_KEY_LEN) {
                return false;
            }
            saw_x = true;
            break;
        }
        case -3: {
            size_t size = 0;
            if (saw_y || !zf_ctap_cbor_read_bytes_copy(cursor, y, ZF_PUBLIC_KEY_LEN, &size) ||
                size != ZF_PUBLIC_KEY_LEN) {
                return false;
            }
            saw_y = true;
            break;
        }
        default:
            return false;
        }
    }

    return saw_kty && saw_alg && saw_crv && saw_x && saw_y;
}

uint8_t zf_ctap_parse_options_map(ZfCborCursor *cursor, bool *up, bool *has_up, bool *uv,
                                  bool *has_uv, bool *rk, bool *has_rk) {
    size_t pairs = 0;
    bool saw_up = false;
    bool saw_uv = false;
    bool saw_rk = false;
    if (!zf_cbor_read_map_start(cursor, &pairs)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    for (size_t i = 0; i < pairs; ++i) {
        const uint8_t *key = NULL;
        size_t key_size = 0;
        bool value = false;

        if (!zf_cbor_read_text_ptr(cursor, &key, &key_size) || !zf_cbor_read_bool(cursor, &value)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }

        switch (zf_ctap_classify_text_key(key, key_size)) {
        case ZfCtapTextKeyUp:
            if (saw_up) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            *up = value;
            *has_up = true;
            saw_up = true;
            break;
        case ZfCtapTextKeyUv:
            if (saw_uv) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            *uv = value;
            *has_uv = true;
            saw_uv = true;
            break;
        case ZfCtapTextKeyRk:
            if (saw_rk) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            *rk = value;
            *has_rk = true;
            saw_rk = true;
            break;
        default:
            break;
        }
    }

    return ZF_CTAP_SUCCESS;
}

uint8_t zf_ctap_parse_make_credential_extensions_map(ZfCborCursor *cursor, bool *has_cred_protect,
                                                     uint8_t *cred_protect,
                                                     bool *hmac_secret_requested) {
    size_t pairs = 0;
    bool saw_cred_protect = false;
    bool saw_hmac_secret = false;

    if (!zf_cbor_read_map_start(cursor, &pairs)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    for (size_t i = 0; i < pairs; ++i) {
        const uint8_t *key = NULL;
        size_t key_size = 0;

        if (!zf_cbor_read_text_ptr(cursor, &key, &key_size)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }

        switch (zf_ctap_classify_text_key(key, key_size)) {
        case ZfCtapTextKeyCredProtect: {
            uint64_t raw = 0;
            if (saw_cred_protect || !zf_cbor_read_uint(cursor, &raw)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            if (!zf_ctap_cred_protect_value_is_valid(raw)) {
                return ZF_CTAP_ERR_INVALID_OPTION;
            }

            *has_cred_protect = true;
            *cred_protect = (uint8_t)raw;
            saw_cred_protect = true;
            continue;
        }

        case ZfCtapTextKeyHmacSecret: {
            if (saw_hmac_secret ||
                !zf_ctap_hmac_secret_parse_make_credential_request(cursor,
                                                                    hmac_secret_requested)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            saw_hmac_secret = true;
            continue;
        }

        default:
            if (!zf_cbor_skip(cursor)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            break;
        }
    }

    return ZF_CTAP_SUCCESS;
}

static bool zf_ctap_parse_credential_descriptor(ZfCborCursor *cursor,
                                                ZfCredentialDescriptor *descriptor,
                                                bool *include_entry) {
    size_t pairs = 0;
    bool saw_id = false;
    bool saw_type = false;
    bool public_key = false;

    if (!zf_cbor_read_map_start(cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        const uint8_t *key = NULL;
        size_t key_size = 0;
        if (!zf_cbor_read_text_ptr(cursor, &key, &key_size)) {
            return false;
        }

        switch (zf_ctap_classify_text_key(key, key_size)) {
        case ZfCtapTextKeyId: {
            if (saw_id) {
                return false;
            }
            const uint8_t *id_ptr = NULL;
            size_t id_size = 0;
            if (!zf_cbor_read_bytes_ptr(cursor, &id_ptr, &id_size)) {
                return false;
            }
            if (!descriptor || id_size == 0 || id_size > ZF_MAX_DESCRIPTOR_ID_LEN) {
                return false;
            }
            zf_crypto_sha256(id_ptr, id_size, descriptor->credential_id_digest);
            saw_id = true;
            continue;
        }

        case ZfCtapTextKeyType: {
            if (saw_type) {
                return false;
            }
            const uint8_t *type_ptr = NULL;
            size_t type_size = 0;
            if (!zf_cbor_read_text_ptr(cursor, &type_ptr, &type_size)) {
                return false;
            }
            saw_type = true;
            public_key = zf_ctap_classify_text_key(type_ptr, type_size) == ZfCtapTextKeyPublicKey;
            continue;
        }

        default:
            if (!zf_cbor_skip(cursor)) {
                return false;
            }
            break;
        }
    }

    *include_entry = public_key;
    return saw_id && saw_type;
}

uint8_t zf_ctap_parse_pubkey_cred_params(ZfCborCursor *cursor, bool *es256_supported) {
    size_t items = 0;
    if (!zf_cbor_read_array_start(cursor, &items)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    for (size_t i = 0; i < items; ++i) {
        size_t pairs = 0;
        int64_t alg = 0;
        bool have_alg = false;
        bool have_type = false;
        bool public_key = false;

        if (!zf_cbor_read_map_start(cursor, &pairs)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }

        for (size_t j = 0; j < pairs; ++j) {
            const uint8_t *key = NULL;
            size_t key_size = 0;
            if (!zf_cbor_read_text_ptr(cursor, &key, &key_size)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }

            switch (zf_ctap_classify_text_key(key, key_size)) {
            case ZfCtapTextKeyAlg:
                if (have_alg) {
                    return ZF_CTAP_ERR_INVALID_CBOR;
                }
                if (!zf_cbor_read_int(cursor, &alg)) {
                    return ZF_CTAP_ERR_INVALID_CBOR;
                }
                have_alg = true;
                continue;

            case ZfCtapTextKeyType: {
                if (have_type) {
                    return ZF_CTAP_ERR_INVALID_CBOR;
                }
                const uint8_t *value = NULL;
                size_t value_size = 0;
                if (!zf_cbor_read_text_ptr(cursor, &value, &value_size)) {
                    return ZF_CTAP_ERR_INVALID_CBOR;
                }
                have_type = true;
                public_key = zf_ctap_classify_text_key(value, value_size) == ZfCtapTextKeyPublicKey;
                continue;
            }

            default:
                if (!zf_cbor_skip(cursor)) {
                    return ZF_CTAP_ERR_INVALID_CBOR;
                }
                break;
            }
        }

        if (!have_alg || !have_type) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }
        if (!public_key) {
            continue;
        }
        if (alg == -7) {
            *es256_supported = true;
        }
    }

    return ZF_CTAP_SUCCESS;
}

/* Descriptor IDs are stored as SHA-256 digests in caller-provided storage. */
uint8_t zf_ctap_parse_descriptor_array(ZfCborCursor *cursor, ZfCredentialDescriptorList *list) {
    size_t items = 0;

    if (!list || !zf_cbor_read_array_start(cursor, &items)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    list->count = 0;

    for (size_t j = 0; j < items; ++j) {
        ZfCredentialDescriptor parsed;
        bool include_entry = false;

        memset(&parsed, 0, sizeof(parsed));
        if (!zf_ctap_parse_credential_descriptor(cursor, &parsed, &include_entry)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }
        if (!include_entry) {
            continue;
        }
        if (!list->entries || list->count >= list->capacity) {
            return ZF_CTAP_ERR_INVALID_PARAMETER;
        }
        for (size_t i = 0; i < list->count; ++i) {
            const ZfCredentialDescriptor *existing = &list->entries[i];
            if (memcmp(existing->credential_id_digest, parsed.credential_id_digest,
                       sizeof(existing->credential_id_digest)) == 0) {
                return ZF_CTAP_ERR_INVALID_PARAMETER;
            }
        }
        list->entries[list->count] = parsed;
        list->count++;
    }

    return ZF_CTAP_SUCCESS;
}

bool zf_ctap_descriptor_list_contains_id(const ZfCredentialDescriptorList *list,
                                         const uint8_t *credential_id, size_t credential_id_len) {
    uint8_t credential_id_digest[ZF_DESCRIPTOR_ID_DIGEST_LEN];

    if (!list || !credential_id || list->count == 0 || credential_id_len == 0 ||
        credential_id_len > ZF_CREDENTIAL_ID_LEN) {
        return false;
    }

    zf_crypto_sha256(credential_id, credential_id_len, credential_id_digest);
    for (size_t i = 0; i < list->count; ++i) {
        const ZfCredentialDescriptor *entry = &list->entries[i];
        if (memcmp(entry->credential_id_digest, credential_id_digest,
                   sizeof(entry->credential_id_digest)) == 0) {
            zf_crypto_secure_zero(credential_id_digest, sizeof(credential_id_digest));
            return true;
        }
    }

    zf_crypto_secure_zero(credential_id_digest, sizeof(credential_id_digest));
    return false;
}
