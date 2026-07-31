#include "ck42x_ctap2.h"

#include "ck42x_sha256.h"
#include "vendor/micro-ecc/uECC.h"

#include <string.h>

typedef struct {
    const uint8_t* data;
    size_t length;
    size_t offset;
} Reader;

typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t length;
    bool ok;
} Writer;

static CkCtap2Platform* ck_rng_platform;

static bool read_head(Reader* reader, uint8_t* major, uint64_t* value) {
    uint8_t byte;
    unsigned extra;
    if(reader->offset >= reader->length) return false;
    byte = reader->data[reader->offset++];
    *major = byte >> 5;
    byte &= 0x1fU;
    if(byte < 24U) {
        *value = byte;
        return true;
    }
    if(byte == 24U) extra = 1;
    else if(byte == 25U) extra = 2;
    else if(byte == 26U) extra = 4;
    else return false;
    if(reader->length - reader->offset < extra) return false;
    *value = 0;
    while(extra-- != 0U) *value = (*value << 8) | reader->data[reader->offset++];
    return true;
}

static bool read_uint(Reader* reader, uint64_t* value) {
    uint8_t major;
    return read_head(reader, &major, value) && major == 0U;
}

static bool read_bytes(Reader* reader, uint8_t major_wanted, const uint8_t** data, size_t* length) {
    uint8_t major;
    uint64_t value;
    if(!read_head(reader, &major, &value) || major != major_wanted ||
       value > reader->length - reader->offset) {
        return false;
    }
    *data = reader->data + reader->offset;
    *length = (size_t)value;
    reader->offset += *length;
    return true;
}

static bool skip_item(Reader* reader, unsigned depth) {
    uint8_t major;
    uint64_t value;
    if(depth > 8U || !read_head(reader, &major, &value)) return false;
    if(major == 2U || major == 3U) {
        if(value > reader->length - reader->offset) return false;
        reader->offset += (size_t)value;
    } else if(major == 4U || major == 5U) {
        uint64_t count = major == 5U ? value * 2U : value;
        if(count > 64U) return false;
        while(count-- != 0U) {
            if(!skip_item(reader, depth + 1U)) return false;
        }
    } else if(major != 0U && major != 1U && major != 7U) {
        return false;
    }
    return true;
}

static bool enter(Reader* reader, uint8_t wanted, uint64_t* count) {
    uint8_t major;
    return read_head(reader, &major, count) && major == wanted && *count <= 32U;
}

static void put_byte(Writer* writer, uint8_t value) {
    if(!writer->ok || writer->length == writer->capacity) {
        writer->ok = false;
        return;
    }
    writer->data[writer->length++] = value;
}

static void put_data(Writer* writer, const uint8_t* data, size_t length) {
    if(!writer->ok || length > writer->capacity - writer->length) {
        writer->ok = false;
        return;
    }
    memcpy(writer->data + writer->length, data, length);
    writer->length += length;
}

static void put_head(Writer* writer, uint8_t major, size_t value) {
    if(value < 24U) put_byte(writer, (uint8_t)((major << 5) | value));
    else if(value <= 0xffU) {
        put_byte(writer, (uint8_t)((major << 5) | 24U));
        put_byte(writer, (uint8_t)value);
    } else {
        put_byte(writer, (uint8_t)((major << 5) | 25U));
        put_byte(writer, (uint8_t)(value >> 8));
        put_byte(writer, (uint8_t)value);
    }
}

static void put_uint(Writer* writer, size_t value) {
    put_head(writer, 0, value);
}

static void put_nint(Writer* writer, size_t encoded_value) {
    put_head(writer, 1, encoded_value);
}

static void put_bytes(Writer* writer, const uint8_t* data, size_t length) {
    put_head(writer, 2, length);
    put_data(writer, data, length);
}

static void put_text(Writer* writer, const char* text) {
    size_t length = strlen(text);
    put_head(writer, 3, length);
    put_data(writer, (const uint8_t*)text, length);
}

