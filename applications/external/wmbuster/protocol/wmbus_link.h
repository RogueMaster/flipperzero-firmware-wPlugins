#pragma once
/*
 * Link-layer parser for wM-Bus Frame Format A and B.
 * Input is a CRC-stripped (Format A) or full (Format B) telegram starting
 * with the L-byte:
 *
 *   L | C | M(2) | A(4=ID,1=Ver,1=Type) | CI | <APDU...>
 *
 * For Format B the CRCs are at the end and have already been verified by
 * the caller (wmbus_crc_verify_format_b). For Format A the inter-block CRCs
 * have been removed.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t  L;
    uint8_t  C;          /* control field */
    uint16_t M;          /* manufacturer */
    uint32_t id;         /* 4-byte BCD-or-binary device id, little-endian */
    uint8_t  version;
    uint8_t  medium;     /* device type */
    uint8_t  ci;         /* CI field (control information) */
    /* Application data section (after CI) */
    const uint8_t* apdu;
    size_t apdu_len;

    /* Encryption flags decoded from CI / configuration word, when present. */
    uint8_t  enc_mode;   /* 0 = none, 5 = AES-128-CBC w/ static IV, 7 = AES-CTR */
    uint16_t access_no;  /* (for short headers) */
} WmbusLinkFrame;

/* Parse the link header. Returns true if header is well-formed (does not
 * imply payload is decryptable). */
bool wmbus_link_parse(const uint8_t* frame, size_t len, WmbusLinkFrame* out);

/* Helper: pretty-print human readable summary into `out` (truncating). */
void wmbus_link_to_str(const WmbusLinkFrame* f, char* out, size_t out_size);
