/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CARD_KEY_POLICY_H_
#define EXAMPLE_CARD_KEY_POLICY_H_
#include <tiny_crypto/key_challenge.h>
#include <tiny_crypto/piv_card.h>

typedef enum {
  EXAMPLE_CARD_KEY_POLICY_OK, EXAMPLE_CARD_KEY_POLICY_INVALID,
  EXAMPLE_CARD_KEY_POLICY_UNSUPPORTED, EXAMPLE_CARD_KEY_POLICY_ERROR
} ExampleCardKeyPolicyResult;

typedef enum { EXAMPLE_CARD_RSA_V15, EXAMPLE_CARD_RSA_PSS } ExampleCardRSAPadding;

typedef struct {
  TC_PIV_card_profile profile;
  uint16_t certificate_key_usage;
  int allow_legacy_rsa1024;
  ExampleCardRSAPadding rsa_padding;
} ExampleCardKeyPolicy;

typedef struct {
  TC_key_challenge_options challenge;
} ExampleCardKeyParameters;

/* Apply PIV/TWIC certificate-use and subject-key policy without card command
 * identifiers or transport behavior. */
ExampleCardKeyPolicyResult example_card_key_parameters_select(
    const TC_X509_public_key* key, const ExampleCardKeyPolicy* policy,
    ExampleCardKeyParameters* out);

#endif
