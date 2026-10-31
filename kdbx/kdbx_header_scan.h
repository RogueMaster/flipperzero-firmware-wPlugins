#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pre-password check of a KDBX file's plaintext outer header.
 *
 * It rejects files FlipPass cannot open before the user types a password: no
 * KeePass signature, KeePass 1.x or KDBX 3.x, a malformed field list, a payload
 * cipher other than AES256/ChaCha20, compression other than none/GZip, an IV of the
 * wrong size, unusable KdfParameters (missing, unknown dictionary version, Argon2,
 * unknown KDF, bad salt) and a header whose unkeyed SHA-256 does not match.
 *
 * It cannot detect what only the password reveals: a required key file, the inner
 * stream cipher (it lives in the encrypted inner header), a damaged payload or a
 * wrong password. The unlock pipeline keeps its own checks as defense in depth. */

/* Header fields scanned before the walk is declared malformed. */
#define KDBX_HEADER_SCAN_MAX_FIELDS    64U
/* Outer header size beyond which the walk is declared malformed. */
#define KDBX_HEADER_SCAN_MAX_BYTES     (64U * 1024U)
/* KdfParameters are copied into a stack buffer of this size; larger blocks are rejected. */
#define KDBX_HEADER_SCAN_KDF_MAX_BYTES 256U

typedef struct {
    void* context;
    /** Read exactly `size` bytes; false on a short read or an I/O error. */
    bool (*read)(void* context, uint8_t* out, size_t size);
    /** Move forward `size` bytes; false when fewer bytes remain. */
    bool (*skip)(void* context, uint32_t size);
    /** Return to the first byte of the file. */
    bool (*rewind)(void* context);
} KDBXHeaderScanSource;

/** Returns true when the header passes every check; otherwise writes the first
 *  failure into `error`. Uses about 0.5 KiB of stack and no heap. */
bool kdbx_header_scan(const KDBXHeaderScanSource* source, char* error, size_t error_size);

/* Publishes compression only after the entire header, including its hash,
 * passes validation. On failure the optional output is reset to zero. */
bool kdbx_header_scan_with_compression(
    const KDBXHeaderScanSource* source,
    uint32_t* compression,
    char* error,
    size_t error_size);

#ifdef __cplusplus
}
#endif
