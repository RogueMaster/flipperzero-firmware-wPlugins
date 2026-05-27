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

#include "../zerofido_crypto.h"
#include "../zerofido_pin.h"

#define ZF_PIN_PROTOCOL_KEYS_LEN 64U

/*
 * CTAP PIN protocol v1 and v2 share the same high-level operations but differ
 * in key derivation, AES IV handling, and PIN/UV auth length. These helpers
 * isolate those version differences from the ClientPIN command handlers.
 */
uint8_t *zf_pin_protocol_hmac_key(uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN]);
uint8_t *zf_pin_protocol_aes_key(uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN]);
bool zf_pin_protocol_supported(uint64_t pin_protocol);
size_t zf_pin_protocol_auth_len(uint64_t pin_protocol);
bool zf_pin_protocol_derive_keys(const ZfClientPinState *state, uint64_t pin_protocol,
                                 const uint8_t platform_x[ZF_PUBLIC_KEY_LEN],
                                 const uint8_t platform_y[ZF_PUBLIC_KEY_LEN],
                                 uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN]);
bool zf_pin_protocol_decrypt(uint64_t pin_protocol, uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN],
                             const uint8_t *ciphertext, size_t ciphertext_len, uint8_t *plaintext,
                             size_t *plaintext_len);
bool zf_pin_protocol_encrypt(uint64_t pin_protocol, uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN],
                             const uint8_t *plaintext, size_t plaintext_len, uint8_t *out,
                             size_t out_capacity, size_t *out_len);
bool zf_pin_protocol_hmac_matches(ZfHmacSha256Scratch *scratch, uint64_t pin_protocol,
                                  const uint8_t key[32], const uint8_t *first, size_t first_len,
                                  const uint8_t *second, size_t second_len, const uint8_t *expected,
                                  size_t expected_len);