static bool fail_response(uint8_t status, uint8_t* response, size_t capacity, size_t* length) {
    if(capacity == 0U) return false;
    response[0] = status;
    *length = 1;
    return true;
}

static int rng_bridge(uint8_t* output, unsigned size) {
    return ck_rng_platform && ck_rng_platform->random &&
           ck_rng_platform->random(ck_rng_platform->context, output, size);
}

static size_t der_integer(const uint8_t input[32], uint8_t output[35]) {
    size_t first = 0;
    size_t length;
    while(first < 31U && input[first] == 0U) ++first;
    length = 32U - first;
    output[0] = 0x02U;
    if((input[first] & 0x80U) != 0U) {
        output[1] = (uint8_t)(length + 1U);
        output[2] = 0;
        memcpy(output + 3, input + first, length);
        return length + 3U;
    }
    output[1] = (uint8_t)length;
    memcpy(output + 2, input + first, length);
    return length + 2U;
}

static size_t der_signature(const uint8_t raw[64], uint8_t der[72]) {
    uint8_t integers[70];
    size_t r_length = der_integer(raw, integers);
    size_t s_length = der_integer(raw + 32, integers + r_length);
    der[0] = 0x30U;
    der[1] = (uint8_t)(r_length + s_length);
    memcpy(der + 2, integers, r_length + s_length);
    ck_secure_zero(integers, sizeof(integers));
    return r_length + s_length + 2U;
}

static bool sign_digest(CkCtap2* authenticator, const uint8_t private_key[32],
                        const uint8_t digest[32], uint8_t der[72], size_t* der_length) {
    uint8_t raw[64];
    int result;
    ck_rng_platform = &authenticator->platform;
    uECC_set_rng(rng_bridge);
    result = uECC_sign(private_key, digest, 32, raw, uECC_secp256r1());
    ck_rng_platform = NULL;
    if(!result) {
        ck_secure_zero(raw, sizeof(raw));
        return false;
    }
    *der_length = der_signature(raw, der);
    ck_secure_zero(raw, sizeof(raw));
    return true;
}

void ck_ctap2_init(CkCtap2* authenticator, const CkCtap2Platform* platform) {
    if(authenticator && platform) authenticator->platform = *platform;
}

bool ck_ctap2_credential_serialize(
    const CkCtap2Credential* credential, uint8_t* output, size_t capacity, size_t* output_length) {
    if(!credential || !output || !output_length || capacity < CK_CTAP2_CREDENTIAL_RECORD_SIZE)
        return false;
    output[0] = CK_CTAP2_CREDENTIAL_RECORD_VERSION;
    memcpy(output + 1, credential->credential_id, 32);
    memcpy(output + 33, credential->rp_id_hash, 32);
    memcpy(output + 65, credential->private_key, 32);
    output[97] = (uint8_t)(credential->sign_count >> 24);
    output[98] = (uint8_t)(credential->sign_count >> 16);
    output[99] = (uint8_t)(credential->sign_count >> 8);
    output[100] = (uint8_t)credential->sign_count;
    *output_length = CK_CTAP2_CREDENTIAL_RECORD_SIZE;
    return true;
}

bool ck_ctap2_credential_deserialize(
    CkCtap2Credential* credential, const uint8_t* input, size_t input_length) {
    if(!credential || !input || input_length != CK_CTAP2_CREDENTIAL_RECORD_SIZE ||
       input[0] != CK_CTAP2_CREDENTIAL_RECORD_VERSION) return false;
    memcpy(credential->credential_id, input + 1, 32);
    memcpy(credential->rp_id_hash, input + 33, 32);
    memcpy(credential->private_key, input + 65, 32);
    credential->sign_count = ((uint32_t)input[97] << 24) | ((uint32_t)input[98] << 16) |
                             ((uint32_t)input[99] << 8) | input[100];
    return true;
}

