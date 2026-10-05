/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_sim.h"

#include <stdlib.h>
#include <string.h>

#include <mbedtls/aes.h>
#include <mbedtls/des.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

#include "../crypto/emrtd_bac.h"
#include "../crypto/emrtd_ec.h"
#include "../crypto/emrtd_kdf.h"
#include "../crypto/emrtd_mac.h"
#include "../crypto/emrtd_rng.h"
#include "../protocol/emrtd_mrz.h"

#define SIM_FILE_MAX      4
#define SIM_SCALAR_SIZE   32
#define SIM_POINT_SIZE    EMRTD_EC_POINT_MAX
#define SIM_APDU_MAX      EMRTD_APDU_MAX_SIZE
#define SIM_FRAME_DEFAULT 256

/* Status words the chip uses. */
#define SIM_SW_OK               0x9000
#define SIM_SW_FILE_NOT_FOUND   0x6A82
#define SIM_SW_WRONG_LENGTH     0x6700
#define SIM_SW_NOT_SUPPORTED    0x6D00
#define SIM_SW_WRONG_PARAMETERS 0x6A80
#define SIM_SW_NO_FILE_SELECTED 0x6986
#define SIM_SW_END_OF_FILE      0x6B00
#define SIM_SW_AUTH_FAILED      0x6300
#define SIM_SW_SM_MISSING       0x6987
#define SIM_SW_SM_INCORRECT     0x6988
#define SIM_SW_CONDITIONS       0x6985

typedef struct {
    uint16_t fid;
    uint8_t sfi;
    uint8_t* data; /**< Owned. A security object runs to well over a kilobyte. */
    size_t len;
} SimFile;

/** A command APDU taken apart again on the chip's side of the wire. */
typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    const uint8_t* data;
    size_t data_len;
    size_t le; /**< 0 when the command asked for nothing. */
} SimCommand;

struct EmrtdSim {
    EmrtdSimConfig config;
    EmrtdTransceiver transceiver;

    SimFile files[SIM_FILE_MAX];
    size_t file_count;
    int selected; /**< Index into @c files, or -1. */
    bool application_selected;
    size_t exchanges;

    /* BAC. */
    uint8_t challenge[8];
    bool has_challenge;

    /* PACE. */
    const EmrtdCurve* curve;
    uint8_t oid[10];
    size_t oid_len;
    uint8_t kpi[EMRTD_KEY_MAX_SIZE];
    uint8_t nonce[EMRTD_BLOCK_MAX_SIZE];
    uint8_t mapping_key[SIM_SCALAR_SIZE];
    uint8_t agreement_key[SIM_SCALAR_SIZE];
    uint8_t mapping_shared[SIM_POINT_SIZE];
    size_t mapping_shared_len;
    uint8_t agreement_ic[SIM_POINT_SIZE];
    size_t agreement_ic_len;
    uint8_t agreement_ifd[SIM_POINT_SIZE];
    size_t agreement_ifd_len;
    bool pace_selected;

    /* The session, once an access protocol has succeeded. */
    bool sm_active;
    EmrtdCipher sm_cipher;
    uint8_t ks_enc[EMRTD_KEY_MAX_SIZE];
    uint8_t ks_mac[EMRTD_KEY_MAX_SIZE];
    uint8_t ssc[EMRTD_BLOCK_MAX_SIZE];
    const char* protocol;
};

/* --- Small helpers ------------------------------------------------------ */

static size_t sim_write_length(uint8_t* out, size_t len) {
    if(len < 0x80) {
        out[0] = (uint8_t)len;
        return 1;
    }
    out[0] = 0x81;
    out[1] = (uint8_t)len;
    return 2;
}

static size_t sim_write_do(uint8_t* out, uint8_t tag, const uint8_t* value, size_t len) {
    size_t offset = 0;
    out[offset++] = tag;
    offset += sim_write_length(out + offset, len);
    if(len != 0) {
        memcpy(out + offset, value, len);
    }
    return offset + len;
}

/** Read one single byte tag data object. Returns false at the end or on rubbish. */
static bool sim_read_do(
    const uint8_t* data,
    size_t len,
    size_t* offset,
    uint8_t* out_tag,
    const uint8_t** out_value,
    size_t* out_len) {
    size_t i = *offset;
    if(i + 2 > len) {
        return false;
    }
    const uint8_t tag = data[i++];
    size_t value_len = data[i++];
    if(value_len & 0x80) {
        /* Only the one byte long form appears here; 0x80 is the indefinite form. */
        if(value_len != 0x81 || i >= len) {
            return false;
        }
        value_len = data[i++];
    }
    if(value_len > len - i) {
        return false;
    }
    *out_tag = tag;
    *out_value = data + i;
    *out_len = value_len;
    *offset = i + value_len;
    return true;
}

static bool sim_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    return memcmp(a, b, len) == 0;
}

static size_t sim_frame_payload(uint16_t size) {
    const size_t frame = size != 0 ? size : SIM_FRAME_DEFAULT;
    return frame > 4 ? frame - 4 : 0;
}

/* --- The synthetic Logical Data Structure -------------------------------- */

/** Assemble a TD3 machine readable zone for the credentials this chip holds. */
static void sim_build_mrz(const EmrtdCredentials* credentials, char out[89]) {
    char number[10];
    const size_t number_len = strlen(credentials->document_number);
    for(size_t i = 0; i < 9; i++) {
        number[i] = i < number_len ? credentials->document_number[i] : '<';
    }
    number[9] = '\0';

    /* Line one: type, issuing state and the name. */
    memset(out, '<', 88);
    out[88] = '\0';
    memcpy(out, "P<UTOERIKSSON<<ANNA<MARIA", 25);

    /* Line two: the fields the access keys are derived from. */
    char* const line2 = out + 44;
    memcpy(line2, number, 9);
    line2[9] = emrtd_mrz_check_digit(number, 9);
    memcpy(line2 + 10, "UTO", 3);
    memcpy(line2 + 13, credentials->date_of_birth, 6);
    line2[19] = emrtd_mrz_check_digit(credentials->date_of_birth, 6);
    line2[20] = 'F';
    memcpy(line2 + 21, credentials->date_of_expiry, 6);
    line2[27] = emrtd_mrz_check_digit(credentials->date_of_expiry, 6);

    /* The composite check digit covers the upper line's data fields. */
    char composite[39];
    memcpy(composite, line2, 10);
    memcpy(composite + 10, line2 + 13, 7);
    memcpy(composite + 17, line2 + 21, 7);
    memcpy(composite + 24, line2 + 28, 15);
    line2[43] = emrtd_mrz_check_digit(composite, sizeof(composite));
}

/** @return the stored copy, so that the caller can amend it, or NULL. */
static SimFile*
    sim_add_file(EmrtdSim* sim, uint16_t fid, uint8_t sfi, const uint8_t* data, size_t len) {
    if(sim->file_count >= SIM_FILE_MAX || len == 0) {
        return NULL;
    }
    uint8_t* const copy = malloc(len);
    if(copy == NULL) {
        return NULL;
    }
    memcpy(copy, data, len);

    SimFile* const file = &sim->files[sim->file_count++];
    file->fid = fid;
    file->sfi = sfi;
    file->len = len;
    file->data = copy;
    return file;
}

/*
 * EF.SOD, the document security object.
 *
 * A security object is a CMS SignedData over a list of data group hashes, and
 * signing one needs an ASN.1 writer and a private key that this chip has no
 * use for. So the structure is a real one, taken from a synthetic passport:
 * an LDSSecurityObject naming SHA-256, entries for DG1 and DG2, and a self
 * signed document signer certificate. 1426 bytes, which is also what makes it
 * the one file here large enough to need several frames to read.
 *
 * It is real rather than filler because a filler object cannot be parsed, and
 * a chip whose EF.SOD cannot be parsed leaves emrtd_lds_parse_sod() and the
 * whole hash comparison untested: every data group would come back with no
 * hash listed, which is the one verdict that proves nothing. With this file
 * the suite can check a read end to end, EF.SOD included.
 *
 * The DG1 entry is overwritten at allocation with the hash of the DG1 this
 * chip actually serves, because that file is built from whatever credentials
 * the chip is given and a frozen hash would be wrong for all but one set. The
 * signature is not recomputed and therefore no longer covers the object. That
 * is acceptable here and nowhere else: passive authentication is out of the
 * reader's scope - see docs/capabilities.md - so no part of this project ever
 * checks it, and what the reader does need from this file, a digest algorithm
 * it recognises and a DG1 hash that is true, is exactly what it gets.
 */
