/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_RSA_VALIDATE_H_
#define EXAMPLE_RSA_VALIDATE_H_
#include <tiny_crypto/rsa.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Keep key bytes and RNG state separate from caller-owned scratch.
 * Accept the key only on TC_RSA_OK, then apply application key-strength policy. */
TC_RSA_result example_validate_rsa_key(const TC_RSA_private_key* key,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch,
    size_t scratch_words);

#ifdef __cplusplus
}
#endif
#endif
