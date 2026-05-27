/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef FURI_PACKED
#define FURI_PACKED __attribute__((packed))
#endif

#define U2F_CMD_REGISTER 0x01
#define U2F_CMD_AUTHENTICATE 0x02
#define U2F_CMD_VERSION 0x03

typedef enum {
    U2fCheckOnly = 0x07,
    U2fEnforce = 0x03,
    U2fDontEnforce = 0x08,
} U2fAuthMode;

#define U2F_HASH_SIZE 32
#define U2F_NONCE_SIZE 32
#define U2F_CHALLENGE_SIZE 32
#define U2F_APP_ID_SIZE 32

#define U2F_EC_KEY_SIZE 32
#define U2F_EC_BIGNUM_SIZE 32
#define U2F_EC_POINT_SIZE 65
#define U2F_DER_SIGNATURE_MAX_LEN 72

/*
 * Packed U2F wire structs mirror the legacy APDU payload layout. Multi-byte
 * fields are encoded exactly as the U2F spec expects, so response code copies
 * them directly into transport buffers.
 */
typedef struct {
    uint8_t format;
    uint8_t xy[64];
} FURI_PACKED U2fPubKey;
_Static_assert(sizeof(U2fPubKey) == U2F_EC_POINT_SIZE, "U2fPubKey size mismatch");

typedef struct {
    uint8_t len;
    uint8_t hash[U2F_HASH_SIZE];
    uint8_t nonce[U2F_NONCE_SIZE];
} FURI_PACKED U2fKeyHandle;

typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t len[3];
    uint8_t challenge[U2F_CHALLENGE_SIZE];
    uint8_t app_id[U2F_APP_ID_SIZE];
} FURI_PACKED U2fRegisterReq;

typedef struct {
    uint8_t reserved;
    U2fPubKey pub_key;
    U2fKeyHandle key_handle;
    uint8_t cert[];
} FURI_PACKED U2fRegisterResp;

typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t len[3];
    uint8_t challenge[U2F_CHALLENGE_SIZE];
    uint8_t app_id[U2F_APP_ID_SIZE];
    U2fKeyHandle key_handle;
} FURI_PACKED U2fAuthReq;

typedef struct {
    uint8_t user_present;
    uint32_t counter;
    uint8_t signature[];
} FURI_PACKED U2fAuthResp;