static const uint8_t sim_sod_template[] = {
    0x77, 0x82, 0x05, 0x8E, 0x30, 0x82, 0x05, 0x8A, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D,
    0x01, 0x07, 0x02, 0xA0, 0x82, 0x05, 0x7B, 0x30, 0x82, 0x05, 0x77, 0x02, 0x01, 0x03, 0x31, 0x0F,
    0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x30,
    0x70, 0x06, 0x06, 0x67, 0x81, 0x08, 0x01, 0x01, 0x01, 0xA0, 0x66, 0x04, 0x64, 0x30, 0x62, 0x02,
    0x01, 0x00, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
    0x00, 0x30, 0x4E, 0x30, 0x25, 0x02, 0x01, 0x01, 0x04, 0x20, 0x2B, 0xCE, 0xB0, 0xD3, 0x1C, 0x89,
    0xD6, 0xC4, 0xDB, 0x69, 0x76, 0x4D, 0x7F, 0xCE, 0xAA, 0x21, 0x68, 0x36, 0x82, 0x13, 0x3C, 0x20,
    0x3F, 0x03, 0xDE, 0x90, 0xC0, 0x10, 0xBE, 0x9F, 0x1E, 0xBC, 0x30, 0x25, 0x02, 0x01, 0x02, 0x04,
    0x20, 0x9B, 0xD2, 0xDC, 0x02, 0x0C, 0x31, 0xEE, 0xE6, 0xAD, 0xA6, 0x25, 0x12, 0x49, 0xF7, 0xCE,
    0x74, 0x0C, 0xA5, 0x34, 0x2C, 0x65, 0x92, 0x28, 0x8F, 0x4C, 0x75, 0xD8, 0x6F, 0xE3, 0x4B, 0x5D,
    0xD1, 0xA0, 0x82, 0x03, 0x18, 0x30, 0x82, 0x03, 0x14, 0x30, 0x82, 0x01, 0xFC, 0xA0, 0x03, 0x02,
    0x01, 0x02, 0x02, 0x14, 0x19, 0x3A, 0xB3, 0xA4, 0xC1, 0xC8, 0x9C, 0xE0, 0x3D, 0xAA, 0xFD, 0xD8,
    0x64, 0x16, 0x8D, 0x2A, 0xF7, 0x53, 0x19, 0xC8, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
    0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x05, 0x00, 0x30, 0x44, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55,
    0x04, 0x06, 0x13, 0x02, 0x55, 0x54, 0x31, 0x1D, 0x30, 0x1B, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C,
    0x14, 0x54, 0x65, 0x73, 0x74, 0x20, 0x44, 0x6F, 0x63, 0x75, 0x6D, 0x65, 0x6E, 0x74, 0x20, 0x53,
    0x69, 0x67, 0x6E, 0x65, 0x72, 0x31, 0x16, 0x30, 0x14, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x0D,
    0x53, 0x79, 0x6E, 0x74, 0x68, 0x65, 0x74, 0x69, 0x63, 0x20, 0x44, 0x53, 0x43, 0x30, 0x1E, 0x17,
    0x0D, 0x32, 0x36, 0x30, 0x39, 0x31, 0x39, 0x31, 0x33, 0x34, 0x31, 0x30, 0x32, 0x5A, 0x17, 0x0D,
    0x33, 0x36, 0x30, 0x39, 0x31, 0x37, 0x31, 0x33, 0x34, 0x31, 0x30, 0x32, 0x5A, 0x30, 0x44, 0x31,
    0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x54, 0x31, 0x1D, 0x30, 0x1B,
    0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x14, 0x54, 0x65, 0x73, 0x74, 0x20, 0x44, 0x6F, 0x63, 0x75,
    0x6D, 0x65, 0x6E, 0x74, 0x20, 0x53, 0x69, 0x67, 0x6E, 0x65, 0x72, 0x31, 0x16, 0x30, 0x14, 0x06,
    0x03, 0x55, 0x04, 0x03, 0x0C, 0x0D, 0x53, 0x79, 0x6E, 0x74, 0x68, 0x65, 0x74, 0x69, 0x63, 0x20,
    0x44, 0x53, 0x43, 0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7,
    0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00, 0x30, 0x82, 0x01, 0x0A, 0x02,
    0x82, 0x01, 0x01, 0x00, 0xC2, 0x19, 0x83, 0xFE, 0x78, 0x10, 0x8C, 0x05, 0x74, 0x91, 0x74, 0xB8,
    0x4D, 0x86, 0xF7, 0x32, 0x6B, 0xB3, 0x32, 0xBB, 0xB6, 0x08, 0x77, 0x04, 0xB3, 0x8B, 0xF1, 0x5C,
    0x89, 0x66, 0xF1, 0xA0, 0x1F, 0xAD, 0xEC, 0x92, 0x93, 0x42, 0x4A, 0xCA, 0xBA, 0x02, 0x72, 0x61,
    0x5D, 0xDE, 0x8D, 0x3C, 0xCE, 0xC0, 0xCE, 0x3F, 0xF7, 0xAE, 0x0D, 0x26, 0xB2, 0x8A, 0xC3, 0x7C,
    0x88, 0x9F, 0x15, 0x2F, 0x47, 0xCD, 0x45, 0xC1, 0x57, 0x1C, 0xFF, 0xAC, 0x1F, 0xEE, 0x63, 0x4C,
    0xDF, 0x29, 0x36, 0xB6, 0xC9, 0xBA, 0x72, 0x27, 0xA2, 0x41, 0x53, 0x67, 0xB3, 0xB9, 0x85, 0x82,
    0x43, 0x5C, 0x67, 0xE1, 0xA6, 0x48, 0x4F, 0x1E, 0x29, 0xC5, 0x12, 0xB1, 0x49, 0x43, 0x29, 0x43,
    0x05, 0x36, 0xFD, 0xF0, 0xD1, 0x4C, 0xB8, 0xB6, 0xB7, 0xCE, 0x74, 0xCD, 0x2C, 0x67, 0x0E, 0xBC,
    0x8D, 0x2D, 0x73, 0xF2, 0x03, 0xCF, 0x32, 0x22, 0xC9, 0xDB, 0xD3, 0x71, 0x91, 0xD2, 0x22, 0x89,
    0x31, 0x37, 0x9D, 0x94, 0x09, 0x88, 0x6F, 0xCC, 0xEA, 0x63, 0xE3, 0xFD, 0x09, 0x45, 0x6F, 0xDA,
    0x0A, 0xF4, 0x10, 0x45, 0x01, 0x14, 0xFF, 0x67, 0x48, 0x41, 0x74, 0xD2, 0x93, 0xE8, 0xCA, 0xA7,
    0x91, 0x71, 0x75, 0x93, 0x0C, 0x47, 0x8E, 0xD5, 0xFD, 0xB3, 0x7F, 0x6E, 0x91, 0xD4, 0x1F, 0x75,
    0x62, 0xF2, 0xA0, 0x27, 0x4E, 0x0A, 0xBE, 0xE1, 0xE4, 0x13, 0x53, 0xCC, 0x38, 0x85, 0xFB, 0x51,
    0xCE, 0xDA, 0x85, 0xB2, 0xE2, 0xB4, 0xFC, 0xE1, 0xC9, 0x6E, 0xBE, 0x11, 0x0D, 0x90, 0x69, 0xB4,
    0x33, 0xA5, 0xEC, 0x75, 0x9A, 0x4C, 0x28, 0x71, 0x8A, 0x52, 0xD8, 0x65, 0xCA, 0x4D, 0x05, 0x2E,
    0x58, 0xBD, 0x57, 0x86, 0x31, 0xAE, 0x6A, 0x91, 0x53, 0xE6, 0xC8, 0xC5, 0xB4, 0x90, 0xF6, 0x20,
    0x4A, 0x1A, 0x01, 0xCD, 0x02, 0x03, 0x01, 0x00, 0x01, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48,
    0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x5C, 0x70, 0x06,
    0x37, 0x50, 0x55, 0x9E, 0x06, 0xB2, 0x23, 0x2B, 0xA6, 0x4F, 0x48, 0x72, 0xFB, 0x99, 0xDC, 0x01,
    0x40, 0x4C, 0x19, 0xC9, 0x7E, 0x3B, 0xE6, 0x0C, 0x87, 0xC8, 0x28, 0x18, 0x38, 0xC8, 0x89, 0xFC,
    0x1B, 0x0E, 0xD2, 0xB4, 0x6C, 0x61, 0x5D, 0x84, 0xD5, 0xB8, 0x30, 0x89, 0xFC, 0xCB, 0x12, 0x95,
    0x4C, 0x2C, 0x7E, 0x3B, 0x4E, 0x74, 0x47, 0xEC, 0x88, 0xC1, 0x6A, 0x75, 0xAE, 0xE6, 0x38, 0x3A,
    0x39, 0x3D, 0x46, 0xBF, 0x16, 0x67, 0x3B, 0x27, 0xE9, 0xDB, 0xA8, 0xD5, 0xCE, 0x15, 0x10, 0x23,
    0xDF, 0x1E, 0xFF, 0x23, 0xEE, 0x98, 0x94, 0xAC, 0xFD, 0x42, 0x11, 0x87, 0x57, 0x16, 0x3C, 0x84,
    0x81, 0x41, 0x9B, 0x47, 0x31, 0xBD, 0x67, 0x71, 0x9A, 0x08, 0xB6, 0x82, 0x4C, 0x7C, 0xE5, 0xDD,
    0xF5, 0x49, 0x6F, 0xA1, 0x73, 0x5E, 0x67, 0xDB, 0xE7, 0xFF, 0xD8, 0xFB, 0x34, 0x77, 0x05, 0x93,
    0x2E, 0x78, 0x1E, 0x5E, 0x70, 0x75, 0x2D, 0x59, 0x3C, 0x75, 0xD2, 0xB4, 0x95, 0x16, 0x15, 0xD5,
    0x57, 0x59, 0x5E, 0x01, 0xA4, 0x2B, 0x17, 0x27, 0xAE, 0xF9, 0xED, 0xFF, 0x82, 0x5A, 0xD8, 0x20,
    0xE5, 0x58, 0x62, 0x1F, 0xD8, 0x39, 0x55, 0x94, 0xFC, 0xE4, 0x58, 0xFF, 0xEA, 0xBD, 0xD9, 0x06,
    0xDC, 0x43, 0x06, 0xF1, 0x75, 0x51, 0xF6, 0x29, 0x67, 0x50, 0x27, 0x1F, 0x89, 0x79, 0xDE, 0xB4,
    0x37, 0x3D, 0xFE, 0x10, 0x28, 0xD5, 0xD9, 0x57, 0x6B, 0xAC, 0x97, 0xD5, 0xB5, 0xA1, 0x6F, 0x23,
    0xB0, 0x58, 0x4D, 0x47, 0x11, 0x85, 0x55, 0xB8, 0xA9, 0xE5, 0x5B, 0x16, 0x66, 0xAD, 0x0A, 0xA0,
    0x58, 0x3F, 0x2E, 0xDF, 0x5E, 0x9B, 0x8A, 0xD3, 0xF2, 0x56, 0x50, 0x3C, 0xA5, 0x36, 0x04, 0x57,
    0xC2, 0x30, 0x18, 0xC1, 0x13, 0xAD, 0xA4, 0x42, 0x71, 0xDB, 0xDB, 0xF7, 0x6C, 0x31, 0x82, 0x01,
    0xD1, 0x30, 0x82, 0x01, 0xCD, 0x02, 0x01, 0x01, 0x30, 0x5C, 0x30, 0x44, 0x31, 0x0B, 0x30, 0x09,
    0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x54, 0x31, 0x1D, 0x30, 0x1B, 0x06, 0x03, 0x55,
    0x04, 0x0A, 0x0C, 0x14, 0x54, 0x65, 0x73, 0x74, 0x20, 0x44, 0x6F, 0x63, 0x75, 0x6D, 0x65, 0x6E,
    0x74, 0x20, 0x53, 0x69, 0x67, 0x6E, 0x65, 0x72, 0x31, 0x16, 0x30, 0x14, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x0C, 0x0D, 0x53, 0x79, 0x6E, 0x74, 0x68, 0x65, 0x74, 0x69, 0x63, 0x20, 0x44, 0x53, 0x43,
    0x02, 0x14, 0x19, 0x3A, 0xB3, 0xA4, 0xC1, 0xC8, 0x9C, 0xE0, 0x3D, 0xAA, 0xFD, 0xD8, 0x64, 0x16,
    0x8D, 0x2A, 0xF7, 0x53, 0x19, 0xC8, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
    0x04, 0x02, 0x01, 0x05, 0x00, 0xA0, 0x48, 0x30, 0x15, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7,
    0x0D, 0x01, 0x09, 0x03, 0x31, 0x08, 0x06, 0x06, 0x67, 0x81, 0x08, 0x01, 0x01, 0x01, 0x30, 0x2F,
    0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x04, 0x31, 0x22, 0x04, 0x20, 0x82,
    0xD9, 0x8B, 0xE5, 0xF8, 0xC8, 0xA9, 0xF5, 0xB5, 0xA0, 0x05, 0x0F, 0x5A, 0x75, 0x04, 0x9E, 0xD2,
    0xB1, 0x32, 0xC5, 0xC9, 0xA1, 0x30, 0x2F, 0xB6, 0xE5, 0x09, 0x3D, 0xAC, 0x27, 0x0C, 0x0A, 0x30,
    0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x00, 0x26, 0x72, 0xC3, 0x25, 0x41, 0xC2, 0x1F, 0x30, 0xE8, 0xA8, 0x55, 0xE4, 0xC8, 0x51,
    0xD8, 0xEE, 0x0A, 0xB9, 0xA6, 0x54, 0x06, 0xFB, 0x26, 0xC5, 0x8F, 0x33, 0x89, 0x0F, 0xDC, 0xFC,
    0xBB, 0x8E, 0x55, 0x45, 0xEF, 0x8A, 0xCD, 0x76, 0xD2, 0xD5, 0x99, 0x8B, 0x50, 0xFE, 0xB8, 0x4C,
    0x98, 0x48, 0xF7, 0x91, 0x1E, 0x06, 0x49, 0x82, 0xE9, 0x20, 0xEC, 0xEB, 0xEE, 0xD4, 0x4C, 0xCF,
    0xE2, 0x9B, 0x07, 0x01, 0x7D, 0xB0, 0x21, 0x50, 0x22, 0x35, 0x84, 0x34, 0xDD, 0xE2, 0xCA, 0x3E,
    0x1E, 0xB6, 0x5F, 0x30, 0xAE, 0x83, 0x7F, 0x1D, 0xA3, 0x49, 0xA5, 0xB1, 0xE9, 0x14, 0x8E, 0xFB,
    0x00, 0x48, 0xCB, 0x93, 0x61, 0x9A, 0x14, 0x87, 0x10, 0x80, 0xB1, 0x4A, 0x94, 0xB1, 0x40, 0xDE,
    0x90, 0x41, 0xD6, 0x51, 0xA9, 0xB4, 0x1D, 0xCF, 0x09, 0x9E, 0x6C, 0x18, 0x07, 0xB6, 0x74, 0xA8,
    0x36, 0xFD, 0x15, 0x0B, 0x75, 0xE6, 0x96, 0x5A, 0xFC, 0xFA, 0x0E, 0x3B, 0xF5, 0xD6, 0xA7, 0x14,
    0xFE, 0x48, 0xAC, 0x6C, 0xC7, 0x34, 0xC2, 0x37, 0x0E, 0x9D, 0x97, 0xED, 0xE0, 0x00, 0x55, 0x66,
    0x63, 0x08, 0x20, 0x14, 0x06, 0x49, 0x6A, 0x11, 0xB8, 0x77, 0xBE, 0xBE, 0xFA, 0x82, 0xE8, 0x64,
    0x54, 0xD9, 0x79, 0x39, 0x9D, 0xA5, 0xFA, 0xF8, 0x09, 0x3E, 0x80, 0xCD, 0xB2, 0x83, 0xF6, 0x69,
    0x47, 0xBD, 0x68, 0x90, 0xF2, 0x58, 0x6B, 0x37, 0x97, 0x81, 0xAC, 0x87, 0x06, 0x1B, 0xA4, 0x4B,
    0x79, 0x15, 0xD7, 0x42, 0xE4, 0x1C, 0xF9, 0x2E, 0x55, 0xCE, 0x0D, 0xC5, 0xF3, 0x13, 0xBA, 0x3C,
    0x0D, 0x85, 0xE8, 0x55, 0x45, 0x42, 0xF8, 0xF9, 0x24, 0x78, 0xAC, 0x71, 0x09, 0x77, 0x2E, 0x9C,
    0x75, 0xE7, 0x7B, 0x31, 0xB0, 0x4B, 0x65, 0x63, 0x5F, 0x67, 0x0F, 0xC8, 0xFD, 0x62, 0x92, 0x65,
    0xB8, 0x07,
};