static uint8_t parse_make(
    const uint8_t* data, size_t length, uint8_t client_hash[32],
    const uint8_t** rp_id, size_t* rp_length,
    const uint8_t* exclude_ids[32], size_t* exclude_count) {
    Reader reader = {data, length, 0};
    uint64_t count;
    bool client_found = false, rp_found = false, user_found = false;
    bool algorithm_found = false, uv_requested = false;
    *exclude_count = 0;
    if(!enter(&reader, 5, &count)) return CK_CTAP2_ERR_INVALID_CBOR;
    while(count-- != 0U) {
        uint64_t key;
        if(!read_uint(&reader, &key)) return CK_CTAP2_ERR_INVALID_CBOR;
        if(key == 1U) {
            const uint8_t* bytes; size_t bytes_length;
            if(!read_bytes(&reader, 2, &bytes, &bytes_length) || bytes_length != 32U)
                return CK_CTAP2_ERR_INVALID_CBOR;
            memcpy(client_hash, bytes, 32); client_found = true;
        } else if(key == 2U) {
            uint64_t fields;
            if(!enter(&reader, 5, &fields)) return CK_CTAP2_ERR_INVALID_CBOR;
            while(fields-- != 0U) {
                const uint8_t* name; size_t name_length;
                if(!read_bytes(&reader, 3, &name, &name_length))
                    return CK_CTAP2_ERR_INVALID_CBOR;
                if(name_length == 2U && memcmp(name, "id", 2) == 0) {
                    if(!read_bytes(&reader, 3, rp_id, rp_length) || *rp_length == 0U ||
                       *rp_length > 253U) return CK_CTAP2_ERR_INVALID_CBOR;
                    rp_found = true;
                } else if(!skip_item(&reader, 0)) return CK_CTAP2_ERR_INVALID_CBOR;
            }
        } else if(key == 4U) {
            uint64_t entries;
            if(!enter(&reader, 4, &entries)) return CK_CTAP2_ERR_INVALID_CBOR;
            while(entries-- != 0U) {
                uint64_t fields;
                bool alg_es256 = false, type_public_key = false;
                if(!enter(&reader, 5, &fields)) return CK_CTAP2_ERR_INVALID_CBOR;
                while(fields-- != 0U) {
                    const uint8_t* name; size_t name_length;
                    if(!read_bytes(&reader, 3, &name, &name_length))
                        return CK_CTAP2_ERR_INVALID_CBOR;
                    if(name_length == 3U && memcmp(name, "alg", 3) == 0) {
                        uint8_t major; uint64_t value;
                        if(!read_head(&reader, &major, &value)) return CK_CTAP2_ERR_INVALID_CBOR;
                        if(major == 1U && value == 6U) alg_es256 = true;
                    } else if(name_length == 4U && memcmp(name, "type", 4) == 0) {
                        const uint8_t* value; size_t value_length;
                        if(!read_bytes(&reader, 3, &value, &value_length))
                            return CK_CTAP2_ERR_INVALID_CBOR;
                        type_public_key =
                            value_length == 10U && memcmp(value, "public-key", 10U) == 0;
                    } else if(!skip_item(&reader, 0)) return CK_CTAP2_ERR_INVALID_CBOR;
                }
                if(alg_es256 && type_public_key) algorithm_found = true;
            }
        } else if(key == 3U) {
            uint64_t fields;
            if(!enter(&reader, 5, &fields)) return CK_CTAP2_ERR_INVALID_CBOR;
            while(fields-- != 0U) {
                const uint8_t* name; size_t name_length;
                if(!read_bytes(&reader, 3, &name, &name_length))
                    return CK_CTAP2_ERR_INVALID_CBOR;
                if(name_length == 2U && memcmp(name, "id", 2) == 0) {
                    const uint8_t* value; size_t value_length;
                    if(!read_bytes(&reader, 2, &value, &value_length) || value_length == 0U)
                        return CK_CTAP2_ERR_INVALID_CBOR;
                    user_found = true;
                } else if(!skip_item(&reader, 0)) return CK_CTAP2_ERR_INVALID_CBOR;
            }
        } else if(key == 5U) {
            uint64_t entries;
            if(!enter(&reader, 4, &entries)) return CK_CTAP2_ERR_INVALID_CBOR;
            while(entries-- != 0U) {
                uint64_t fields;
                const uint8_t* id = NULL;
                bool type_public_key = false;
                if(!enter(&reader, 5, &fields)) return CK_CTAP2_ERR_INVALID_CBOR;
                while(fields-- != 0U) {
                    const uint8_t* name; size_t name_length;
                    if(!read_bytes(&reader, 3, &name, &name_length))
                        return CK_CTAP2_ERR_INVALID_CBOR;
                    if(name_length == 2U && memcmp(name, "id", 2) == 0) {
                        const uint8_t* value; size_t value_length;
                        if(!read_bytes(&reader, 2, &value, &value_length))
                            return CK_CTAP2_ERR_INVALID_CBOR;
                        if(value_length == CK_CTAP2_CREDENTIAL_ID_SIZE) id = value;
                    } else if(name_length == 4U && memcmp(name, "type", 4) == 0) {
                        const uint8_t* value; size_t value_length;
                        if(!read_bytes(&reader, 3, &value, &value_length))
                            return CK_CTAP2_ERR_INVALID_CBOR;
                        type_public_key =
                            value_length == 10U && memcmp(value, "public-key", 10U) == 0;
                    } else if(!skip_item(&reader, 0)) return CK_CTAP2_ERR_INVALID_CBOR;
                }
                if(id && type_public_key && *exclude_count < 32U)
                    exclude_ids[(*exclude_count)++] = id;
            }
        } else if(key == 7U) {
            uint64_t fields;
            if(!enter(&reader, 5, &fields)) return CK_CTAP2_ERR_INVALID_CBOR;
            while(fields-- != 0U) {
                const uint8_t* name; size_t name_length;
                uint8_t major; uint64_t value;
                if(!read_bytes(&reader, 3, &name, &name_length) ||
                   !read_head(&reader, &major, &value) || major != 7U)
                    return CK_CTAP2_ERR_INVALID_CBOR;
                if(name_length == 2U && memcmp(name, "uv", 2) == 0 && value == 21U)
                    uv_requested = true;
            }
        } else if(!skip_item(&reader, 0)) return CK_CTAP2_ERR_INVALID_CBOR;
    }
    if(reader.offset != reader.length) return CK_CTAP2_ERR_INVALID_CBOR;
    if(!client_found || !rp_found || !user_found) return CK_CTAP2_ERR_MISSING_PARAMETER;
    if(uv_requested) return CK_CTAP2_ERR_UNSUPPORTED_OPTION;
    return algorithm_found ? CK_CTAP2_OK : CK_CTAP2_ERR_UNSUPPORTED_ALGORITHM;
}

