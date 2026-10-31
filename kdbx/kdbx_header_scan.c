#include "kdbx_header_scan.h"
#include "kdbx_constants.h"
#include "kdbx_kdf_params.h"
#include "sha2.h"

#include <stdio.h>
#include <string.h>

#define KDBX_HEADER_SCAN_PREFIX_BYTES 12U /* two signatures and the version */
#define KDBX_HEADER_SCAN_FIELD_BYTES  5U /* field id and 32-bit size */
#define KDBX_HEADER_SCAN_END_SIZE     4U

#define KDBX_HEADER_SCAN_SAVE_AS_KDBX4 " Save the database as KDBX 4 in KeePass or KeePassXC."

typedef struct {
    bool has_cipher;
    uint8_t cipher_uuid[16];
    bool has_compression;
    bool compression_valid;
    uint32_t compression;
    uint32_t iv_size;
    bool has_kdf;
    bool kdf_too_large;
    uint32_t kdf_size;
    uint32_t header_size; /* bytes covered by the header SHA-256 */
} KDBXHeaderScanFields;

static bool kdbx_header_scan_fail(char* error, size_t error_size, const char* message) {
    if(error != NULL && error_size > 0U) {
        snprintf(error, error_size, "%s", message);
    }
    return false;
}

static uint32_t kdbx_header_scan_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* Checks a-c: readable, KeePass signature, KDBX 4. */
static bool
    kdbx_header_scan_prefix(const KDBXHeaderScanSource* source, char* error, size_t error_size) {
    uint8_t prefix[KDBX_HEADER_SCAN_PREFIX_BYTES];

    if(!source->read(source->context, prefix, 8U)) {
        return kdbx_header_scan_fail(
            error, error_size, "This file is too short to be a KeePass database.");
    }
    const uint32_t signature1 = kdbx_header_scan_u32(prefix);
    const uint32_t signature2 = kdbx_header_scan_u32(prefix + 4);
    if(signature1 != KDBX_SIGNATURE_1 ||
       (signature2 != KDBX_SIGNATURE_2 && signature2 != KDBX_SIGNATURE_2_KDB)) {
        return kdbx_header_scan_fail(error, error_size, "Not a KeePass database.");
    }
    if(signature2 == KDBX_SIGNATURE_2_KDB) {
        return kdbx_header_scan_fail(
            error,
            error_size,
            "KeePass 1.x (.kdb) files are not supported." KDBX_HEADER_SCAN_SAVE_AS_KDBX4);
    }
    if(!source->read(source->context, prefix + 8, 4U)) {
        return kdbx_header_scan_fail(error, error_size, "The database header is truncated.");
    }
    const uint32_t version = kdbx_header_scan_u32(prefix + 8);
    if((version >> 16) != 4U) {
        if(error != NULL && error_size > 0U) {
            snprintf(
                error,
                error_size,
                "KDBX %lu.%lu is not supported." KDBX_HEADER_SCAN_SAVE_AS_KDBX4,
                (unsigned long)(version >> 16),
                (unsigned long)(version & 0xFFFFU));
        }
        return false;
    }
    return true;
}

static bool kdbx_header_scan_read_or_skip(
    const KDBXHeaderScanSource* source,
    uint8_t* out,
    uint32_t size,
    bool keep) {
    return keep ? source->read(source->context, out, size) : source->skip(source->context, size);
}