/** SHA-256, which is what the template's LDSSecurityObject announces. */
#define SIM_SOD_HASH_SIZE 32

/**
 * Overwrite the hash the security object lists for one data group.
 *
 * A DataGroupHash is SEQUENCE { INTEGER number, OCTET STRING hash }, which for
 * a 32 byte digest and a single digit number is the fixed seven byte header
 * searched for here. Both entries in the template are unique, so a search is
 * clearer than an offset that would have to be recomputed by hand if the
 * template were ever replaced.
 */
static bool sim_sod_patch_hash(uint8_t* sod, size_t len, uint8_t dg_number, const uint8_t* hash) {
    const uint8_t header[7] = {0x30, 0x25, 0x02, 0x01, dg_number, 0x04, SIM_SOD_HASH_SIZE};
    if(len < sizeof(header) + SIM_SOD_HASH_SIZE) {
        return false;
    }

    for(size_t i = 0; i + sizeof(header) + SIM_SOD_HASH_SIZE <= len; i++) {
        if(memcmp(sod + i, header, sizeof(header)) == 0) {
            memcpy(sod + i + sizeof(header), hash, SIM_SOD_HASH_SIZE);
            return true;
        }
    }
    return false;
}

/** EF.CardAccess: one PACEInfo, generic mapping over ECDH (9303-11 appendix A). */
static void sim_build_card_access(EmrtdSim* sim) {
    uint8_t cipher_byte;
    switch(sim->config.pace_cipher) {
    case EmrtdCipherTdes:
        cipher_byte = 1;
        break;
    case EmrtdCipherAes192:
        cipher_byte = 3;
        break;
    case EmrtdCipherAes256:
        cipher_byte = 4;
        break;
    default:
        cipher_byte = 2;
        break;
    }

    /* 0.4.0.127.0.7.2.2.4.2.<cipher> - id-PACE-ECDH-GM-... */
    const uint8_t oid[10] = {0x04, 0x00, 0x7F, 0x00, 0x07, 0x02, 0x02, 0x04, 0x02, cipher_byte};
    memcpy(sim->oid, oid, sizeof(oid));
    sim->oid_len = sizeof(oid);

    uint8_t sequence[32];
    size_t offset = 0;
    offset += sim_write_do(sequence + offset, 0x06, oid, sizeof(oid));
    const uint8_t version = 2;
    offset += sim_write_do(sequence + offset, 0x02, &version, 1);
    const uint8_t parameter = sim->config.pace_parameter_id;
    offset += sim_write_do(sequence + offset, 0x02, &parameter, 1);

    uint8_t set[48];
    uint8_t inner[40];
    const size_t inner_len = sim_write_do(inner, 0x30, sequence, offset);
    const size_t set_len = sim_write_do(set, 0x31, inner, inner_len);
    sim_add_file(sim, EMRTD_SIM_FID_CARD_ACCESS, 0x1C, set, set_len);
}

