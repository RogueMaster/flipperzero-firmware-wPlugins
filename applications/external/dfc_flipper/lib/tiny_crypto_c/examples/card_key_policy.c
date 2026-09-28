/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "card_key_policy.h"
#include <string.h>

enum { CARD_PSS_SALT_BYTES = 32 };

static int exponent_supported(TC_bytes exponent)
{
  static const uint8_t minimum[] = {1,0,1};
  if (!exponent.data || exponent.length < sizeof minimum || exponent.length > 32 ||
      !exponent.data[0] || !(exponent.data[exponent.length - 1] & 1)) return 0;
  return exponent.length != sizeof minimum ||
      memcmp(exponent.data,minimum,sizeof minimum) >= 0;
}

ExampleCardKeyPolicyResult example_card_key_parameters_select(
    const TC_X509_public_key* key, const ExampleCardKeyPolicy* policy,
    ExampleCardKeyParameters* out)
{
  if (!key || !policy || !out ||
      (policy->profile != TC_PIV_CARD && policy->profile != TC_TWIC_LEGACY_CARD &&
       policy->profile != TC_TWIC_NEXGEN_CARD) ||
      (policy->allow_legacy_rsa1024 != 0 && policy->allow_legacy_rsa1024 != 1) ||
      (policy->rsa_padding != EXAMPLE_CARD_RSA_V15 && policy->rsa_padding != EXAMPLE_CARD_RSA_PSS))
    return EXAMPLE_CARD_KEY_POLICY_ERROR;
  if (!(policy->certificate_key_usage & TC_KEY_USAGE_DIGITAL_SIGNATURE))
    return EXAMPLE_CARD_KEY_POLICY_INVALID;

  ExampleCardKeyParameters selected;
  memset(&selected,0,sizeof selected);
  selected.challenge.signature.hash = TC_HASH_SHA256;
  if (key->type == TC_KEY_RSA) {
    selected.challenge.signature.scheme = TC_SIGNATURE_RSA_V15;
    if (policy->rsa_padding == EXAMPLE_CARD_RSA_PSS) {
      selected.challenge.signature.scheme = TC_SIGNATURE_RSA_PSS;
      selected.challenge.signature.mgf_hash = TC_HASH_SHA256;
      selected.challenge.signature.salt_length = CARD_PSS_SALT_BYTES;
    }
    if (!exponent_supported(key->exponent) || key->modulus.length != key->bits / 8u)
      return EXAMPLE_CARD_KEY_POLICY_INVALID;
    if (key->bits == 1024) {
      if (policy->profile != TC_TWIC_LEGACY_CARD || !policy->allow_legacy_rsa1024)
        return EXAMPLE_CARD_KEY_POLICY_UNSUPPORTED;
    } else if (key->bits != 2048 && key->bits != 3072) {
      return EXAMPLE_CARD_KEY_POLICY_UNSUPPORTED;
    }
  } else if (key->type == TC_KEY_EC) {
    selected.challenge.signature.scheme = TC_SIGNATURE_ECDSA;
    if (key->curve == TC_EC_P256 && key->bits == 256 && key->key.length == 65) {
      /* SHA-256 is already selected. */
    } else if (key->curve == TC_EC_P384 && key->bits == 384 && key->key.length == 97) {
      selected.challenge.signature.hash = TC_HASH_SHA384;
    } else return EXAMPLE_CARD_KEY_POLICY_UNSUPPORTED;
  } else return EXAMPLE_CARD_KEY_POLICY_UNSUPPORTED;

  /* TWIC Part 2 v5 fixes the NEXGEN card-authentication key to RSA-2048. */
  if (policy->profile == TC_TWIC_NEXGEN_CARD &&
      (key->type != TC_KEY_RSA || key->bits != 2048))
    return EXAMPLE_CARD_KEY_POLICY_UNSUPPORTED;
  *out = selected;
  return EXAMPLE_CARD_KEY_POLICY_OK;
}
