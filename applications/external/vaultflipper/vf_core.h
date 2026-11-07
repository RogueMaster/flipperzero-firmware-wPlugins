// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT
//
// VaultFlipper – portable core: file format, crypto, parsers.
// Depends only on Monocypher and the C standard library, so it also builds
// natively for the test suite. Full description: docs/FORMAT.md.
//
// The vault is split into a small directory and many pages, each encrypted on
// its own. Unlocking decrypts only the directory; a page (one folder or a part
// of it, at most 48 logins) is decrypted when it is opened. RAM use therefore
// does not grow with the size of the vault.
//
// File layout (integers little endian):
//   header (72 bytes)
//     0  4  magic "VFLP"
//     4  1  version (2)
//     5  3  reserved (0)
//     8  4  Argon2id memory in KiB
//    12  4  Argon2id passes
//    16 16  salt
//    32 32  ephemeral X25519 public key of the companion
//    64  4  directory ciphertext length (<= 24 KiB)
//    68  4  reserved (0)
//   block = nonce (24) | Poly1305 tag (16) | ciphertext
//     directory = block 0 at offset 72, pages = blocks 1..n at offsets from the directory
//     AAD = header (72 bytes) | u32 block number
//
// Keys:
//   dev_sk  = BLAKE2b-256(key = device secret, "VaultFlipper x25519 v1")
//   shared  = X25519(dev_sk, eph_pk)
//   pin_key = Argon2id(PIN, salt, blocks, passes, lanes = 1)
//   key     = BLAKE2b-256(key = shared, pin_key)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VF_VERSION_BYTE   2
#define VF_HEADER_SIZE    72
#define VF_BLOCK_OVERHEAD 40
#define VF_KEY_SIZE       32
#define VF_MAX_DIR        (24u * 1024u)
#define VF_MAX_PAGE       (8u * 1024u)
#define VF_MAX_PAGES      1024u
#define VF_MAX_ACCOUNTS   16u
#define VF_MIN_BLOCKS     8u
#define VF_MAX_BLOCKS     64u
#define VF_MIN_PASSES     1u
#define VF_MAX_PASSES     1000u
#define VF_MAX_PIN        32u
/* Pages with this account byte hold the 2FA list: name, user and TOTP secret
 * of every login that has one, across all accounts, sorted by name. */
#define VF_CODES_ACCOUNT  0xFFu

typedef enum {
    VfOk = 0,
    VfErrFormat = -1, /* corrupt file or wrong version */
    VfErrAuth = -2, /* wrong PIN or wrong device */
    VfErrMemory = -3,
    VfErrParams = -4, /* Argon2 parameters out of bounds */
    VfErrIo = -5, /* reading the file failed */
} VfResult;

/* field tags */
enum {
    VfTagAccount = 1,
    VfTagGroup = 2,
    VfTagName = 3,
    VfTagUser = 4,
    VfTagPass = 5,
    VfTagTotp = 6,
    VfTagUri = 7,
    VfTagMetaLayout = 0x10,
    VfTagMetaSynced = 0x11,
    VfTagFirst = 0x20, /* first entry name on a page */
    VfTagLast = 0x21, /* last entry name on a page */
};

typedef enum {
    VfFieldAccount,
    VfFieldGroup,
    VfFieldName,
    VfFieldUser,
    VfFieldPass,
    VfFieldTotp,
    VfFieldUri,
    VfFieldLayout,
    VfFieldSynced,
    VfFieldFirst,
    VfFieldLast,
    VfFieldCount,
} VfField;

typedef struct {
    const char* f[VfFieldCount]; /* point into a plaintext buffer, never NULL */
} VfRecord;

typedef struct {
    uint32_t offset; /* of the block in the file */
    uint32_t length; /* ciphertext length */
    uint16_t count; /* logins on the page */
    uint8_t account; /* index into accounts, or VF_CODES_ACCOUNT */
    const char* group;
    const char* first; /* first entry name on the page */
    const char* last; /* last entry name on the page */
} VfPageInfo;

typedef struct {
    uint8_t* buf;
    size_t len;
    VfRecord meta; /* layout, sync time */
    const char* accounts[VF_MAX_ACCOUNTS];
    uint8_t account_count;
    VfPageInfo* pages;
    uint16_t page_count;
    uint32_t entry_count; /* logins, without the 2FA list */
    uint32_t code_count; /* entries in the 2FA list */
} VfDir;

typedef struct {
    uint8_t* buf;
    size_t len;
    VfRecord* entries; /* name, user, pass, totp, uri */
    uint16_t count;
    uint16_t index; /* page number in the directory */
} VfPage;

/* Reads len bytes at offset; returns false on error. */
typedef bool (*VfReadFn)(void* ctx, uint32_t offset, void* buf, size_t len);

typedef struct {
    VfReadFn read;
    void* ctx;
    uint8_t header[VF_HEADER_SIZE];
    uint32_t blocks;
    uint32_t passes;
    uint32_t dir_len;
    uint8_t key[VF_KEY_SIZE];
    bool unlocked;
} VfFile;

/* Derive the device key pair from the 32-byte device secret. */
void vf_device_keypair(const uint8_t device_secret[32], uint8_t sk[32], uint8_t pk[32]);

/* Read and check the header. */
VfResult vf_file_open(VfFile* f, VfReadFn read, void* ctx);

/* Derive the key and decrypt the directory. Argon2id needs blocks KiB of work
 * memory: pass a buffer of work_size bytes, or NULL to malloc it. */
VfResult vf_file_unlock(
    VfFile* f,
    const uint8_t dev_sk[32],
    const char* pin,
    size_t pin_len,
    void* work,
    size_t work_size,
    VfDir* dir);

/* Bytes needed to load page `index` (buffer + parsed records). */
size_t vf_page_mem(const VfDir* dir, uint16_t index);

/* Decrypt one page. */
VfResult vf_page_load(VfFile* f, const VfDir* dir, uint16_t index, VfPage* out);

void vf_page_free(VfPage* p);
void vf_dir_free(VfDir* d);
/* Wipe the key. */
void vf_file_close(VfFile* f);

/* 8 hex chars + dash: "AB12-CD34" (10 bytes incl. NUL) */
void vf_fingerprint(const uint8_t pk[32], char out[10]);

/* ASCII case-insensitive substring test. */
bool vf_contains(const char* haystack, const char* needle);

void vf_wipe(void* p, size_t n);