static void sim_build_files(EmrtdSim* sim) {
    if(sim->config.access != EmrtdSimAccessBacOnly) {
        sim_build_card_access(sim);
    }

    /* EF.COM announces the data groups that follow (9303-10, 3.11). */
    uint8_t body[48];
    size_t offset = 0;

    /* 5F01, the LDS version, and 5F36, the Unicode version, are two byte tags. */
    body[offset++] = 0x5F;
    body[offset++] = 0x01;
    body[offset++] = 4;
    memcpy(body + offset, "0107", 4);
    offset += 4;
    body[offset++] = 0x5F;
    body[offset++] = 0x36;
    body[offset++] = 6;
    memcpy(body + offset, "040000", 6);
    offset += 6;

    /* DO'5C' lists the template tag of every data group present. */
    const uint8_t tags[1] = {0x61};
    offset += sim_write_do(body + offset, 0x5C, tags, sizeof(tags));

    uint8_t com[64];
    const size_t com_len = sim_write_do(com, 0x60, body, offset);
    sim_add_file(sim, EMRTD_SIM_FID_COM, 0x1E, com, com_len);

    /* DG1 is the machine readable zone, tag 5F1F inside template 61. */
    char mrz[89];
    sim_build_mrz(&sim->config.credentials, mrz);
    uint8_t dg1_body[96];
    size_t dg1_offset = 0;
    dg1_body[dg1_offset++] = 0x5F;
    dg1_body[dg1_offset++] = 0x1F;
    dg1_offset += sim_write_length(dg1_body + dg1_offset, 88);
    memcpy(dg1_body + dg1_offset, mrz, 88);
    dg1_offset += 88;

    uint8_t dg1[128];
    const size_t dg1_len = sim_write_do(dg1, 0x61, dg1_body, dg1_offset);
    sim_add_file(sim, EMRTD_SIM_FID_DG1, 0x01, dg1, dg1_len);

    /*
     * EF.SOD, with the hash of the DG1 written just above. The digest is taken
     * over the file exactly as the chip will hand it out, tag and length
     * included, because that is what the reader hashes as the bytes stream
     * past.
     */
    SimFile* const sod =
        sim_add_file(sim, EMRTD_SIM_FID_SOD, 0x1D, sim_sod_template, sizeof(sim_sod_template));
    if(sod != NULL) {
        uint8_t digest[SIM_SOD_HASH_SIZE];
        if(mbedtls_sha256(dg1, dg1_len, digest, 0) == 0) {
            sim_sod_patch_hash(sod->data, sod->len, 1, digest);
        }
        memset(digest, 0, sizeof(digest));
    }
}

/* --- Symmetric primitives, the chip's own copies ------------------------- */

static bool sim_cipher(
    EmrtdCipher cipher,
    const uint8_t* key,
    bool encrypt,
    const uint8_t* iv_in,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    uint8_t iv[EMRTD_BLOCK_MAX_SIZE];
    memset(iv, 0, sizeof(iv));
    if(iv_in != NULL) {
        memcpy(iv, iv_in, emrtd_cipher_block_size(cipher));
    }

    bool ok;
    if(cipher == EmrtdCipherTdes) {
        mbedtls_des3_context des3;
        mbedtls_des3_init(&des3);
        ok = (encrypt ? mbedtls_des3_set2key_enc(&des3, key) :
                        mbedtls_des3_set2key_dec(&des3, key)) == 0 &&
             mbedtls_des3_crypt_cbc(
                 &des3, encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT, len, iv, in, out) ==
                 0;
        mbedtls_des3_free(&des3);
    } else {
        const unsigned int bits = (unsigned int)(emrtd_cipher_key_size(cipher) * 8);
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        ok = (encrypt ? mbedtls_aes_setkey_enc(&aes, key, bits) :
                        mbedtls_aes_setkey_dec(&aes, key, bits)) == 0 &&
             mbedtls_aes_crypt_cbc(
                 &aes, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, len, iv, in, out) == 0;
        mbedtls_aes_free(&aes);
    }
    return ok;
}

