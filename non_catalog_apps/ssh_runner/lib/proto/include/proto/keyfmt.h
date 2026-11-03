/*
 * SSH key files as the user drops them into keys/: recognise the format, the
 * type and size, whether it is encrypted, and work out public key lines and SHA256
 * fingerprints. Shared by the Flipper app (Keys screen, Hosts editor, connect) and the
 * ESP32 (checks before libssh parses a staged key). Pure C, no allocation.
 *
 * Everything works on a prefix of the file: the first KEYFMT_SNIFF_BYTES are enough
 * for the header and an OpenSSH container's public section, so the Flipper never
 * needs a whole key file in RAM.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEYFMT_SNIFF_BYTES 2048u /* file prefix to read for keyfmt_sniff() */
#define KEYFMT_SCRATCH     1280u /* scratch buffer for decoded data (RSA 8192 public blob fits) */
#define KEYFMT_FP_SIZE     52u /* "SHA256:" + 43 base64 chars + NUL */
#define KEYFMT_LABEL_SIZE  16u

typedef enum {
    KeyFmtUnknown = 0,
    KeyFmtOpenSsh, /* -----BEGIN OPENSSH PRIVATE KEY----- */
    KeyFmtPemRsa, /* -----BEGIN RSA PRIVATE KEY----- (PKCS#1) */
    KeyFmtPemEc, /* -----BEGIN EC PRIVATE KEY----- (SEC1) */
    KeyFmtPemDsa, /* -----BEGIN DSA PRIVATE KEY----- */
    KeyFmtPkcs8, /* -----BEGIN PRIVATE KEY----- */
    KeyFmtPkcs8Enc, /* -----BEGIN ENCRYPTED PRIVATE KEY----- */
    KeyFmtPublic, /* a public key: one-line, PEM or RFC 4716 */
    KeyFmtPutty, /* PuTTY .ppk */
} KeyFmt;

typedef enum {
    KeyTypeUnknown = 0, /* not known before decryption (encrypted PEM) */
    KeyTypeEd25519,
    KeyTypeEcdsa,
    KeyTypeRsa,
    KeyTypeDsa,
    KeyTypeOther, /* security-key, certificate or anything else we do not use */
} KeyType;

typedef struct {
    KeyFmt fmt;
    KeyType type;
    uint16_t bits; /* 0 when unknown */
    bool encrypted;
    /* OpenSSH container: the public key blob from its plaintext public section,
     * pointing into the caller's scratch buffer; NULL otherwise. */
    const uint8_t* pub;
    size_t pub_len;
} KeyInfo;

/* Recognise a key file from its first bytes (`len` may be less than the file).
 * `scratch` (KEYFMT_SCRATCH bytes) holds decoded data that `ki->pub` points into. */
void keyfmt_sniff(const char* text, size_t len, KeyInfo* ki, uint8_t* scratch, size_t scratch_cap);

/* Why this file cannot be used, as a message for the user, or NULL if it may work. */
const char* keyfmt_problem(const KeyInfo* ki);

/* Type and size from an SSH public key blob (RFC 4253 §6.6 wire format). */
bool keyfmt_blob_info(const uint8_t* blob, size_t len, KeyInfo* ki);

/* Parse a one-line public key "<type> <base64> [comment]" into its blob. */
bool keyfmt_parse_pub_line(
    const char* line,
    size_t len,
    uint8_t* blob,
    size_t cap,
    size_t* blob_len);

/* "<type> <base64>[ <comment>]" for a blob; returns the length, 0 if it does not fit. */
size_t
    keyfmt_pub_line(const uint8_t* blob, size_t len, const char* comment, char* out, size_t cap);

/* OpenSSH fingerprint "SHA256:<unpadded base64>" of a public key blob. */
void keyfmt_fingerprint(const uint8_t* blob, size_t len, char out[KEYFMT_FP_SIZE]);

/* Label for screens: "Ed25519", "ECDSA P-256", "RSA 3072", "RSA", "Key". */
void keyfmt_label(const KeyInfo* ki, char out[KEYFMT_LABEL_SIZE]);

/* ---- building blocks, exposed for streaming use and tests ---- */

typedef struct {
    uint32_t h[8];
    uint64_t bytes;
    uint8_t block[64];
    uint8_t fill;
} KeyfmtSha256;

void keyfmt_sha256_init(KeyfmtSha256* s);
void keyfmt_sha256_update(KeyfmtSha256* s, const void* data, size_t len);
void keyfmt_sha256_final(KeyfmtSha256* s, uint8_t out[32]);

/* Base64 without line breaks; `pad` adds '=' padding. Returns the length written
 * (excluding the NUL), 0 if it does not fit. */
size_t keyfmt_b64_encode(const uint8_t* in, size_t len, bool pad, char* out, size_t cap);
/* Decodes until `cap` bytes are out, padding, a '-' or the end of the input;
 * whitespace is skipped. Returns the number of bytes written. */
size_t keyfmt_b64_decode(const char* in, size_t len, uint8_t* out, size_t cap);

/* "file:SHA256:..." id of a key whose public key is not known (encrypted PEM). */
void keyfmt_file_id(const uint8_t digest[32], char out[KEYFMT_FP_SIZE + 5]);

#ifdef __cplusplus
}
#endif
