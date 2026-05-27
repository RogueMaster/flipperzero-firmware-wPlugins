/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../zerofido_types.h"

size_t zf_ecdsa_der_encode_signature(const uint8_t r[ZF_PUBLIC_KEY_LEN],
                                     const uint8_t s[ZF_PUBLIC_KEY_LEN], uint8_t *out,
                                     size_t capacity);
bool zf_ecdsa_der_decode_signature(const uint8_t *signature, size_t signature_len,
                                   uint8_t raw_signature[ZF_PUBLIC_KEY_LEN * 2U]);