/** The Secure Messaging initialisation vector: zero for 3DES, E(KSEnc, SSC) for AES. */
static bool sim_session_iv(const EmrtdSim* sim, uint8_t iv[EMRTD_BLOCK_MAX_SIZE]) {
    memset(iv, 0, EMRTD_BLOCK_MAX_SIZE);
    if(sim->sm_cipher == EmrtdCipherTdes) {
        return true;
    }
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const unsigned int bits = (unsigned int)(emrtd_cipher_key_size(sim->sm_cipher) * 8);
    const bool ok = mbedtls_aes_setkey_enc(&aes, sim->ks_enc, bits) == 0 &&
                    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, sim->ssc, iv) == 0;
    mbedtls_aes_free(&aes);
    return ok;
}

/** The checksum of the cipher family, over data this function pads. */
static bool sim_mac(
    EmrtdCipher cipher,
    const uint8_t* key,
    uint8_t* buffer,
    size_t len,
    size_t capacity,
    uint8_t mac[8]) {
    const size_t block = emrtd_cipher_block_size(cipher);
    if(emrtd_padded_len(len, block) > capacity) {
        return false;
    }
    const size_t padded = emrtd_pad_iso9797_m2(buffer, len, block);
    if(cipher == EmrtdCipherTdes) {
        return emrtd_retail_mac(key, buffer, padded, mac);
    }
    return emrtd_aes_cmac(key, emrtd_cipher_key_size(cipher), buffer, padded, mac, 8);
}

static void sim_increment_ssc(EmrtdSim* sim) {
    const size_t block = emrtd_cipher_block_size(sim->sm_cipher);
    for(size_t i = block; i > 0; i--) {
        if(++sim->ssc[i - 1] != 0) {
            break;
        }
    }
}

/* --- Elliptic curve helpers --------------------------------------------- */

/**
 * Multiply @p point (or the generator when it is NULL) by @p scalar.
 *
 * The scalar is reduced into the group so that the fixed values this chip uses
 * stay legal whichever curve a test selects.
 */
static bool sim_ec_mul(
    const EmrtdCurve* curve,
    const uint8_t* scalar,
    size_t scalar_len,
    const uint8_t* point,
    size_t point_len,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    mbedtls_ecp_group group;
    mbedtls_mpi d;
    mbedtls_ecp_point base;
    mbedtls_ecp_point result;
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&base);
    mbedtls_ecp_point_init(&result);

    bool ok = emrtd_ec_group_load(&group, curve) == 0;
    if(ok) {
        ok = mbedtls_mpi_read_binary(&d, scalar, scalar_len) == 0 &&
             mbedtls_mpi_mod_mpi(&d, &d, &group.N) == 0 && mbedtls_mpi_cmp_int(&d, 0) != 0;
    }
    if(ok) {
        if(point == NULL) {
            ok = mbedtls_ecp_copy(&base, &group.G) == 0;
        } else {
            ok = emrtd_ec_point_read(&group, &base, point, point_len) == 0;
        }
    }
    if(ok) {
        ok = mbedtls_ecp_mul(&group, &result, &d, &base, emrtd_random_mbedtls, NULL) == 0 &&
             emrtd_ec_point_write(&group, &result, out, out_size, out_len) == 0;
    }

    mbedtls_ecp_point_free(&result);
    mbedtls_ecp_point_free(&base);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&group);
    return ok;
}

/** The mapped generator of the generic mapping: s * G + H. */
static bool sim_ec_map(
    const EmrtdCurve* curve,
    const uint8_t* s,
    size_t s_len,
    const uint8_t* h,
    size_t h_len,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    mbedtls_ecp_group group;
    mbedtls_mpi scalar;
    mbedtls_mpi one;
    mbedtls_ecp_point shared;
    mbedtls_ecp_point result;
    mbedtls_mpi_init(&scalar);
    mbedtls_mpi_init(&one);
    mbedtls_ecp_point_init(&shared);
    mbedtls_ecp_point_init(&result);

    bool ok = emrtd_ec_group_load(&group, curve) == 0;
    if(ok) {
        ok = mbedtls_mpi_read_binary(&scalar, s, s_len) == 0 && mbedtls_mpi_lset(&one, 1) == 0 &&
             emrtd_ec_point_read(&group, &shared, h, h_len) == 0 &&
             mbedtls_ecp_muladd(&group, &result, &scalar, &group.G, &one, &shared) == 0 &&
             emrtd_ec_point_write(&group, &result, out, out_size, out_len) == 0;
    }

    mbedtls_ecp_point_free(&result);
    mbedtls_ecp_point_free(&shared);
    mbedtls_mpi_free(&one);
    mbedtls_mpi_free(&scalar);
    mbedtls_ecp_group_free(&group);
    return ok;
}

/* --- The chip's own PACE ------------------------------------------------- */

/** Kpi, derived here from first principles rather than through the reader's code. */
static bool sim_pace_password_key(EmrtdSim* sim) {
    const EmrtdCredentials* const credentials = &sim->config.credentials;
    uint8_t password[20];
    size_t password_len;

    if(credentials->has_can && credentials->can[0] != '\0') {
        password_len = strlen(credentials->can);
        if(password_len > sizeof(password)) {
            return false;
        }
        memcpy(password, credentials->can, password_len);
    } else {
        char information[EMRTD_MRZ_INFO_MAX];
        if(emrtd_mrz_information(credentials, information, sizeof(information)) !=
           EmrtdErrorNone) {
            return false;
        }
        if(mbedtls_sha1((const unsigned char*)information, strlen(information), password) != 0) {
            return false;
        }
        password_len = sizeof(password);
    }
    return emrtd_kdf(
        sim->config.pace_cipher, password, password_len, EMRTD_KDF_COUNTER_PACE, sim->kpi);
}

/** The token input, 7F49 { 06 oid 86 point }, built independently of the reader. */
static size_t
    sim_token_input(const EmrtdSim* sim, const uint8_t* point, size_t point_len, uint8_t* out) {
    uint8_t inner[2 + sizeof(sim->oid) + 2 + SIM_POINT_SIZE];
    size_t offset = 0;
    offset += sim_write_do(inner + offset, 0x06, sim->oid, sim->oid_len);
    offset += sim_write_do(inner + offset, 0x86, point, point_len);

    out[0] = 0x7F;
    out[1] = 0x49;
    size_t total = 2;
    total += sim_write_length(out + total, offset);
    memcpy(out + total, inner, offset);
    return total + offset;
}

/**
 * The authentication token, ICAO 9303-11 section 4.4.3.4.
 *
 * Note that this is not the checksum Secure Messaging uses. There the input
 * SSC || M is padded with ISO/IEC 9797-1 method 2 before the MAC, for both
 * cipher families; here the token input goes to the MAC as it stands, and only
 * the Retail MAC - which cannot pad itself - needs the padding added. The
 * appendix G vector in test_pace.c pins the AES case to the byte.
 */
static bool sim_pace_token(EmrtdSim* sim, const uint8_t* point, size_t point_len, uint8_t mac[8]) {
    uint8_t input[4 + 2 + sizeof(sim->oid) + 2 + SIM_POINT_SIZE + EMRTD_BLOCK_MAX_SIZE];
    const size_t len = sim_token_input(sim, point, point_len, input);

    if(sim->config.pace_cipher == EmrtdCipherTdes) {
        const size_t padded = emrtd_pad_iso9797_m2(input, len, 8);
        return emrtd_retail_mac(sim->ks_mac, input, padded, mac);
    }
    return emrtd_aes_cmac(
        sim->ks_mac, emrtd_cipher_key_size(sim->config.pace_cipher), input, len, mac, 8);
}

/**
 * One General Authenticate round, chip side.
 *
 * Returns the status word and, on success, writes the DO 7C answer.
 */
