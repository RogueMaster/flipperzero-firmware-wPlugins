// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT
//
// TOTP codes (RFC 6238) with HMAC-SHA1, -SHA256 or -SHA512, and Steam Guard.
// Portable C, no dependencies - also built natively for the tests.
//
// The companion stores the secret as "ALGO:DIGITS:PERIOD:BASE32SECRET",
// e.g. "SHA1:6:30:JBSWY3DPEHPK3PXP" (ALGO = SHA1, SHA256, SHA512 or STEAM).
// A bare base32 secret means SHA1, 6 digits, 30 seconds.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VF_TOTP_MAX_KEY  64u
#define VF_TOTP_CODE_MAX 11u /* 10 digits + NUL */

typedef enum {
    VfTotpSha1,
    VfTotpSha256,
    VfTotpSha512,
    VfTotpSteam, /* HMAC-SHA1, 5 characters from the Steam alphabet */
} VfTotpAlgo;

typedef struct {
    uint8_t key[VF_TOTP_MAX_KEY];
    uint8_t key_len;
    uint8_t digits;
    uint16_t period;
    VfTotpAlgo algo;
} VfTotp;

/* Parse a stored secret. Returns false if it is empty or malformed. */
bool vf_totp_parse(const char* field, VfTotp* out);

/* Code for a Unix time (UTC). out gets digits characters and a NUL. */
void vf_totp_code(const VfTotp* t, uint64_t unix_time, char out[VF_TOTP_CODE_MAX]);

/* Seconds until the code changes (1..period). */
uint32_t vf_totp_remaining(const VfTotp* t, uint64_t unix_time);

/* Wipe the key. */
void vf_totp_wipe(VfTotp* t);
