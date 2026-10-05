/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The Logical Data Structure, ICAO Doc 9303 part 10.
 *
 * Only the files a reader can do something useful with are decoded here:
 * EF.COM for the table of contents, DG1 for the MRZ, DG2 for the face, DG11
 * and DG12 for the additional details, DG14 and DG15 for the security
 * material, and the outer shape of EF.SOD. The rest are still read, hashed and
 * exported - they are simply shown as raw bytes.
 *
 * DG2 is the one file that will not fit in memory, so it is never parsed as a
 * whole: emrtd_lds_dg2_find_image() is given the leading bytes of the file and
 * reports where the image starts, which lets the worker stream the rest
 * straight to the SD card.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "emrtd_files.h"
#include "emrtd_mrz.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- EF.COM ------------------------------------------------------------ */

typedef struct {
    char lds_version[8]; /**< "1.7". */
    char unicode_version[8]; /**< "4.0.0". */
    EmrtdFileMask present; /**< The data groups the chip announces. */
    uint8_t tag_count;
} EmrtdEfCom;

EmrtdError emrtd_lds_parse_com(const uint8_t* data, size_t len, EmrtdEfCom* out);

/* --- DG1 --------------------------------------------------------------- */

EmrtdError emrtd_lds_parse_dg1(const uint8_t* data, size_t len, EmrtdMrz* out);

/* --- DG2 --------------------------------------------------------------- */

typedef enum {
    EmrtdImageUnknown,
    EmrtdImageJpeg,
    EmrtdImageJpeg2000,
} EmrtdImageFormat;

typedef struct {
    EmrtdImageFormat format;
    size_t offset; /**< Where the image starts within the file. */
    const char* suffix; /**< ".jpg" or ".jp2". */
} EmrtdFaceImage;

/**
 * Locate the facial image inside the beginning of DG2.
 *
 * @param[in] data  the first bytes of the file; a few hundred is plenty
 * @return false when no image signature is in range, which means the caller
 *         should feed more of the file
 */
bool emrtd_lds_dg2_find_image(const uint8_t* data, size_t len, EmrtdFaceImage* out);

/* --- DG11 and DG12, the additional details ----------------------------- */

typedef struct {
    char full_name[64];
    char personal_number[24];
    char date_of_birth[16];
    char place_of_birth[48];
    char address[64];
    char telephone[24];
    char profession[32];
    bool any;
} EmrtdDg11;

EmrtdError emrtd_lds_parse_dg11(const uint8_t* data, size_t len, EmrtdDg11* out);

typedef struct {
    char issuing_authority[48];
    char date_of_issue[16];
    char endorsements[48];
    bool any;
} EmrtdDg12;

EmrtdError emrtd_lds_parse_dg12(const uint8_t* data, size_t len, EmrtdDg12* out);

/* --- DG15, the Active Authentication public key ------------------------ */

typedef struct {
    char algorithm[24]; /**< "RSA" or "EC". */
    uint16_t key_bits;
} EmrtdDg15;

EmrtdError emrtd_lds_parse_dg15(const uint8_t* data, size_t len, EmrtdDg15* out);

/* --- EF.SOD ------------------------------------------------------------ */

/** One data group hash listed by the security object. */
typedef struct {
    uint8_t dg_number;
    uint8_t hash[64];
    uint8_t hash_len;
} EmrtdSodHash;

#define EMRTD_SOD_HASH_MAX 16

typedef struct {
    char digest_algorithm[16]; /**< "SHA-1", "SHA-256", ... */
    uint8_t digest_len; /**< 0 when the algorithm is not one we can compute. */
    bool digest_supported;
    EmrtdSodHash hashes[EMRTD_SOD_HASH_MAX];
    uint8_t hash_count;
    char signer_algorithm[24];
    bool has_certificate;
} EmrtdEfSod;

/**
 * Parse the security object far enough to check the data group hashes.
 *
 * The signature itself is not verified on the device: that needs RSA, which
 * the firmware's mbed TLS does not include, and a country signing certificate
 * store. See docs/platform.md and the README.
 */
EmrtdError emrtd_lds_parse_sod(const uint8_t* data, size_t len, EmrtdEfSod* out);

/** Find the listed hash for a data group. */
const EmrtdSodHash* emrtd_lds_sod_hash_for(const EmrtdEfSod* sod, int dg_number);

#ifdef __cplusplus
}
#endif