static uint16_t sim_general_authenticate(
    EmrtdSim* sim,
    const SimCommand* command,
    uint8_t* out,
    size_t* out_len) {
    if(sim->config.access == EmrtdSimAccessBacOnly || !sim->pace_selected) {
        return SIM_SW_CONDITIONS;
    }
    if(command->data_len < 2 || command->data[0] != 0x7C) {
        return SIM_SW_WRONG_PARAMETERS;
    }

    /* Unwrap the dynamic authentication data. */
    size_t offset = 0;
    uint8_t tag = 0;
    const uint8_t* wrapper = NULL;
    size_t wrapper_len = 0;
    if(!sim_read_do(command->data, command->data_len, &offset, &tag, &wrapper, &wrapper_len)) {
        return SIM_SW_WRONG_PARAMETERS;
    }

    const uint8_t* value = NULL;
    size_t value_len = 0;
    uint8_t inner_tag = 0;
    size_t inner_offset = 0;
    if(wrapper_len != 0 &&
       !sim_read_do(wrapper, wrapper_len, &inner_offset, &inner_tag, &value, &value_len)) {
        return SIM_SW_WRONG_PARAMETERS;
    }

    uint8_t body[2 + SIM_POINT_SIZE];
    const size_t block = emrtd_cipher_block_size(sim->config.pace_cipher);

    if(wrapper_len == 0) {
        /* Step 1: hand out the nonce, encrypted under Kpi with a zero IV. */
        uint8_t encrypted[EMRTD_BLOCK_MAX_SIZE];
        if(!sim_cipher(
               sim->config.pace_cipher, sim->kpi, true, NULL, sim->nonce, block, encrypted)) {
            return SIM_SW_CONDITIONS;
        }
        const size_t body_len = sim_write_do(body, 0x80, encrypted, block);
        *out_len = sim_write_do(out, 0x7C, body, body_len);
        return SIM_SW_OK;
    }

    if(inner_tag == 0x81) {
        /* Step 2: the mapping exchange. */
        if(!sim_ec_mul(
               sim->curve,
               sim->mapping_key,
               sizeof(sim->mapping_key),
               value,
               value_len,
               sim->mapping_shared,
               sizeof(sim->mapping_shared),
               &sim->mapping_shared_len)) {
            return SIM_SW_WRONG_PARAMETERS;
        }
        uint8_t public_key[SIM_POINT_SIZE];
        size_t public_len = 0;
        if(!sim_ec_mul(
               sim->curve,
               sim->mapping_key,
               sizeof(sim->mapping_key),
               NULL,
               0,
               public_key,
               sizeof(public_key),
               &public_len)) {
            return SIM_SW_CONDITIONS;
        }
        const size_t body_len = sim_write_do(body, 0x82, public_key, public_len);
        *out_len = sim_write_do(out, 0x7C, body, body_len);
        return SIM_SW_OK;
    }

    if(inner_tag == 0x83) {
        /* Step 3: the key agreement, over the mapped generator. */
        uint8_t shared[SIM_POINT_SIZE];
        size_t shared_len = 0;
        if(!sim_ec_mul(
               sim->curve,
               sim->agreement_key,
               sizeof(sim->agreement_key),
               value,
               value_len,
               shared,
               sizeof(shared),
               &shared_len)) {
            return SIM_SW_WRONG_PARAMETERS;
        }
        if(shared_len != 1 + 2 * (size_t)sim->curve->size) {
            return SIM_SW_CONDITIONS;
        }
        if(!emrtd_kdf_enc_mac(
               sim->config.pace_cipher, shared + 1, sim->curve->size, sim->ks_enc, sim->ks_mac)) {
            return SIM_SW_CONDITIONS;
        }

        /* Remember the terminal's point: its token is taken over it. */
        if(value_len > sizeof(sim->agreement_ifd)) {
            return SIM_SW_WRONG_PARAMETERS;
        }
        memcpy(sim->agreement_ifd, value, value_len);
        sim->agreement_ifd_len = value_len;

        uint8_t generator[SIM_POINT_SIZE];
        size_t generator_len = 0;
        if(!sim_ec_map(
               sim->curve,
               sim->nonce,
               block,
               sim->mapping_shared,
               sim->mapping_shared_len,
               generator,
               sizeof(generator),
               &generator_len) ||
           !sim_ec_mul(
               sim->curve,
               sim->agreement_key,
               sizeof(sim->agreement_key),
               generator,
               generator_len,
               sim->agreement_ic,
               sizeof(sim->agreement_ic),
               &sim->agreement_ic_len)) {
            return SIM_SW_CONDITIONS;
        }

        const size_t body_len = sim_write_do(body, 0x84, sim->agreement_ic, sim->agreement_ic_len);
        *out_len = sim_write_do(out, 0x7C, body, body_len);
        return SIM_SW_OK;
    }

    if(inner_tag == 0x85) {
        /* Step 4: the tokens. */
        uint8_t expected[8];
        if(!sim_pace_token(sim, sim->agreement_ic, sim->agreement_ic_len, expected)) {
            return SIM_SW_CONDITIONS;
        }
        if(value_len != sizeof(expected) || !sim_equal(value, expected, sizeof(expected))) {
            /* 63C2 is a real chip counting down the attempts it will allow. */
            return 0x63C2;
        }

        uint8_t token[8];
        if(!sim_pace_token(sim, sim->agreement_ifd, sim->agreement_ifd_len, token)) {
            return SIM_SW_CONDITIONS;
        }

        sim->sm_active = true;
        sim->sm_cipher = sim->config.pace_cipher;
        memset(sim->ssc, 0, sizeof(sim->ssc));
        sim->protocol = "PACE";
        sim->selected = -1;

        const size_t body_len = sim_write_do(body, 0x86, token, sizeof(token));
        *out_len = sim_write_do(out, 0x7C, body, body_len);
        return SIM_SW_OK;
    }

    return SIM_SW_WRONG_PARAMETERS;
}

/* --- The chip's own BAC -------------------------------------------------- */

static uint16_t sim_external_authenticate(
    EmrtdSim* sim,
    const SimCommand* command,
    uint8_t* out,
    size_t* out_len) {
    if(sim->config.access == EmrtdSimAccessPaceOnly) {
        return SIM_SW_CONDITIONS;
    }
    if(!sim->has_challenge || command->data_len != 40) {
        return SIM_SW_CONDITIONS;
    }

    uint8_t k_enc[16];
    uint8_t k_mac[16];
    if(emrtd_bac_derive_keys(&sim->config.credentials, k_enc, k_mac) != EmrtdErrorNone) {
        return SIM_SW_CONDITIONS;
    }

    uint8_t buffer[48];
    memcpy(buffer, command->data, 32);
    uint8_t mac[8];
    if(!sim_mac(EmrtdCipherTdes, k_mac, buffer, 32, sizeof(buffer), mac)) {
        return SIM_SW_CONDITIONS;
    }
    if(!sim_equal(mac, command->data + 32, sizeof(mac))) {
        return SIM_SW_AUTH_FAILED;
    }

    uint8_t s[32];
    if(!sim_cipher(EmrtdCipherTdes, k_enc, false, NULL, command->data, 32, s)) {
        return SIM_SW_CONDITIONS;
    }
    const uint8_t* const rnd_ifd = s;
    const uint8_t* const rnd_ic = s + 8;
    const uint8_t* const k_ifd = s + 16;
    if(!sim_equal(rnd_ic, sim->challenge, 8)) {
        return SIM_SW_AUTH_FAILED;
    }

    /* The chip contributes its own half of the seed. */
    uint8_t k_ic[16];
    emrtd_random_fill(k_ic, sizeof(k_ic));

    uint8_t r[32];
    memcpy(r, sim->challenge, 8);
    memcpy(r + 8, rnd_ifd, 8);
    memcpy(r + 16, k_ic, 16);
    if(!sim_cipher(EmrtdCipherTdes, k_enc, true, NULL, r, sizeof(r), out)) {
        return SIM_SW_CONDITIONS;
    }
    memcpy(buffer, out, 32);
    if(!sim_mac(EmrtdCipherTdes, k_mac, buffer, 32, sizeof(buffer), out + 32)) {
        return SIM_SW_CONDITIONS;
    }
    *out_len = 40;

    uint8_t kseed[16];
    for(size_t i = 0; i < sizeof(kseed); i++) {
        kseed[i] = (uint8_t)(k_ifd[i] ^ k_ic[i]);
    }
    if(!emrtd_kdf_enc_mac(EmrtdCipherTdes, kseed, sizeof(kseed), sim->ks_enc, sim->ks_mac)) {
        return SIM_SW_CONDITIONS;
    }

    sim->sm_active = true;
    sim->sm_cipher = EmrtdCipherTdes;
    memset(sim->ssc, 0, sizeof(sim->ssc));
    memcpy(sim->ssc, sim->challenge + 4, 4);
    memcpy(sim->ssc + 4, rnd_ifd + 4, 4);
    sim->protocol = "BAC";
    sim->has_challenge = false;
    return SIM_SW_OK;
}