static uint8_t make_credential(
    CkCtap2* authenticator, const uint8_t* data, size_t length,
    uint8_t* response, size_t capacity, size_t* response_length) {
    uint8_t client_hash[32], public_key[64], auth_data[180], digest[32], signature[72], record[101];
    CkCtap2Credential credential;
    const uint8_t* exclude_ids[32];
    const uint8_t* rp_id = NULL;
    size_t rp_length = 0, auth_length = 0, signature_length = 0, record_length = 0;
    size_t exclude_count = 0;
    uint8_t status =
        parse_make(data, length, client_hash, &rp_id, &rp_length, exclude_ids, &exclude_count);
    Writer auth = {auth_data, sizeof(auth_data), 0, true};
    Writer out = {response, capacity, 0, true};
    if(status != CK_CTAP2_OK) return fail_response(status, response, capacity, response_length);
    if(!authenticator->platform.random || !authenticator->platform.user_present ||
       !authenticator->platform.save_credential)
        return fail_response(CK_CTAP2_ERR_OTHER, response, capacity, response_length);
    if(exclude_count && authenticator->platform.load_credential) {
        uint8_t existing_record[CK_CTAP2_CREDENTIAL_RECORD_SIZE];
        CkCtap2Credential existing;
        uint8_t rp_hash[32];
        ck_sha256(rp_id, rp_length, rp_hash);
        for(size_t i = 0; i < exclude_count; ++i) {
            size_t existing_length = 0;
            if(authenticator->platform.load_credential(
                   authenticator->platform.context,
                   exclude_ids[i],
                   existing_record,
                   sizeof(existing_record),
                   &existing_length) &&
               ck_ctap2_credential_deserialize(&existing, existing_record, existing_length) &&
               memcmp(existing.rp_id_hash, rp_hash, sizeof(rp_hash)) == 0) {
                if(!authenticator->platform.user_present(authenticator->platform.context)) {
                    status = authenticator->platform.was_cancelled &&
                                     authenticator->platform.was_cancelled(
                                         authenticator->platform.context) ?
                                 CK_CTAP2_ERR_KEEPALIVE_CANCEL :
                                 CK_CTAP2_ERR_OPERATION_DENIED;
                } else {
                    status = CK_CTAP2_ERR_CREDENTIAL_EXCLUDED;
                }
                ck_secure_zero(existing_record, sizeof(existing_record));
                ck_secure_zero(&existing, sizeof(existing));
                ck_secure_zero(rp_hash, sizeof(rp_hash));
                goto cleanup;
            }
        }
        ck_secure_zero(existing_record, sizeof(existing_record));
        ck_secure_zero(&existing, sizeof(existing));
        ck_secure_zero(rp_hash, sizeof(rp_hash));
    }
    if(!authenticator->platform.user_present(authenticator->platform.context)) {
        status = authenticator->platform.was_cancelled &&
                         authenticator->platform.was_cancelled(authenticator->platform.context) ?
                     CK_CTAP2_ERR_KEEPALIVE_CANCEL :
                     CK_CTAP2_ERR_OPERATION_DENIED;
        goto cleanup;
    }
    memset(&credential, 0, sizeof(credential));
    ck_sha256(rp_id, rp_length, credential.rp_id_hash);
    if(!authenticator->platform.random(
           authenticator->platform.context, credential.credential_id, 32) ||
       !authenticator->platform.random(
           authenticator->platform.context, credential.private_key, 32)) {
        status = CK_CTAP2_ERR_OTHER; goto cleanup;
    }
    ck_rng_platform = &authenticator->platform;
    uECC_set_rng(rng_bridge);
    if(!uECC_compute_public_key(credential.private_key, public_key, uECC_secp256r1())) {
        status = CK_CTAP2_ERR_OTHER; ck_rng_platform = NULL; goto cleanup;
    }
    ck_rng_platform = NULL;
    put_data(&auth, credential.rp_id_hash, 32);
    put_byte(&auth, 0x41U);
    put_data(&auth, (const uint8_t[]){0, 0, 0, 0}, 4);
    put_data(&auth, (const uint8_t[16]){0}, 16);
    put_byte(&auth, 0); put_byte(&auth, 32);
    put_data(&auth, credential.credential_id, 32);
    put_head(&auth, 5, 5);
    put_uint(&auth, 1); put_uint(&auth, 2);
    put_uint(&auth, 3); put_nint(&auth, 6);
    put_nint(&auth, 0); put_uint(&auth, 1);
    put_nint(&auth, 1); put_bytes(&auth, public_key, 32);
    put_nint(&auth, 2); put_bytes(&auth, public_key + 32, 32);
    if(!auth.ok) { status = CK_CTAP2_ERR_OTHER; goto cleanup; }
    auth_length = auth.length;
    {
        CkSha256 hash;
        ck_sha256_init(&hash);
        ck_sha256_update(&hash, auth_data, auth_length);
        ck_sha256_update(&hash, client_hash, 32);
        ck_sha256_final(&hash, digest);
    }
    if(!sign_digest(authenticator, credential.private_key, digest, signature, &signature_length)) {
        status = CK_CTAP2_ERR_OTHER; goto cleanup;
    }
    if(!ck_ctap2_credential_serialize(&credential, record, sizeof(record), &record_length) ||
       !authenticator->platform.save_credential(
           authenticator->platform.context, record, record_length)) {
        status = CK_CTAP2_ERR_KEY_STORE_FULL; goto cleanup;
    }
    put_byte(&out, CK_CTAP2_OK);
    put_head(&out, 5, 3);
    put_uint(&out, 1); put_text(&out, "packed");
    put_uint(&out, 2); put_bytes(&out, auth_data, auth_length);
    put_uint(&out, 3); put_head(&out, 5, 2);
    put_text(&out, "alg"); put_nint(&out, 6);
    put_text(&out, "sig"); put_bytes(&out, signature, signature_length);
    if(!out.ok) status = CK_CTAP2_ERR_OTHER;
    else { *response_length = out.length; status = CK_CTAP2_OK; }
cleanup:
    ck_secure_zero(client_hash, sizeof(client_hash));
    ck_secure_zero(public_key, sizeof(public_key));
    ck_secure_zero(auth_data, sizeof(auth_data));
    ck_secure_zero(digest, sizeof(digest));
    ck_secure_zero(signature, sizeof(signature));
    ck_secure_zero(record, sizeof(record));
    ck_secure_zero(&credential, sizeof(credential));
    if(status != CK_CTAP2_OK)
        return fail_response(status, response, capacity, response_length);
    return true;
}

