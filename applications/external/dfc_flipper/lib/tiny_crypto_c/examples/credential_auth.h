/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CREDENTIAL_AUTH_H_
#define EXAMPLE_CREDENTIAL_AUTH_H_
#include "card_key_policy.h"
#include "credential_io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EXAMPLE_CARD_KEY_VERIFIED, EXAMPLE_CARD_KEY_INVALID, EXAMPLE_CARD_KEY_UNSUPPORTED,
  EXAMPLE_CARD_KEY_LIMIT, EXAMPLE_CARD_KEY_TRANSPORT, EXAMPLE_CARD_KEY_ERROR
} ExampleCardKeyResult;

enum { EXAMPLE_CARD_KEY_RESPONSE_BYTES = 514 };

typedef struct {
  TC_key_challenge_workspace key;
  uint8_t response[EXAMPLE_CARD_KEY_RESPONSE_BYTES];
} ExampleCardKeyWorkspace;

/* Prove possession of a 9A or 9E key with a fresh random challenge.
 * Keep the transaction held and key bytes stable during the call.
 * The function wipes workspace before returning. */
ExampleCardKeyResult example_card_check_key(ExampleCardIO* io, ExampleCardKeyReference reference,
    const TC_X509_public_key* key, const ExampleCardKeyPolicy* policy,
    const TC_X509_signature_provider* provider, TC_random_fn random, void* random_context,
    ExampleCardKeyWorkspace* workspace, size_t* work);
#ifdef __cplusplus
}
#endif
#endif