/* --- File access --------------------------------------------------------- */

static int sim_find_file(const EmrtdSim* sim, uint16_t fid) {
    for(size_t i = 0; i < sim->file_count; i++) {
        if(sim->files[i].fid == fid) {
            return (int)i;
        }
    }
    return -1;
}

static int sim_find_by_sfi(const EmrtdSim* sim, uint8_t sfi) {
    for(size_t i = 0; i < sim->file_count; i++) {
        if(sim->files[i].sfi == sfi) {
            return (int)i;
        }
    }
    return -1;
}

/** Run the command the reader really asked for, protected or not. */
static uint16_t sim_execute(
    EmrtdSim* sim,
    const SimCommand* command,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    *out_len = 0;

    if(command->ins == 0xA4) {
        if(command->p1 == 0x04) {
            /* SELECT by application identifier. */
            static const uint8_t aid[7] = {0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01};
            if(command->data_len != sizeof(aid) || !sim_equal(command->data, aid, sizeof(aid))) {
                return SIM_SW_FILE_NOT_FOUND;
            }
            sim->application_selected = true;
            sim->selected = -1;
            return SIM_SW_OK;
        }
        if(command->p1 == 0x02 && command->data_len == 2) {
            const uint16_t fid = (uint16_t)((command->data[0] << 8) | command->data[1]);
            const int index = sim_find_file(sim, fid);
            if(index < 0) {
                return SIM_SW_FILE_NOT_FOUND;
            }
            sim->selected = index;
            return SIM_SW_OK;
        }
        return SIM_SW_WRONG_PARAMETERS;
    }

    if(command->ins == 0xB0) {
        size_t offset;
        if(command->p1 & 0x80) {
            /* The short file identifier selects and reads in one command. */
            const int index = sim_find_by_sfi(sim, (uint8_t)(command->p1 & 0x1F));
            if(index < 0) {
                return SIM_SW_FILE_NOT_FOUND;
            }
            sim->selected = index;
            offset = command->p2;
        } else {
            offset = (size_t)(((command->p1 & 0x7F) << 8) | command->p2);
        }
        if(sim->selected < 0) {
            return SIM_SW_NO_FILE_SELECTED;
        }

        const SimFile* const file = &sim->files[sim->selected];
        if(offset >= file->len) {
            return SIM_SW_END_OF_FILE;
        }
        size_t length = command->le != 0 ? command->le : 256;
        if(length > file->len - offset) {
            length = file->len - offset;
        }
        if(length > out_size) {
            return SIM_SW_WRONG_LENGTH;
        }
        memcpy(out, file->data + offset, length);
        *out_len = length;
        return SIM_SW_OK;
    }

    return SIM_SW_NOT_SUPPORTED;
}

/* --- Secure Messaging, chip side ----------------------------------------- */

static uint16_t sim_handle_protected(
    EmrtdSim* sim,
    const SimCommand* command,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(!sim->sm_active) {
        return SIM_SW_SM_MISSING;
    }

    const size_t block = emrtd_cipher_block_size(sim->sm_cipher);
    sim_increment_ssc(sim);

    /* Pick the data objects apart, keeping them as they arrived. */
    const uint8_t* cryptogram = NULL;
    size_t cryptogram_len = 0;
    const uint8_t* checksum = NULL;
    size_t checksum_len = 0;
    size_t expected_len = 0;
    size_t covered = 0;

    size_t offset = 0;
    while(offset < command->data_len) {
        uint8_t tag = 0;
        const uint8_t* value = NULL;
        size_t value_len = 0;
        const size_t start = offset;
        if(!sim_read_do(command->data, command->data_len, &offset, &tag, &value, &value_len)) {
            return SIM_SW_SM_INCORRECT;
        }
        if(tag == 0x87) {
            cryptogram = value;
            cryptogram_len = value_len;
        } else if(tag == 0x97) {
            expected_len = value_len == 1 ? (value[0] == 0 ? 256 : value[0]) : 0;
        } else if(tag == 0x8E) {
            checksum = value;
            checksum_len = value_len;
            covered = start;
            break;
        }
    }
    if(checksum == NULL || checksum_len != 8) {
        return SIM_SW_SM_MISSING;
    }

    /* The checksum covers SSC, the padded header and the objects before DO'8E'. */
    uint8_t buffer[EMRTD_BLOCK_MAX_SIZE * 2 + SIM_APDU_MAX + EMRTD_BLOCK_MAX_SIZE];
    size_t len = 0;
    memcpy(buffer, sim->ssc, block);
    len += block;
    buffer[len] = command->cla;
    buffer[len + 1] = command->ins;
    buffer[len + 2] = command->p1;
    buffer[len + 3] = command->p2;
    len += emrtd_pad_iso9797_m2(buffer + len, 4, block);
    memcpy(buffer + len, command->data, covered);
    len += covered;

    uint8_t mac[8];
    if(!sim_mac(sim->sm_cipher, sim->ks_mac, buffer, len, sizeof(buffer), mac)) {
        return SIM_SW_SM_INCORRECT;
    }
    if(!sim_equal(mac, checksum, sizeof(mac))) {
        return SIM_SW_SM_INCORRECT;
    }

    /* Decrypt whatever the command carried. */
    uint8_t plain[SIM_APDU_MAX];
    size_t plain_len = 0;
    if(cryptogram != NULL) {
        if(cryptogram_len < 2 || cryptogram[0] != 0x01 || (cryptogram_len - 1) % block != 0 ||
           cryptogram_len - 1 > sizeof(plain)) {
            return SIM_SW_SM_INCORRECT;
        }
        uint8_t iv[EMRTD_BLOCK_MAX_SIZE];
        if(!sim_session_iv(sim, iv) ||
           !sim_cipher(
               sim->sm_cipher, sim->ks_enc, false, iv, cryptogram + 1, cryptogram_len - 1, plain) ||
           !emrtd_unpad_iso9797_m2(plain, cryptogram_len - 1, &plain_len)) {
            return SIM_SW_SM_INCORRECT;
        }
    }

    SimCommand inner = *command;
    inner.cla = (uint8_t)(command->cla & (uint8_t)~0x0C);
    inner.data = plain;
    inner.data_len = plain_len;
    inner.le = expected_len;

    uint8_t answer[SIM_APDU_MAX];
    size_t answer_len = 0;
    const uint16_t sw = sim_execute(sim, &inner, answer, sizeof(answer), &answer_len);

    /* Wrap the answer up again under the next counter. */
    sim_increment_ssc(sim);
    size_t written = 0;
    if(answer_len != 0) {
        uint8_t padded[SIM_APDU_MAX + EMRTD_BLOCK_MAX_SIZE];
        memcpy(padded, answer, answer_len);
        const size_t padded_len = emrtd_pad_iso9797_m2(padded, answer_len, block);
        uint8_t iv[EMRTD_BLOCK_MAX_SIZE];
        uint8_t encrypted[sizeof(padded)];
        if(!sim_session_iv(sim, iv) ||
           !sim_cipher(sim->sm_cipher, sim->ks_enc, true, iv, padded, padded_len, encrypted)) {
            return SIM_SW_SM_INCORRECT;
        }
        if(written + 3 + padded_len > out_size) {
            return SIM_SW_WRONG_LENGTH;
        }
        out[written++] = 0x87;
        written += sim_write_length(out + written, padded_len + 1);
        out[written++] = 0x01;
        memcpy(out + written, encrypted, padded_len);
        written += padded_len;
    }

    const uint8_t status[2] = {(uint8_t)(sw >> 8), (uint8_t)(sw & 0xFF)};
    if(written + 4 + 10 > out_size) {
        return SIM_SW_WRONG_LENGTH;
    }
    written += sim_write_do(out + written, 0x99, status, sizeof(status));

    len = 0;
    memcpy(buffer, sim->ssc, block);
    len += block;
    memcpy(buffer + len, out, written);
    len += written;
    if(!sim_mac(sim->sm_cipher, sim->ks_mac, buffer, len, sizeof(buffer), mac)) {
        return SIM_SW_SM_INCORRECT;
    }
    written += sim_write_do(out + written, 0x8E, mac, sizeof(mac));

    *out_len = written;
    /* The envelope always succeeds; the real answer travelled in DO'99'. */
    return SIM_SW_OK;
}