static uint8_t parse_assertion(
    const uint8_t* data, size_t length, uint8_t client_hash[32],
    const uint8_t** rp_id, size_t* rp_length,
    const uint8_t* credential_ids[32], size_t* credential_count) {
    Reader reader = {data, length, 0};
    uint64_t count;
    bool client_found = false, rp_found = false, uv_requested = false;
    *credential_count = 0;
    if(!enter(&reader, 5, &count)) return CK_CTAP2_ERR_INVALID_CBOR;
    while(count-- != 0U) {
        uint64_t key;
        if(!read_uint(&reader, &key)) return CK_CTAP2_ERR_INVALID_CBOR;
        if(key == 1U) {
            if(!read_bytes(&reader, 3, rp_id, rp_length) || *rp_length == 0U ||
               *rp_length > 253U) return CK_CTAP2_ERR_INVALID_CBOR;
            rp_found = true;
        } else if(key == 2U) {
            const uint8_t* bytes; size_t bytes_length;
            if(!read_bytes(&reader, 2, &bytes, &bytes_length) || bytes_length != 32U)
                return CK_CTAP2_ERR_INVALID_CBOR;
            memcpy(client_hash, bytes, 32); client_found = true;
        } else if(key == 3U) {
            uint64_t entries;
            if(!enter(&reader, 4, &entries) || entries == 0U)
                return CK_CTAP2_ERR_INVALID_CBOR;
            while(entries-- != 0U) {
                uint64_t fields;
                const uint8_t* id = NULL;
                bool type_public_key = false;
                if(!enter(&reader, 5, &fields)) return CK_CTAP2_ERR_INVALID_CBOR;
                while(fields-- != 0U) {
                    const uint8_t* name; size_t name_length;
                    if(!read_bytes(&reader, 3, &name, &name_length))
                        return CK_CTAP2_ERR_INVALID_CBOR;
                    if(name_length == 2U && memcmp(name, "id", 2) == 0) {
                        const uint8_t* bytes; size_t bytes_length;
                        if(!read_bytes(&reader, 2, &bytes, &bytes_length))
                            return CK_CTAP2_ERR_INVALID_CBOR;
                        if(bytes_length == 32U) id = bytes;
                    } else if(name_length == 4U && memcmp(name, "type", 4) == 0) {
                        const uint8_t* value; size_t value_length;
                        if(!read_bytes(&reader, 3, &value, &value_length))
                            return CK_CTAP2_ERR_INVALID_CBOR;
                        type_public_key =
                            value_length == 10U && memcmp(value, "public-key", 10U) == 0;
                    } else if(!skip_item(&reader, 0)) return CK_CTAP2_ERR_INVALID_CBOR;
                }
                if(id && type_public_key && *credential_count < 32U)
                    credential_ids[(*credential_count)++] = id;
            }
        } else if(key == 5U) {
            uint64_t fields;
            if(!enter(&reader, 5, &fields)) return CK_CTAP2_ERR_INVALID_CBOR;
            while(fields-- != 0U) {
                const uint8_t* name; size_t name_length;
                uint8_t major; uint64_t value;
                if(!read_bytes(&reader, 3, &name, &name_length) ||
                   !read_head(&reader, &major, &value) || major != 7U)
                    return CK_CTAP2_ERR_INVALID_CBOR;
                if(name_length == 2U && memcmp(name, "uv", 2) == 0 && value == 21U)
                    uv_requested = true;
            }
        } else if(!skip_item(&reader, 0)) return CK_CTAP2_ERR_INVALID_CBOR;
    }
    if(reader.offset != reader.length) return CK_CTAP2_ERR_INVALID_CBOR;
    if(!client_found || !rp_found) return CK_CTAP2_ERR_MISSING_PARAMETER;
    if(uv_requested) return CK_CTAP2_ERR_UNSUPPORTED_OPTION;
    return *credential_count ? CK_CTAP2_OK : CK_CTAP2_ERR_NO_CREDENTIALS;
}

