#include "kdbx_kdf_params.h"
#include "kdbx_constants.h"

#include <string.h>

#define KDBX_KDF_DICTIONARY_VERSION 0x0100U
#define KDBX_VARIANT_TYPE_UINT32    0x04U
#define KDBX_VARIANT_TYPE_UINT64    0x05U
#define KDBX_VARIANT_TYPE_BYTES     0x42U

// AES-KDF UUID: C9D9F39A628A4460BF740D08C18A4FEA
static const uint8_t KDBX_KDF_UUID_AES[16] =
    {0xC9, 0xD9, 0xF3, 0x9A, 0x62, 0x8A, 0x44, 0x60, 0xBF, 0x74, 0x0D, 0x08, 0xC1, 0x8A, 0x4F, 0xEA};
// Argon2d UUID: EF636DDF8C29444B91F7A9A403E30A0C
static const uint8_t KDBX_KDF_UUID_ARGON2D[16] =
    {0xEF, 0x63, 0x6D, 0xDF, 0x8C, 0x29, 0x44, 0x4B, 0x91, 0xF7, 0xA9, 0xA4, 0x03, 0xE3, 0x0A, 0x0C};
// Argon2id UUID: 9E298B1956DB4773B23DFC3EC6F0A1E6
static const uint8_t KDBX_KDF_UUID_ARGON2ID[16] =
    {0x9E, 0x29, 0x8B, 0x19, 0x56, 0xDB, 0x47, 0x73, 0xB2, 0x3D, 0xFC, 0x3E, 0xC6, 0xF0, 0xA1, 0xE6};

static uint32_t kdbx_kdf_read_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool kdbx_kdf_has_bytes(const uint8_t* p, const uint8_t* end, size_t needed) {
    return (p <= end) && ((size_t)(end - p) >= needed);
}

static bool kdbx_kdf_name_equals(const uint8_t* name, uint32_t name_size, const char* expected) {
    const size_t expected_len = strlen(expected);
    return expected_len == name_size && memcmp(name, expected, name_size) == 0;
}

static bool kdbx_kdf_read_uint(
    uint8_t value_type,
    const uint8_t* value,
    uint32_t value_size,
    uint64_t* out_value) {
    if(value_type == KDBX_VARIANT_TYPE_UINT32 && value_size == 4U) {
        *out_value = kdbx_kdf_read_u32(value);
        return true;
    }
    if(value_type == KDBX_VARIANT_TYPE_UINT64 && value_size == 8U) {
        *out_value = (uint64_t)kdbx_kdf_read_u32(value) |
                     ((uint64_t)kdbx_kdf_read_u32(value + 4) << 32);
        return true;
    }
    return false;
}

KDBXKdfParseResult
    kdbx_kdf_parameters_parse(const uint8_t* data, size_t data_size, KDBXKdfParameters* out) {
    const uint8_t* p = data;
    const uint8_t* end = data + data_size;
    uint8_t kdf_uuid[16] = {0};

    memset(out, 0, sizeof(*out));
    if(data == NULL || data_size < 2U) {
        return KDBXKdfParseMissing;
    }
    if(!kdbx_kdf_has_bytes(p, end, 2U)) {
        return KDBXKdfParseTruncatedBlock;
    }

    out->version = (uint16_t)((p[1] << 8) | p[0]);
    p += 2;
    if(out->version != KDBX_KDF_DICTIONARY_VERSION) {
        return KDBXKdfParseUnsupportedVersion;
    }

    while(p < end && *p != 0x00U) {
        if(!kdbx_kdf_has_bytes(p, end, 1U + 4U)) {
            return KDBXKdfParseTruncatedEntryHeader;
        }
        const uint8_t value_type = *p++;
        const uint32_t name_size = kdbx_kdf_read_u32(p);
        p += 4;
        /* name_size + 4 cannot wrap: it is compared against the remaining size. */
        if(name_size > (size_t)(end - p) || !kdbx_kdf_has_bytes(p, end, name_size + 4U)) {
            return KDBXKdfParseTruncatedEntryName;
        }
        const uint8_t* name = p;
        p += name_size;
        const uint32_t value_size = kdbx_kdf_read_u32(p);
        p += 4;
        if(!kdbx_kdf_has_bytes(p, end, value_size)) {
            return KDBXKdfParseTruncatedEntryValue;
        }
        const uint8_t* value = p;
        p += value_size;

        if(kdbx_kdf_name_equals(name, name_size, "$UUID")) {
            if(value_type != KDBX_VARIANT_TYPE_BYTES || value_size != sizeof(kdf_uuid)) {
                return KDBXKdfParseInvalidUuid;
            }
            memcpy(kdf_uuid, value, sizeof(kdf_uuid));
        } else if(kdbx_kdf_name_equals(name, name_size, "S")) {
            if(value_type != KDBX_VARIANT_TYPE_BYTES || value_size == 0U) {
                return KDBXKdfParseInvalidSalt;
            }
            out->salt = value;
            out->salt_size = value_size;
        } else if(kdbx_kdf_name_equals(name, name_size, "R")) {
            if(!kdbx_kdf_read_uint(value_type, value, value_size, &out->rounds)) {
                return KDBXKdfParseInvalidRounds;
            }
        }
    }

    if(memcmp(kdf_uuid, KDBX_KDF_UUID_AES, sizeof(kdf_uuid)) == 0) {
        out->type = KDBXKdfTypeAes;
    } else if(
        memcmp(kdf_uuid, KDBX_KDF_UUID_ARGON2D, sizeof(kdf_uuid)) == 0 ||
        memcmp(kdf_uuid, KDBX_KDF_UUID_ARGON2ID, sizeof(kdf_uuid)) == 0) {
        out->type = KDBXKdfTypeArgon2;
        return KDBXKdfParseArgon2;
    } else {
        return KDBXKdfParseUnknownKdf;
    }

    if(out->salt == NULL) {
        return KDBXKdfParseMissingSalt;
    }
    if(out->salt_size != 32U || out->rounds == 0U) {
        return KDBXKdfParseInvalidAesParameters;
    }
    return KDBXKdfParseOk;
}

const char* kdbx_kdf_parse_message(KDBXKdfParseResult result) {
    switch(result) {
    case KDBXKdfParseOk:
        return NULL;
    case KDBXKdfParseMissing:
        return "Missing KDF parameters.";
    case KDBXKdfParseTruncatedBlock:
        return "The KDF parameter block is truncated.";
    case KDBXKdfParseUnsupportedVersion:
        return KDBX_MSG_KDF_DICTIONARY_VERSION_FORMAT;
    case KDBXKdfParseTruncatedEntryHeader:
        return "A KDF parameter header is truncated.";
    case KDBXKdfParseTruncatedEntryName:
        return "A KDF parameter name is truncated.";
    case KDBXKdfParseTruncatedEntryValue:
        return "A KDF parameter value is truncated.";
    case KDBXKdfParseInvalidUuid:
        return "The KDF UUID parameter is invalid.";
    case KDBXKdfParseInvalidSalt:
        return "The KDF salt parameter is invalid.";
    case KDBXKdfParseInvalidRounds:
        return "The AES-KDF rounds parameter is invalid.";
    case KDBXKdfParseUnknownKdf:
        return "This database uses an unsupported KDF.";
    case KDBXKdfParseArgon2:
        return KDBX_ARGON2_UNSUPPORTED_MESSAGE;
    case KDBXKdfParseMissingSalt:
        return "The KDF salt parameter is missing.";
    case KDBXKdfParseInvalidAesParameters:
    default:
        return "The AES-KDF parameters are incomplete or invalid.";
    }
}