/* --- The port ------------------------------------------------------------ */

static bool sim_parse_command(const uint8_t* apdu, size_t len, SimCommand* out) {
    if(len < 4) {
        return false;
    }
    out->cla = apdu[0];
    out->ins = apdu[1];
    out->p1 = apdu[2];
    out->p2 = apdu[3];
    out->data = NULL;
    out->data_len = 0;
    out->le = 0;

    if(len == 4) {
        return true;
    }
    if(len == 5) {
        out->le = apdu[4] == 0 ? 256 : apdu[4];
        return true;
    }

    const size_t lc = apdu[4];
    if(lc == 0 || 5 + lc > len) {
        /* Extended lengths are not offered: the Flipper never needs them. */
        return false;
    }
    out->data = apdu + 5;
    out->data_len = lc;

    const size_t rest = len - 5 - lc;
    if(rest == 0) {
        return true;
    }
    if(rest != 1) {
        return false;
    }
    out->le = apdu[len - 1] == 0 ? 256 : apdu[len - 1];
    return true;
}

static EmrtdError sim_transceive(
    void* ctx,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len) {
    EmrtdSim* const sim = ctx;
    sim->exchanges++;

    uint8_t body[SIM_APDU_MAX];
    size_t body_len = 0;
    uint16_t sw;

    SimCommand command;
    if(tx_len > sim_frame_payload(sim->config.fsc)) {
        /* The command did not fit in the card's frame; a real one never saw it. */
        return EmrtdErrorProtocol;
    }
    if(!sim_parse_command(tx, tx_len, &command)) {
        sw = SIM_SW_WRONG_LENGTH;
    } else if(command.cla & 0x0C) {
        sw = sim_handle_protected(sim, &command, body, sizeof(body), &body_len);
    } else if(sim->sm_active) {
        /*
         * Once a session exists the chip expects every command inside it. A
         * plain one at that point is a reader that has lost the thread, and a
         * real chip answers 6987 rather than obliging.
         */
        sw = SIM_SW_SM_MISSING;
    } else if(command.ins == 0x84) {
        if(sim->config.access == EmrtdSimAccessPaceOnly) {
            sw = SIM_SW_CONDITIONS;
        } else {
            emrtd_random_fill(sim->challenge, sizeof(sim->challenge));
            sim->has_challenge = true;
            memcpy(body, sim->challenge, sizeof(sim->challenge));
            body_len = sizeof(sim->challenge);
            sw = SIM_SW_OK;
        }
    } else if(command.ins == 0x82) {
        sw = sim_external_authenticate(sim, &command, body, &body_len);
    } else if(command.ins == 0x22) {
        /* MSE:Set AT - the chip only offers the one protocol it announced. */
        if(sim->config.access == EmrtdSimAccessBacOnly) {
            sw = SIM_SW_CONDITIONS;
        } else if(
            command.data_len < 2 + sim->oid_len || command.data[0] != 0x80 ||
            command.data[1] != sim->oid_len ||
            !sim_equal(command.data + 2, sim->oid, sim->oid_len)) {
            sw = SIM_SW_WRONG_PARAMETERS;
        } else {
            sim->pace_selected = true;
            sw = SIM_SW_OK;
        }
    } else if(command.ins == 0x86) {
        sw = sim_general_authenticate(sim, &command, body, &body_len);
    } else {
        sw = sim_execute(sim, &command, body, sizeof(body), &body_len);
    }

    if(body_len + 2 > rx_cap) {
        return EmrtdErrorBufferTooSmall;
    }

    /*
     * The Flipper's ISO 14443-4 layer does not reassemble a chained response,
     * so a chip answer that would not fit in one reader frame is a reader bug
     * this suite must fail on rather than quietly paper over.
     */
    if(body_len + 2 > sim_frame_payload(sim->config.fsd)) {
        return EmrtdErrorProtocol;
    }

    memcpy(rx, body, body_len);
    rx[body_len] = (uint8_t)(sw >> 8);
    rx[body_len + 1] = (uint8_t)(sw & 0xFF);
    *rx_len = body_len + 2;
    return EmrtdErrorNone;
}

static const EmrtdTransceiverApi sim_api = {
    .name = "simulated eMRTD",
    .transceive = sim_transceive,
};

EmrtdSim* emrtd_sim_alloc(const EmrtdSimConfig* config) {
    EmrtdSim* const sim = calloc(1, sizeof(EmrtdSim));
    if(sim == NULL) {
        return NULL;
    }

    if(config != NULL) {
        sim->config = *config;
    } else {
        sim->config.access = EmrtdSimAccessBoth;
    }
    if(sim->config.credentials.document_number[0] == '\0') {
        strcpy(sim->config.credentials.document_number, "L898902C");
        strcpy(sim->config.credentials.date_of_birth, "690806");
        strcpy(sim->config.credentials.date_of_expiry, "300701");
    }
    if(sim->config.pace_parameter_id == 0) {
        sim->config.pace_parameter_id = 13;
    }
    if(sim->config.pace_cipher == EmrtdCipherTdes) {
        /* See the header: the zero value means "the usual one", AES-128. */
        sim->config.pace_cipher = EmrtdCipherAes128;
    }
    if(sim->config.fsc == 0) {
        sim->config.fsc = SIM_FRAME_DEFAULT;
    }
    if(sim->config.fsd == 0) {
        sim->config.fsd = SIM_FRAME_DEFAULT;
    }

    sim->selected = -1;
    sim->protocol = "";
    sim->curve = emrtd_ec_curve_by_param_id(sim->config.pace_parameter_id);

    /*
     * Fixed ephemeral values, so that a failing run can be repeated. A real
     * chip draws these; nothing in the reader may depend on them.
     */
    for(size_t i = 0; i < sizeof(sim->nonce); i++) {
        sim->nonce[i] = (uint8_t)i;
    }
    for(size_t i = 0; i < SIM_SCALAR_SIZE; i++) {
        sim->mapping_key[i] = (uint8_t)(0x1F + i);
        sim->agreement_key[i] = (uint8_t)(0x0B + 3 * i);
    }

    sim_build_files(sim);
    if(sim->config.access != EmrtdSimAccessBacOnly && !sim_pace_password_key(sim)) {
        emrtd_sim_free(sim);
        return NULL;
    }

    sim->transceiver.api = &sim_api;
    sim->transceiver.ctx = sim;
    sim->transceiver.fsc = sim->config.fsc;
    sim->transceiver.fsd = sim->config.fsd;
    return sim;
}

void emrtd_sim_free(EmrtdSim* sim) {
    if(sim == NULL) {
        return;
    }
    for(size_t i = 0; i < sim->file_count; i++) {
        free(sim->files[i].data);
    }
    memset(sim, 0, sizeof(*sim));
    free(sim);
}

EmrtdTransceiver* emrtd_sim_transceiver(EmrtdSim* sim) {
    return sim != NULL ? &sim->transceiver : NULL;
}

const uint8_t* emrtd_sim_file(const EmrtdSim* sim, uint16_t fid, size_t* out_len) {
    if(sim == NULL) {
        return NULL;
    }
    const int index = sim_find_file(sim, fid);
    if(index < 0) {
        return NULL;
    }
    if(out_len != NULL) {
        *out_len = sim->files[index].len;
    }
    return sim->files[index].data;
}

bool emrtd_sim_authenticated(const EmrtdSim* sim) {
    return sim != NULL && sim->sm_active;
}

const char* emrtd_sim_protocol(const EmrtdSim* sim) {
    return sim != NULL ? sim->protocol : "";
}

size_t emrtd_sim_exchanges(const EmrtdSim* sim) {
    return sim != NULL ? sim->exchanges : 0;
}
