/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The Logical Data Structure, ICAO Doc 9303 part 10.
 *
 * Every parser here works on a buffer it does not own and writes into fixed
 * fields the caller supplied, because the alternative - building an object
 * graph - is not available on 186 KB of heap. Nothing is fatal
 * except a file that is not the file it claims to be: a missing optional
 * element leaves its field empty and the read goes on, since a passport that
 * omits an optional data object is not a passport that is broken.
 */

#include "emrtd_lds.h"

#include <string.h>

#include "emrtd_tlv.h"

/* Outer template tags, ICAO 9303-10 table 36. */
#define EMRTD_TAG_EF_COM 0x60u
#define EMRTD_TAG_DG1    0x61u
#define EMRTD_TAG_DG2    0x75u
#define EMRTD_TAG_DG11   0x6Bu
#define EMRTD_TAG_DG12   0x6Cu
#define EMRTD_TAG_DG15   0x6Fu
#define EMRTD_TAG_EF_SOD 0x77u

/* ASN.1 universal tags used by the structures below. */
#define EMRTD_ASN1_INTEGER      0x02u
#define EMRTD_ASN1_BIT_STRING   0x03u
#define EMRTD_ASN1_OCTET_STRING 0x04u
#define EMRTD_ASN1_OID          0x06u
#define EMRTD_ASN1_SEQUENCE     0x30u
#define EMRTD_ASN1_SET          0x31u
#define EMRTD_ASN1_CONTEXT_0    0xA0u

/* --- Small shared helpers ---------------------------------------------- */

/** Append an unsigned value in decimal. Returns the new position. */
static size_t lds_append_uint(char* dst, size_t dst_size, size_t pos, unsigned value) {
    char digits[3];
    size_t count = 0;

    do {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    } while(value != 0 && count < sizeof(digits));

    while(count > 0 && pos + 1 < dst_size) {
        dst[pos++] = digits[--count];
    }
    return pos;
}

/**
 * Copy a text data object into a fixed field.
 *
 * LDS text fields separate their components with filler characters, so a run
 * of them becomes one separator and a run at either end disappears. Control
 * bytes are dropped outright: they cannot be displayed and a chip is free to
 * put anything in a field this reader only ever shows.
 *
 * @param[in] name_style true for the identifier fields, where a run of two or
 *                       more fillers divides surname from given names and a
 *                       single one divides the parts of either
 */
