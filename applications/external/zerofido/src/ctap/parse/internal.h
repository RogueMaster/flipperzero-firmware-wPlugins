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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../zerofido_cbor.h"
#include "../../zerofido_types.h"

typedef enum {
    ZfCtapTextKeyUnknown = 0,
    ZfCtapTextKeyAlg,
    ZfCtapTextKeyCredProtect,
    ZfCtapTextKeyDisplayName,
    ZfCtapTextKeyHmacSecret,
    ZfCtapTextKeyIcon,
    ZfCtapTextKeyId,
    ZfCtapTextKeyName,
    ZfCtapTextKeyPublicKey,
    ZfCtapTextKeyRk,
    ZfCtapTextKeyType,
    ZfCtapTextKeyUp,
    ZfCtapTextKeyUv,
} ZfCtapTextKey;

/*
 * Shared parser helpers keep CTAP map handling strict: duplicate known keys are
 * rejected, strings/bytes are bounded, and unsupported substructures are skipped
 * only where the CTAP command allows extension.
 */
ZfCtapTextKey zf_ctap_classify_text_key(const uint8_t *ptr, size_t size);

bool zf_ctap_mark_seen_key(uint16_t *seen_keys, uint64_t key);

bool zf_ctap_cbor_read_text_copy(ZfCborCursor *cursor, char *out, size_t out_size);

bool zf_ctap_cbor_read_text_discard(ZfCborCursor *cursor);

bool zf_ctap_cbor_read_bytes_copy(ZfCborCursor *cursor, uint8_t *out, size_t out_capacity,
                                  size_t *out_size);

bool zf_ctap_parse_cose_p256_key_agreement(ZfCborCursor *cursor, uint8_t x[ZF_PUBLIC_KEY_LEN],
                                           uint8_t y[ZF_PUBLIC_KEY_LEN]);

uint8_t zf_ctap_parse_options_map(ZfCborCursor *cursor, bool *up, bool *has_up, bool *uv,
                                  bool *has_uv, bool *rk, bool *has_rk);

uint8_t zf_ctap_parse_make_credential_extensions_map(ZfCborCursor *cursor, bool *has_cred_protect,
                                                     uint8_t *cred_protect,
                                                     bool *hmac_secret_requested);

uint8_t zf_ctap_parse_pubkey_cred_params(ZfCborCursor *cursor, bool *es256_supported);

uint8_t zf_ctap_parse_descriptor_array(ZfCborCursor *cursor, ZfCredentialDescriptorList *list);