static uint8_t get_assertion(
    CkCtap2* authenticator, const uint8_t* data, size_t length,
    uint8_t* response, size_t capacity, size_t* response_length) {
    uint8_t client_hash[32], rp_hash[32], record[101], auth_data[37], digest[32], signature[72];
    CkCtap2Credential credential;
    const uint8_t *rp_id = NULL, *credential_ids[32];
    size_t rp_length = 0, record_length = 0, signature_length = 0, credential_count = 0;
    uint8_t status = parse_assertion(
        data, length, client_hash, &rp_id, &rp_length, credential_ids, &credential_count);
    Writer out = {response, capacity, 0, true};
    if(status != CK_CTAP2_OK) return fail_response(status, response, capacity, response_length);
    if(!authenticator->platform.load_credential || !authenticator->platform.user_present)
        return fail_response(CK_CTAP2_ERR_OTHER, response, capacity, response_length);
    ck_sha256(rp_id, rp_length, rp_hash);
    bool found = false;
    for(size_t i = 0; i < credential_count; ++i) {
        record_length = 0;
        if(authenticator->platform.load_credential(
               authenticator->platform.context,
               credential_ids[i],
               record,
               sizeof(record),
               &record_length) &&
           ck_ctap2_credential_deserialize(&credential, record, record_length) &&
           memcmp(rp_hash, credential.rp_id_hash, 32) == 0) {
            found = true;
            break;
        }
        ck_secure_zero(record, sizeof(record));
        ck_secure_zero(&credential, sizeof(credential));
    }
    if(!found) {
        status = CK_CTAP2_ERR_NO_CREDENTIALS; goto cleanup;
    }
    if(!authenticator->platform.user_present(authenticator->platform.context)) {
        status = authenticator->platform.was_cancelled &&
                         authenticator->platform.was_cancelled(authenticator->platform.context) ?
                     CK_CTAP2_ERR_KEEPALIVE_CANCEL :
                     CK_CTAP2_ERR_OPERATION_DENIED;
        goto cleanup;
    }
    memcpy(auth_data, rp_hash, 32);
    auth_data[32] = 0x01U;
    if(credential.sign_count != UINT32_MAX) ++credential.sign_count;
    auth_data[33] = (uint8_t)(credential.sign_count >> 24);
    auth_data[34] = (uint8_t)(credential.sign_count >> 16);
    auth_data[35] = (uint8_t)(credential.sign_count >> 8);
    auth_data[36] = (uint8_t)credential.sign_count;
    {
        CkSha256 hash;
        ck_sha256_init(&hash);
        ck_sha256_update(&hash, auth_data, sizeof(auth_data));
        ck_sha256_update(&hash, client_hash, 32);
        ck_sha256_final(&hash, digest);
    }
    if(!sign_digest(authenticator, credential.private_key, digest, signature, &signature_length)) {
        status = CK_CTAP2_ERR_OTHER; goto cleanup;
    }
    if(!authenticator->platform.save_credential ||
       !ck_ctap2_credential_serialize(&credential, record, sizeof(record), &record_length) ||
       !authenticator->platform.save_credential(
           authenticator->platform.context, record, record_length)) {
        status = CK_CTAP2_ERR_KEY_STORE_FULL; goto cleanup;
    }
    put_byte(&out, CK_CTAP2_OK);
    put_head(&out, 5, 3);
    put_uint(&out, 1); put_head(&out, 5, 2);
    put_text(&out, "id"); put_bytes(&out, credential.credential_id, 32);
    put_text(&out, "type"); put_text(&out, "public-key");
    put_uint(&out, 2); put_bytes(&out, auth_data, sizeof(auth_data));
    put_uint(&out, 3); put_bytes(&out, signature, signature_length);
    if(!out.ok) status = CK_CTAP2_ERR_OTHER;
    else { *response_length = out.length; status = CK_CTAP2_OK; }
cleanup:
    ck_secure_zero(client_hash, sizeof(client_hash)); ck_secure_zero(rp_hash, sizeof(rp_hash));
    ck_secure_zero(record, sizeof(record)); ck_secure_zero(auth_data, sizeof(auth_data));
    ck_secure_zero(digest, sizeof(digest)); ck_secure_zero(signature, sizeof(signature));
    ck_secure_zero(&credential, sizeof(credential));
    if(status != CK_CTAP2_OK)
        return fail_response(status, response, capacity, response_length);
    return true;
}