/* Check d: walk the field list, keep only what checks e-h need. */
static bool kdbx_header_scan_fields(
    const KDBXHeaderScanSource* source,
    KDBXHeaderScanFields* fields,
    uint8_t kdf[KDBX_HEADER_SCAN_KDF_MAX_BYTES],
    char* error,
    size_t error_size) {
    uint32_t consumed = KDBX_HEADER_SCAN_PREFIX_BYTES;

    for(uint32_t index = 0U; index < KDBX_HEADER_SCAN_MAX_FIELDS; index++) {
        uint8_t field_header[KDBX_HEADER_SCAN_FIELD_BYTES];
        if(!source->read(source->context, field_header, sizeof(field_header))) {
            return kdbx_header_scan_fail(error, error_size, "The database header is truncated.");
        }
        const uint8_t id = field_header[0];
        const uint32_t size = kdbx_header_scan_u32(field_header + 1);
        consumed += KDBX_HEADER_SCAN_FIELD_BYTES;
        if(size > KDBX_HEADER_SCAN_MAX_BYTES - consumed) {
            return kdbx_header_scan_fail(error, error_size, "The database header is too large.");
        }

        bool ok = true;
        switch(id) {
        case KDBX_HEADER_FIELD_ID_END: {
            uint8_t end_marker[KDBX_HEADER_SCAN_END_SIZE];
            if(size != KDBX_HEADER_SCAN_END_SIZE) {
                return kdbx_header_scan_fail(
                    error, error_size, "The database header end marker is invalid.");
            }
            if(!source->read(source->context, end_marker, sizeof(end_marker))) {
                return kdbx_header_scan_fail(
                    error, error_size, "The database header is truncated.");
            }
            fields->header_size = consumed + size;
            return true;
        }
        case KDBX_HEADER_FIELD_ID_ENCRYPTION_ALGORITHM:
            fields->has_cipher = size == sizeof(fields->cipher_uuid);
            ok = kdbx_header_scan_read_or_skip(
                source, fields->cipher_uuid, size, fields->has_cipher);
            break;
        case KDBX_HEADER_FIELD_ID_COMPRESSION_ALGORITHM: {
            uint8_t value[4];
            fields->has_compression = true;
            fields->compression_valid = size == sizeof(value);
            ok = kdbx_header_scan_read_or_skip(source, value, size, fields->compression_valid);
            fields->compression = fields->compression_valid ? kdbx_header_scan_u32(value) : 0U;
            break;
        }
        case KDBX_HEADER_FIELD_ID_ENCRYPTION_IV:
            fields->iv_size = size;
            ok = source->skip(source->context, size);
            break;
        case KDBX_HEADER_FIELD_ID_KDF_PARAMETERS:
            fields->has_kdf = true;
            fields->kdf_too_large = size > KDBX_HEADER_SCAN_KDF_MAX_BYTES;
            fields->kdf_size = fields->kdf_too_large ? 0U : size;
            ok = kdbx_header_scan_read_or_skip(source, kdf, size, !fields->kdf_too_large);
            break;
        default:
            ok = source->skip(source->context, size);
            break;
        }
        if(!ok) {
            return kdbx_header_scan_fail(error, error_size, "The database header is truncated.");
        }
        consumed += size;
    }

    return kdbx_header_scan_fail(error, error_size, "The database header has too many fields.");
}

/* Checks e-h, in that order. */
static bool kdbx_header_scan_profile(
    const KDBXHeaderScanFields* fields,
    const uint8_t* kdf,
    char* error,
    size_t error_size) {
    const bool is_aes = fields->has_cipher &&
                        memcmp(fields->cipher_uuid, KDBX_UUID_AES256, sizeof(KDBX_UUID_AES256)) ==
                            0;
    const bool is_chacha20 =
        fields->has_cipher &&
        memcmp(fields->cipher_uuid, KDBX_UUID_CHACHA20, sizeof(KDBX_UUID_CHACHA20)) == 0;
    KDBXKdfParameters kdf_parameters;

    if(!is_aes && !is_chacha20) {
        return kdbx_header_scan_fail(error, error_size, KDBX_MSG_UNSUPPORTED_CIPHER);
    }
    /* Like the pipeline, a missing compression field means no compression. */
    if(fields->has_compression &&
       (!fields->compression_valid || (fields->compression != KDBX_COMPRESSION_NONE &&
                                       fields->compression != KDBX_COMPRESSION_GZIP))) {
        return kdbx_header_scan_fail(error, error_size, KDBX_MSG_UNSUPPORTED_COMPRESSION);
    }
    if(is_aes && fields->iv_size != 16U) {
        return kdbx_header_scan_fail(error, error_size, KDBX_MSG_AES_IV_SIZE);
    }
    if(is_chacha20 && fields->iv_size != 12U) {
        return kdbx_header_scan_fail(error, error_size, KDBX_MSG_CHACHA20_NONCE_SIZE);
    }
    if(!fields->has_kdf) {
        return kdbx_header_scan_fail(
            error, error_size, kdbx_kdf_parse_message(KDBXKdfParseMissing));
    }
    if(fields->kdf_too_large) {
        return kdbx_header_scan_fail(error, error_size, "The KDF parameter block is too large.");
    }

    const KDBXKdfParseResult result =
        kdbx_kdf_parameters_parse(kdf, fields->kdf_size, &kdf_parameters);
    if(result == KDBXKdfParseOk) {
        return true;
    }
    if(error != NULL && error_size > 0U) {
        if(result == KDBXKdfParseUnsupportedVersion) {
            /* The shared message is the format string that takes the version. */
            snprintf(error, error_size, kdbx_kdf_parse_message(result), kdf_parameters.version);
        } else {
            snprintf(
                error,
                error_size,
                "%s%s",
                kdbx_kdf_parse_message(result),
                result == KDBXKdfParseArgon2 ?
                    " Change its key derivation to AES-KDF in KeePass or KeePassXC." :
                    "");
        }
    }
    return false;
}