static void
    lds_copy_text(char* dst, size_t dst_size, const uint8_t* src, size_t len, bool name_style) {
    if(dst == NULL || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if(src == NULL) {
        return;
    }

    size_t pos = 0;
    size_t i = 0;
    while(i < len && pos + 1 < dst_size) {
        if(src[i] == '<') {
            size_t run = 0;
            while(i < len && src[i] == '<') {
                run++;
                i++;
            }
            /* Trailing filler is padding, so nothing is emitted for it. */
            if(pos == 0 || i >= len) {
                continue;
            }
            if(name_style && run == 1) {
                dst[pos++] = ' ';
            } else if(pos + 2 < dst_size) {
                dst[pos++] = ',';
                dst[pos++] = ' ';
            } else {
                break;
            }
            continue;
        }

        const uint8_t byte = src[i++];
        if(byte >= 0x20) {
            dst[pos++] = (char)byte;
        }
    }
    dst[pos] = '\0';
}

/**
 * Copy a date data object as "DD.MM.YYYY".
 *
 * ICAO 9303-10 writes the full dates of DG11 and DG12 as eight ASCII digits,
 * but chips in the field also send them as four packed BCD bytes, so both are
 * accepted and anything else is passed through as text.
 */
static void lds_copy_date(char* dst, size_t dst_size, const uint8_t* src, size_t len) {
    if(dst == NULL || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if(src == NULL) {
        return;
    }

    char digits[9];
    bool have = false;

    if(len == 8) {
        have = true;
        for(size_t i = 0; i < 8; i++) {
            if(src[i] < '0' || src[i] > '9') {
                have = false;
                break;
            }
            digits[i] = (char)src[i];
        }
    } else if(len == 4) {
        have = true;
        for(size_t i = 0; i < 4; i++) {
            const uint8_t high = (uint8_t)(src[i] >> 4);
            const uint8_t low = (uint8_t)(src[i] & 0x0F);
            if(high > 9 || low > 9) {
                have = false;
                break;
            }
            digits[i * 2] = (char)('0' + high);
            digits[i * 2 + 1] = (char)('0' + low);
        }
    }

    if(!have) {
        lds_copy_text(dst, dst_size, src, len, false);
        return;
    }

    digits[8] = '\0';
    if(dst_size < 11) {
        lds_copy_text(dst, dst_size, (const uint8_t*)digits, 8, false);
        return;
    }

    dst[0] = digits[6];
    dst[1] = digits[7];
    dst[2] = '.';
    dst[3] = digits[4];
    dst[4] = digits[5];
    dst[5] = '.';
    dst[6] = digits[0];
    dst[7] = digits[1];
    dst[8] = digits[2];
    dst[9] = digits[3];
    dst[10] = '\0';
}

/** Copy a name into a fixed field, always terminating it. */
static void lds_copy_ascii(char* dst, size_t dst_size, const char* text) {
    if(dst == NULL || dst_size == 0) {
        return;
    }
    size_t pos = 0;
    while(text[pos] != '\0' && pos + 1 < dst_size) {
        dst[pos] = text[pos];
        pos++;
    }
    dst[pos] = '\0';
}

/** True when @p node is an object identifier equal to @p oid. */
static bool lds_oid_is(const EmrtdTlv* node, const uint8_t* oid, size_t oid_len) {
    return node->tag == EMRTD_ASN1_OID && node->value_len == oid_len &&
           memcmp(node->value, oid, oid_len) == 0;
}

/* --- EF.COM ------------------------------------------------------------ */

/** Render a packed version field, "0107" becoming "1.7". */
static void lds_version_text(char* dst, size_t dst_size, const uint8_t* src, size_t len) {
    if(dst == NULL || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if(src == NULL) {
        return;
    }

    /* Two digits per component: four characters for LDS, six for Unicode. */
    const bool packed = (len == 4 || len == 6);
    if(packed) {
        for(size_t i = 0; i < len; i++) {
            if(src[i] < '0' || src[i] > '9') {
                lds_copy_text(dst, dst_size, src, len, false);
                return;
            }
        }
        size_t pos = 0;
        for(size_t i = 0; i + 1 < len; i += 2) {
            if(i > 0 && pos + 1 < dst_size) {
                dst[pos++] = '.';
            }
            const unsigned value = (unsigned)((src[i] - '0') * 10 + (src[i + 1] - '0'));
            pos = lds_append_uint(dst, dst_size, pos, value);
        }
        dst[pos] = '\0';
        return;
    }

    lds_copy_text(dst, dst_size, src, len, false);
}

EmrtdError emrtd_lds_parse_com(const uint8_t* data, size_t len, EmrtdEfCom* out) {
    if(data == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }
    memset(out, 0, sizeof(*out));

    EmrtdTlv root;
    if(!emrtd_tlv_parse_first(data, len, &root) || root.tag != EMRTD_TAG_EF_COM) {
        return EmrtdErrorParse;
    }

    EmrtdTlv node;
    if(emrtd_tlv_find(root.value, root.value_len, 0x5F01, &node)) {
        lds_version_text(out->lds_version, sizeof(out->lds_version), node.value, node.value_len);
    }
    if(emrtd_tlv_find(root.value, root.value_len, 0x5F36, &node)) {
        lds_version_text(
            out->unicode_version, sizeof(out->unicode_version), node.value, node.value_len);
    }

    /*
     * DO '5C' lists the outer template tag of every group the chip carries.
     * A tag this reader does not know is counted but cannot be selected, so
     * the mask and the count deliberately disagree in that case.
     */
    if(emrtd_tlv_find(root.value, root.value_len, 0x5C, &node)) {
        for(size_t i = 0; i < node.value_len; i++) {
            const EmrtdFileInfo* info = emrtd_file_by_tag(node.value[i]);
            if(info != NULL) {
                out->present |= EMRTD_FILE_BIT(info->id);
            }
        }
        out->tag_count = (uint8_t)(node.value_len > 255 ? 255 : node.value_len);
    }

    return EmrtdErrorNone;
}

/* --- DG1 --------------------------------------------------------------- */

EmrtdError emrtd_lds_parse_dg1(const uint8_t* data, size_t len, EmrtdMrz* out) {
    if(data == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }
    memset(out, 0, sizeof(*out));

    EmrtdTlv root;
    if(!emrtd_tlv_parse_first(data, len, &root) || root.tag != EMRTD_TAG_DG1) {
        return EmrtdErrorParse;
    }

    EmrtdTlv mrz;
    if(!emrtd_tlv_find(root.value, root.value_len, 0x5F1F, &mrz)) {
        return EmrtdErrorParse;
    }
    return emrtd_mrz_parse((const char*)mrz.value, mrz.value_len, out);
}

/* --- DG2 --------------------------------------------------------------- */

bool emrtd_lds_dg2_find_image(const uint8_t* data, size_t len, EmrtdFaceImage* out) {
    if(data == NULL || out == NULL) {
        return false;
    }

    /*
     * The image is found by its signature rather than by walking the
     * biometric templates. DG2 is streamed to the SD card in pieces that are
     * never all in memory at once, so the parser is given a prefix and has to
     * cope with headers whose layout varies between issuers - and with a
     * prefix that ends in the middle of a template.
     */
    static const uint8_t jpeg_soi[] = {0xFF, 0xD8, 0xFF};
    static const uint8_t jp2_signature[] = {
        0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A};
    static const uint8_t j2k_codestream[] = {0xFF, 0x4F, 0xFF, 0x51};

    static const struct {
        const uint8_t* magic;
        size_t magic_len;
        EmrtdImageFormat format;
        const char* suffix;
    } signatures[] = {
        {jpeg_soi, sizeof(jpeg_soi), EmrtdImageJpeg, ".jpg"},
        {jp2_signature, sizeof(jp2_signature), EmrtdImageJpeg2000, ".jp2"},
        {j2k_codestream, sizeof(j2k_codestream), EmrtdImageJpeg2000, ".jp2"},
    };

    for(size_t offset = 0; offset < len; offset++) {
        for(size_t s = 0; s < sizeof(signatures) / sizeof(signatures[0]); s++) {
            const size_t magic_len = signatures[s].magic_len;
            if(magic_len > len - offset) {
                continue;
            }
            if(memcmp(data + offset, signatures[s].magic, magic_len) == 0) {
                out->format = signatures[s].format;
                out->offset = offset;
                out->suffix = signatures[s].suffix;
                return true;
            }
        }
    }

    out->format = EmrtdImageUnknown;
    out->offset = 0;
    out->suffix = "";
    return false;
}

/* --- DG11 and DG12 ------------------------------------------------------ */

EmrtdError emrtd_lds_parse_dg11(const uint8_t* data, size_t len, EmrtdDg11* out) {
    if(data == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }
    memset(out, 0, sizeof(*out));

    EmrtdTlv root;
    if(!emrtd_tlv_parse_first(data, len, &root) || root.tag != EMRTD_TAG_DG11) {
        return EmrtdErrorParse;
    }

    EmrtdTlvIter iter;
    EmrtdTlv node;
    emrtd_tlv_iter_children(&iter, &root);

    while(emrtd_tlv_iter_next(&iter, &node)) {
        switch(node.tag) {
        case 0x5F0E: /* Full name, primary and secondary identifier. */
            lds_copy_text(
                out->full_name, sizeof(out->full_name), node.value, node.value_len, true);
            out->any = true;
            break;
        case 0x5F10: /* Personal number. */
            lds_copy_text(
                out->personal_number,
                sizeof(out->personal_number),
                node.value,
                node.value_len,
                false);
            out->any = true;
            break;
        case 0x5F2B: /* Full date of birth, where the MRZ only has two digits of year. */
            lds_copy_date(
                out->date_of_birth, sizeof(out->date_of_birth), node.value, node.value_len);
            out->any = true;
            break;
        case 0x5F11: /* Place of birth, components divided by fillers. */
            lds_copy_text(
                out->place_of_birth,
                sizeof(out->place_of_birth),
                node.value,
                node.value_len,
                false);
            out->any = true;
            break;
        case 0x5F42: /* Permanent address. */
            lds_copy_text(out->address, sizeof(out->address), node.value, node.value_len, false);
            out->any = true;
            break;
        case 0x5F12: /* Telephone. */
            lds_copy_text(
                out->telephone, sizeof(out->telephone), node.value, node.value_len, false);
            out->any = true;
            break;
        case 0x5F13: /* Profession. */
            lds_copy_text(
                out->profession, sizeof(out->profession), node.value, node.value_len, false);
            out->any = true;
            break;
        default:
            /* DO '5C' and the other optional details are left to the raw export. */
            break;
        }
    }

    return EmrtdErrorNone;
}

EmrtdError emrtd_lds_parse_dg12(const uint8_t* data, size_t len, EmrtdDg12* out) {
    if(data == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }
    memset(out, 0, sizeof(*out));

    EmrtdTlv root;
    if(!emrtd_tlv_parse_first(data, len, &root) || root.tag != EMRTD_TAG_DG12) {
        return EmrtdErrorParse;
    }

    EmrtdTlvIter iter;
    EmrtdTlv node;
    emrtd_tlv_iter_children(&iter, &root);

    while(emrtd_tlv_iter_next(&iter, &node)) {
        switch(node.tag) {
        case 0x5F19: /* Issuing authority. */
            lds_copy_text(
                out->issuing_authority,
                sizeof(out->issuing_authority),
                node.value,
                node.value_len,
                false);
            out->any = true;
            break;
        case 0x5F26: /* Date of issue. */
            lds_copy_date(
                out->date_of_issue, sizeof(out->date_of_issue), node.value, node.value_len);
            out->any = true;
            break;
        case 0x5F1B: /* Endorsements and observations. */
            lds_copy_text(
                out->endorsements, sizeof(out->endorsements), node.value, node.value_len, false);
            out->any = true;
            break;
        default:
            break;
        }
    }

    return EmrtdErrorNone;
}

/* --- DG15 --------------------------------------------------------------- */

/** Bit length of a big endian integer, leading zero padding ignored. */
static uint16_t lds_integer_bits(const uint8_t* data, size_t len) {
    size_t i = 0;
    while(i < len && data[i] == 0x00) {
        i++;
    }
    if(i >= len) {
        return 0;
    }

    uint16_t bits = (uint16_t)((len - i - 1) * 8);
    for(uint8_t top = data[i]; top != 0; top >>= 1) {
        bits++;
    }
    return bits;
}

EmrtdError emrtd_lds_parse_dg15(const uint8_t* data, size_t len, EmrtdDg15* out) {
    if(data == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }
    memset(out, 0, sizeof(*out));

    /* 1.2.840.113549.1.1.1 and 1.2.840.10045.2.1. */
    static const uint8_t oid_rsa[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
    static const uint8_t oid_ec[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01};

    EmrtdTlv root;
    if(!emrtd_tlv_parse_first(data, len, &root)) {
        return EmrtdErrorParse;
    }

    /* The template is optional here: a caller may hand over the bare key. */
    const uint8_t* der = data;
    size_t der_len = len;
    if(root.tag == EMRTD_TAG_DG15) {
        der = root.value;
        der_len = root.value_len;
        if(!emrtd_tlv_parse_first(der, der_len, &root)) {
            return EmrtdErrorParse;
        }
    }
    if(root.tag != EMRTD_ASN1_SEQUENCE) {
        return EmrtdErrorParse;
    }

    /* SubjectPublicKeyInfo ::= SEQUENCE { AlgorithmIdentifier, BIT STRING }. */
    EmrtdTlv algorithm = {0};
    EmrtdTlv key;
    if(!emrtd_tlv_find(root.value, root.value_len, EMRTD_ASN1_SEQUENCE, &algorithm) ||
       !emrtd_tlv_find(root.value, root.value_len, EMRTD_ASN1_BIT_STRING, &key)) {
        return EmrtdErrorParse;
    }

    EmrtdTlv oid;
    if(!emrtd_tlv_find(algorithm.value, algorithm.value_len, EMRTD_ASN1_OID, &oid)) {
        return EmrtdErrorParse;
    }

    /* The first content byte of a BIT STRING counts its unused trailing bits. */
    if(key.value_len < 2 || key.value[0] != 0x00) {
        return EmrtdErrorParse;
    }
    const uint8_t* key_bytes = key.value + 1;
    const size_t key_len = key.value_len - 1;

    if(lds_oid_is(&oid, oid_rsa, sizeof(oid_rsa))) {
        lds_copy_ascii(out->algorithm, sizeof(out->algorithm), "RSA");

        /* RSAPublicKey ::= SEQUENCE { modulus INTEGER, exponent INTEGER }. */
        EmrtdTlv sequence;
        EmrtdTlv modulus;
        if(emrtd_tlv_parse_first(key_bytes, key_len, &sequence) &&
           sequence.tag == EMRTD_ASN1_SEQUENCE &&
           emrtd_tlv_find(sequence.value, sequence.value_len, EMRTD_ASN1_INTEGER, &modulus)) {
            out->key_bits = lds_integer_bits(modulus.value, modulus.value_len);
        }
    } else if(lds_oid_is(&oid, oid_ec, sizeof(oid_ec))) {
        lds_copy_ascii(out->algorithm, sizeof(out->algorithm), "EC");

        /*
         * The key is the point itself in the TR-03111 uncompressed encoding,
         * so the field size, and with it the curve, follows from its length.
         */
        if(key_len >= 3 && key_bytes[0] == 0x04) {
            out->key_bits = (uint16_t)(((key_len - 1) / 2) * 8);
        }
    } else {
        lds_copy_ascii(out->algorithm, sizeof(out->algorithm), "unknown");
    }

    return EmrtdErrorNone;
}

/* --- EF.SOD ------------------------------------------------------------ */

typedef struct {
    const uint8_t* oid;
    size_t oid_len;
    const char* name;
    uint8_t digest_len;
    bool supported;
} EmrtdDigestAlgorithm;

/* 1.3.14.3.2.26 and the 2.16.840.1.101.3.4.2.x family. */
static const uint8_t oid_sha1[] = {0x2B, 0x0E, 0x03, 0x02, 0x1A};
static const uint8_t oid_sha256[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
static const uint8_t oid_sha384[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02};
static const uint8_t oid_sha512[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03};
static const uint8_t oid_sha224[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04};

/*
 * The firmware's mbed TLS is built with SHA-1 and the SHA-256 family only, so
 * a security object signed over SHA-384 or SHA-512 can be read and reported
 * but its hashes cannot be recomputed here. That is named rather than hidden:
 * digest_len stays zero, and the worker marks the groups unverified.
 */
static const EmrtdDigestAlgorithm emrtd_digest_algorithms[] = {
    {oid_sha1, sizeof(oid_sha1), "SHA-1", 20, true},
    {oid_sha224, sizeof(oid_sha224), "SHA-224", 28, true},
    {oid_sha256, sizeof(oid_sha256), "SHA-256", 32, true},
    {oid_sha384, sizeof(oid_sha384), "SHA-384", 0, false},
    {oid_sha512, sizeof(oid_sha512), "SHA-512", 0, false},
};

typedef struct {
    const uint8_t* oid;
    size_t oid_len;
    const char* name;
} EmrtdSignatureAlgorithm;

/* The PKCS#1 and ANSI X9.62 signature identifiers seen on document signers. */
static const uint8_t oid_rsa_encryption[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
static const uint8_t oid_rsa_pss[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0A};
static const uint8_t oid_sha1_rsa[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x05};
static const uint8_t oid_sha256_rsa[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B};
static const uint8_t oid_sha384_rsa[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0C};
static const uint8_t oid_sha512_rsa[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0D};
static const uint8_t oid_ecdsa_sha1[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x01};
static const uint8_t oid_ecdsa_sha224[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x01};
static const uint8_t oid_ecdsa_sha256[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02};
static const uint8_t oid_ecdsa_sha384[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x03};
static const uint8_t oid_ecdsa_sha512[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x04};

static const EmrtdSignatureAlgorithm emrtd_signature_algorithms[] = {
    {oid_rsa_encryption, sizeof(oid_rsa_encryption), "RSA PKCS#1"},
    {oid_rsa_pss, sizeof(oid_rsa_pss), "RSA-PSS"},
    {oid_sha1_rsa, sizeof(oid_sha1_rsa), "RSA SHA-1"},
    {oid_sha256_rsa, sizeof(oid_sha256_rsa), "RSA SHA-256"},
    {oid_sha384_rsa, sizeof(oid_sha384_rsa), "RSA SHA-384"},
    {oid_sha512_rsa, sizeof(oid_sha512_rsa), "RSA SHA-512"},
    {oid_ecdsa_sha1, sizeof(oid_ecdsa_sha1), "ECDSA SHA-1"},
    {oid_ecdsa_sha224, sizeof(oid_ecdsa_sha224), "ECDSA SHA-224"},
    {oid_ecdsa_sha256, sizeof(oid_ecdsa_sha256), "ECDSA SHA-256"},
    {oid_ecdsa_sha384, sizeof(oid_ecdsa_sha384), "ECDSA SHA-384"},
    {oid_ecdsa_sha512, sizeof(oid_ecdsa_sha512), "ECDSA SHA-512"},
};

/** Read the hash algorithm out of an AlgorithmIdentifier. */
static void lds_read_digest_algorithm(const EmrtdTlv* algorithm, EmrtdEfSod* out) {
    EmrtdTlv oid;
    if(!emrtd_tlv_find(algorithm->value, algorithm->value_len, EMRTD_ASN1_OID, &oid)) {
        return;
    }

    for(size_t i = 0; i < sizeof(emrtd_digest_algorithms) / sizeof(emrtd_digest_algorithms[0]);
        i++) {
        const EmrtdDigestAlgorithm* entry = &emrtd_digest_algorithms[i];
        if(lds_oid_is(&oid, entry->oid, entry->oid_len)) {
            lds_copy_ascii(out->digest_algorithm, sizeof(out->digest_algorithm), entry->name);
            out->digest_len = entry->digest_len;
            out->digest_supported = entry->supported;
            return;
        }
    }

    lds_copy_ascii(out->digest_algorithm, sizeof(out->digest_algorithm), "unknown");
}

/** Collect the per group hashes out of the DataGroupHashValues sequence. */
static void lds_read_hashes(const EmrtdTlv* list, EmrtdEfSod* out) {
    EmrtdTlvIter iter;
    EmrtdTlv entry;
    emrtd_tlv_iter_children(&iter, list);

    while(emrtd_tlv_iter_next(&iter, &entry) && out->hash_count < EMRTD_SOD_HASH_MAX) {
        if(entry.tag != EMRTD_ASN1_SEQUENCE) {
            continue;
        }

        EmrtdTlv number;
        EmrtdTlv value;
        uint32_t dg_number = 0;
        if(!emrtd_tlv_find(entry.value, entry.value_len, EMRTD_ASN1_INTEGER, &number) ||
           !emrtd_tlv_find(entry.value, entry.value_len, EMRTD_ASN1_OCTET_STRING, &value)) {
            continue;
        }
        if(!emrtd_tlv_read_uint(&number, &dg_number) || dg_number < 1 || dg_number > 16) {
            continue;
        }
        /* A digest longer than the field would be one this reader cannot hold. */
        if(value.value_len == 0 || value.value_len > sizeof(out->hashes[0].hash)) {
            continue;
        }

        EmrtdSodHash* slot = &out->hashes[out->hash_count];
        slot->dg_number = (uint8_t)dg_number;
        slot->hash_len = (uint8_t)value.value_len;
        memcpy(slot->hash, value.value, value.value_len);
        out->hash_count++;
    }
}

/** Name the algorithm the document signer used, for the report. */
static void lds_read_signer_algorithm(const EmrtdTlv* signer_infos, EmrtdEfSod* out) {
    EmrtdTlv signer;
    if(!emrtd_tlv_parse_first(signer_infos->value, signer_infos->value_len, &signer) ||
       signer.tag != EMRTD_ASN1_SEQUENCE) {
        return;
    }

    /*
     * SignerInfo ends with the signature as an OCTET STRING, and the
     * SEQUENCE immediately before it is the signature algorithm. Counting
     * from the end is what keeps this right whether or not the optional
     * signed attributes are present (RFC 5652, section 5.3).
     */
    EmrtdTlvIter iter;
    EmrtdTlv node;
    EmrtdTlv previous = {0};
    bool have_previous = false;
    emrtd_tlv_iter_children(&iter, &signer);

    while(emrtd_tlv_iter_next(&iter, &node)) {
        if(node.tag == EMRTD_ASN1_OCTET_STRING && have_previous) {
            EmrtdTlv oid;
            if(!emrtd_tlv_find(previous.value, previous.value_len, EMRTD_ASN1_OID, &oid)) {
                return;
            }
            for(size_t i = 0;
                i < sizeof(emrtd_signature_algorithms) / sizeof(emrtd_signature_algorithms[0]);
                i++) {
                const EmrtdSignatureAlgorithm* entry = &emrtd_signature_algorithms[i];
                if(lds_oid_is(&oid, entry->oid, entry->oid_len)) {
                    lds_copy_ascii(
                        out->signer_algorithm, sizeof(out->signer_algorithm), entry->name);
                    return;
                }
            }
            lds_copy_ascii(out->signer_algorithm, sizeof(out->signer_algorithm), "unknown");
            return;
        }
        if(node.tag == EMRTD_ASN1_SEQUENCE) {
            previous = node;
            have_previous = true;
        }
    }
}

EmrtdError emrtd_lds_parse_sod(const uint8_t* data, size_t len, EmrtdEfSod* out) {
    if(data == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }
    memset(out, 0, sizeof(*out));

    /* 1.2.840.113549.1.7.2 and 2.23.136.1.1.1. */
    static const uint8_t oid_signed_data[] = {
        0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x02};
    static const uint8_t oid_lds_security_object[] = {0x67, 0x81, 0x08, 0x01, 0x01, 0x01};

    EmrtdTlv node;
    if(!emrtd_tlv_parse_first(data, len, &node)) {
        return EmrtdErrorParse;
    }

    /* Strip the eMRTD template, which EF.SOD has and a bare CMS blob has not. */
    const uint8_t* cms = data;
    size_t cms_len = len;
    if(node.tag == EMRTD_TAG_EF_SOD) {
        cms = node.value;
        cms_len = node.value_len;
        if(!emrtd_tlv_parse_first(cms, cms_len, &node)) {
            return EmrtdErrorParse;
        }
    }
    if(node.tag != EMRTD_ASN1_SEQUENCE) {
        return EmrtdErrorParse;
    }

    /* ContentInfo ::= SEQUENCE { contentType OID, content [0] EXPLICIT ANY }. */
    EmrtdTlv content_type;
    if(emrtd_tlv_find(node.value, node.value_len, EMRTD_ASN1_OID, &content_type) &&
       !lds_oid_is(&content_type, oid_signed_data, sizeof(oid_signed_data))) {
        return EmrtdErrorParse;
    }

    EmrtdTlv content;
    if(!emrtd_tlv_find(node.value, node.value_len, EMRTD_ASN1_CONTEXT_0, &content)) {
        return EmrtdErrorParse;
    }

    EmrtdTlv signed_data;
    if(!emrtd_tlv_parse_first(content.value, content.value_len, &signed_data) ||
       signed_data.tag != EMRTD_ASN1_SEQUENCE) {
        return EmrtdErrorParse;
    }

    /*
     * SignedData ::= SEQUENCE { version, digestAlgorithms SET,
     *   encapContentInfo SEQUENCE, certificates [0] IMPLICIT OPTIONAL,
     *   crls [1] OPTIONAL, signerInfos SET }
     *
     * Both digestAlgorithms and signerInfos are SETs, told apart by position:
     * the first one is the algorithm list and the last is the signers.
     */
    EmrtdTlvIter iter;
    EmrtdTlv encap = {0};
    EmrtdTlv signer_infos = {0};
    bool have_encap = false;
    bool have_signers = false;
    bool first_set_seen = false;
    emrtd_tlv_iter_children(&iter, &signed_data);

    while(emrtd_tlv_iter_next(&iter, &node)) {
        if(node.tag == EMRTD_ASN1_SEQUENCE && !have_encap) {
            encap = node;
            have_encap = true;
        } else if(node.tag == EMRTD_ASN1_SET) {
            if(!first_set_seen) {
                first_set_seen = true;
            } else {
                signer_infos = node;
                have_signers = true;
            }
        } else if(node.tag == EMRTD_ASN1_CONTEXT_0) {
            /* The document signer certificate travels here when it is included. */
            out->has_certificate = node.value_len > 0;
        }
    }
    if(!have_encap) {
        return EmrtdErrorParse;
    }

    /* EncapsulatedContentInfo ::= SEQUENCE { eContentType OID, eContent [0] }. */
    EmrtdTlv econtent_type;
    if(emrtd_tlv_find(encap.value, encap.value_len, EMRTD_ASN1_OID, &econtent_type) &&
       !lds_oid_is(&econtent_type, oid_lds_security_object, sizeof(oid_lds_security_object))) {
        return EmrtdErrorParse;
    }

    EmrtdTlv econtent;
    EmrtdTlv octets;
    if(!emrtd_tlv_find(encap.value, encap.value_len, EMRTD_ASN1_CONTEXT_0, &econtent) ||
       !emrtd_tlv_find(econtent.value, econtent.value_len, EMRTD_ASN1_OCTET_STRING, &octets)) {
        return EmrtdErrorParse;
    }
    if(octets.constructed) {
        /*
         * A segmented OCTET STRING would have to be reassembled before it
         * could be parsed, and DER - which ICAO 9303-10 requires here - does
         * not allow one. Refused by name rather than parsed halfway.
         */
        return EmrtdErrorUnsupported;
    }

    /*
     * LDSSecurityObject ::= SEQUENCE { version INTEGER,
     *   hashAlgorithm AlgorithmIdentifier, dataGroupHashValues SEQUENCE OF,
     *   ldsVersionInfo OPTIONAL }
     */
    EmrtdTlv security_object;
    if(!emrtd_tlv_parse_first(octets.value, octets.value_len, &security_object) ||
       security_object.tag != EMRTD_ASN1_SEQUENCE) {
        return EmrtdErrorParse;
    }

    EmrtdTlv algorithm = {0};
    EmrtdTlv hash_list = {0};
    bool have_algorithm = false;
    bool have_hashes = false;
    emrtd_tlv_iter_children(&iter, &security_object);

    while(emrtd_tlv_iter_next(&iter, &node)) {
        if(node.tag != EMRTD_ASN1_SEQUENCE) {
            continue;
        }
        if(!have_algorithm) {
            algorithm = node;
            have_algorithm = true;
        } else if(!have_hashes) {
            hash_list = node;
            have_hashes = true;
        }
    }
    if(!have_algorithm || !have_hashes) {
        return EmrtdErrorParse;
    }

    lds_read_digest_algorithm(&algorithm, out);
    lds_read_hashes(&hash_list, out);
    if(have_signers) {
        lds_read_signer_algorithm(&signer_infos, out);
    }

    if(out->hash_count == 0) {
        /* A security object that lists nothing cannot verify anything. */
        return EmrtdErrorParse;
    }
    return EmrtdErrorNone;
}

const EmrtdSodHash* emrtd_lds_sod_hash_for(const EmrtdEfSod* sod, int dg_number) {
    if(sod == NULL || dg_number < 1 || dg_number > 16) {
        return NULL;
    }
    for(uint8_t i = 0; i < sod->hash_count && i < EMRTD_SOD_HASH_MAX; i++) {
        if(sod->hashes[i].dg_number == (uint8_t)dg_number) {
            return &sod->hashes[i];
        }
    }
    return NULL;
}