static bool get_info(uint8_t* response, size_t capacity, size_t* length) {
    Writer out = {response, capacity, 0, true};
    put_byte(&out, CK_CTAP2_OK);
    put_head(&out, 5, 4);
    put_uint(&out, 1); put_head(&out, 4, 1); put_text(&out, "FIDO_2_0");
    put_uint(&out, 3); put_bytes(&out, (const uint8_t[16]){0}, 16);
    put_uint(&out, 4); put_head(&out, 5, 2);
    put_text(&out, "rk"); put_byte(&out, 0xf4U);
    put_text(&out, "up"); put_byte(&out, 0xf5U);
    put_uint(&out, 5); put_uint(&out, 1024);
    if(!out.ok) return false;
    *length = out.length;
    return true;
}

bool ck_ctap2_handle(
    CkCtap2* authenticator, const uint8_t* request, size_t request_length,
    uint8_t* response, size_t response_capacity, size_t* response_length) {
    if(!authenticator || !request || !response || !response_length || request_length == 0U)
        return false;
    *response_length = 0;
    if(request[0] == CK_CTAP2_CMD_GET_INFO) {
        if(request_length != 1U)
            return fail_response(CK_CTAP2_ERR_INVALID_LENGTH, response, response_capacity,
                                 response_length);
        return get_info(response, response_capacity, response_length);
    }
    if(request[0] == CK_CTAP2_CMD_MAKE_CREDENTIAL)
        return make_credential(authenticator, request + 1, request_length - 1U,
                               response, response_capacity, response_length);
    if(request[0] == CK_CTAP2_CMD_GET_ASSERTION)
        return get_assertion(authenticator, request + 1, request_length - 1U,
                             response, response_capacity, response_length);
    return fail_response(
        CK_CTAP2_ERR_INVALID_COMMAND, response, response_capacity, response_length);
}