/* Check i: the unkeyed SHA-256 KDBX 4 stores right after the header. */
static bool kdbx_header_scan_hash(
    const KDBXHeaderScanSource* source,
    uint32_t header_size,
    uint8_t chunk[KDBX_HEADER_SCAN_KDF_MAX_BYTES],
    char* error,
    size_t error_size) {
    SHA256_CTX context;
    uint8_t digest[SHA256_DIGEST_LENGTH];

    if(!source->rewind(source->context)) {
        return kdbx_header_scan_fail(error, error_size, "The database header is truncated.");
    }
    sha256_Init(&context);
    for(uint32_t remaining = header_size; remaining > 0U;) {
        const uint32_t step = remaining < KDBX_HEADER_SCAN_KDF_MAX_BYTES ?
                                  remaining :
                                  KDBX_HEADER_SCAN_KDF_MAX_BYTES;
        if(!source->read(source->context, chunk, step)) {
            return kdbx_header_scan_fail(error, error_size, "The database header is truncated.");
        }
        sha256_Update(&context, chunk, step);
        remaining -= step;
    }
    sha256_Final(&context, digest);

    if(!source->read(source->context, chunk, KDBX_HEADER_SHA256_SIZE)) {
        return kdbx_header_scan_fail(error, error_size, "The database header is truncated.");
    }
    if(memcmp(chunk, digest, KDBX_HEADER_SHA256_SIZE) != 0) {
        return kdbx_header_scan_fail(error, error_size, "The database header is damaged.");
    }
    return true;
}

bool kdbx_header_scan_with_compression(
    const KDBXHeaderScanSource* source,
    uint32_t* compression,
    char* error,
    size_t error_size) {
    KDBXHeaderScanFields fields;
    uint8_t buffer[KDBX_HEADER_SCAN_KDF_MAX_BYTES];

    if(compression != NULL) *compression = KDBX_COMPRESSION_NONE;
    if(error != NULL && error_size > 0U) {
        error[0] = '\0';
    }
    if(source == NULL || source->read == NULL || source->skip == NULL || source->rewind == NULL) {
        return kdbx_header_scan_fail(error, error_size, "The database file could not be read.");
    }

    memset(&fields, 0, sizeof(fields));
    const bool ok = kdbx_header_scan_prefix(source, error, error_size) &&
                    kdbx_header_scan_fields(source, &fields, buffer, error, error_size) &&
                    kdbx_header_scan_profile(&fields, buffer, error, error_size) &&
                    kdbx_header_scan_hash(source, fields.header_size, buffer, error, error_size);
    if(ok && compression != NULL) *compression = fields.compression;
    return ok;
}

bool kdbx_header_scan(const KDBXHeaderScanSource* source, char* error, size_t error_size) {
    return kdbx_header_scan_with_compression(source, NULL, error, error_size);
}
