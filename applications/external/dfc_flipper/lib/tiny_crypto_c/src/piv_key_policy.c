/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Key policy of the key proofs: certificate key use, the SP 800-78-5
 * algorithm identifiers of Table 9 with the per-key rules of Table 10, and
 * the TWIC reader profiles. */
#include <tiny_crypto/piv_key_proof.h>
#if TC_ENABLE_PIV_KEY_PROOF
#include "internal.h"
#include "piv_key_proof_internal.h"

enum {
  KEY_USAGE_EXTENSION = 0x0f, /* id-ce-keyUsage, 2.5.29.15 */
  PSS_SALT_BYTES = 32,
  MAX_EXPONENT_BYTES = 32,
  P256_POINT_BYTES = 65,
  P384_POINT_BYTES = 97
};

/* SP 800-78-5 Table 10: RSA-2048 (07) ends after 2030. */
static const TC_X509_time rsa2048_end = {2030, 12, 31, 23, 59, 59};

int tc_piv_key_policy_valid(const TC_PIV_key_policy* policy)
{
  return (policy->profile == TC_PIV_CARD || policy->profile == TC_TWIC_LEGACY_CARD ||
          policy->profile == TC_TWIC_NEXGEN_CARD) &&
         (policy->rsa_padding == TC_PIV_RSA_PKCS1_V15 || policy->rsa_padding == TC_PIV_RSA_PSS) &&
         policy->allow_rsa1024 <= 1 && TC_X509_time_check(&policy->at) == TC_TLV_OK;
}

/* The keyUsage bits of certificate, or 0 when the extension is absent. The
 * certificate reader checked the extension syntax, so the scan needs no
 * further limits than the extension bytes. */
static TC_TLV_result key_usage(const TC_X509_certificate* certificate, uint16_t* usage)
{
  const size_t length = certificate->extensions.length;
  const TC_TLV_limits limits = {length, length, length, 4};
  static const uint8_t oid[] = {0x55, 0x1d, KEY_USAGE_EXTENSION};
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result result = TC_X509_extensions_init(&reader, certificate->extensions, &limits);
  *usage = 0;
  while (result == TC_TLV_OK && (result = TC_X509_extension_next(&reader, &extension)) == TC_TLV_OK)
    if (extension.oid.length == sizeof oid && !memcmp(extension.oid.data, oid, sizeof oid))
      result = TC_X509_key_usage_read(extension.value, &limits, usage);
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

/* SP 800-78-5 section 3.1 and Table 9: 65537 <= e <= 2^256 - 1, so an odd
 * value of 3 to 32 bytes without leading zeros, at least 01 00 01. */
static int exponent_valid(TC_bytes exponent)
{
  static const uint8_t minimum[] = {0x01, 0x00, 0x01};
  if (!exponent.data || exponent.length < sizeof minimum || exponent.length > MAX_EXPONENT_BYTES ||
      !exponent.data[0] || !(exponent.data[exponent.length - 1] & 1))
    return 0;
  return exponent.length > sizeof minimum || memcmp(exponent.data, minimum, sizeof minimum) >= 0;
}

/* RSA identifiers of Table 9 and the Table 10 rules of the profile. */
static TC_PIV_result rsa_algorithm(const TC_X509_public_key* key, const TC_PIV_key_policy* policy,
                                   uint8_t* algorithm)
{
  if (!exponent_valid(key->exponent) || !key->modulus.data || key->modulus.length != key->bits / 8u)
    return TC_PIV_INVALID;
  if (key->bits == 3072) {
    *algorithm = TC_PIV_ALGORITHM_RSA_3072;
    return TC_PIV_OK;
  }
  if (key->bits == 1024) {
    /* Table 10 keeps 06 for retired keys only. TWIC Legacy readers may
     * accept it by explicit policy (TWIC Part 2 v5 Appendix H). */
    if (policy->profile != TC_TWIC_LEGACY_CARD || !policy->allow_rsa1024)
      return TC_PIV_UNSUPPORTED;
    *algorithm = TC_PIV_ALGORITHM_RSA_1024;
    return TC_PIV_OK;
  }
  if (key->bits != 2048)
    return TC_PIV_UNSUPPORTED;
  if (policy->profile == TC_PIV_CARD) {
    int order = 0;
    if (TC_X509_time_compare(&policy->at, &rsa2048_end, &order) != TC_TLV_OK || order > 0)
      return TC_PIV_UNSUPPORTED;
  }
  *algorithm = TC_PIV_ALGORITHM_RSA_2048;
  return TC_PIV_OK;
}

/* ECDSA identifiers of Table 9 with their Table 2 hashes. */
static TC_PIV_result ec_algorithm(const TC_X509_public_key* key, uint8_t* algorithm,
                                  TC_hash_algorithm* hash)
{
  if (key->curve == TC_EC_P256 && key->bits == 256 && key->key.length == P256_POINT_BYTES) {
    *algorithm = TC_PIV_ALGORITHM_ECC_P256;
    *hash = TC_HASH_SHA256;
    return TC_PIV_OK;
  }
  if (key->curve == TC_EC_P384 && key->bits == 384 && key->key.length == P384_POINT_BYTES) {
    *algorithm = TC_PIV_ALGORITHM_ECC_P384;
    *hash = TC_HASH_SHA384;
    return TC_PIV_OK;
  }
  return TC_PIV_UNSUPPORTED;
}

TC_PIV_result TC_PIV_key_parameters_select(const TC_X509_certificate* certificate,
                                           const TC_PIV_key_policy* policy,
                                           TC_PIV_key_parameters* out)
{
  if (!certificate || !policy || !out || !tc_piv_key_policy_valid(policy))
    return TC_PIV_ARGUMENT;
  /* The card keys sign challenges, so their certificates assert
   * digitalSignature (RFC 5280 section 4.2.1.3). */
  uint16_t usage = 0;
  if (key_usage(certificate, &usage) != TC_TLV_OK || !(usage & TC_KEY_USAGE_DIGITAL_SIGNATURE))
    return TC_PIV_INVALID;
  const TC_X509_public_key* key = &certificate->public_key;
  TC_PIV_key_parameters selected;
  memset(&selected, 0, sizeof selected);
  selected.challenge.signature.hash = TC_HASH_SHA256;
  TC_PIV_result result = TC_PIV_UNSUPPORTED;
  if (key->type == TC_KEY_RSA) {
    result = rsa_algorithm(key, policy, &selected.algorithm);
    selected.challenge.signature.scheme = TC_SIGNATURE_RSA_V15;
    if (policy->rsa_padding == TC_PIV_RSA_PSS) {
      selected.challenge.signature.scheme = TC_SIGNATURE_RSA_PSS;
      selected.challenge.signature.mgf_hash = TC_HASH_SHA256;
      selected.challenge.signature.salt_length = PSS_SALT_BYTES;
    }
  } else if (key->type == TC_KEY_EC) {
    selected.challenge.signature.scheme = TC_SIGNATURE_ECDSA;
    result = ec_algorithm(key, &selected.algorithm, &selected.challenge.signature.hash);
  }
  if (result != TC_PIV_OK)
    return result;
  /* TWIC Part 2 v5 4.5 and 5.3: the NEXGEN card authentication key is 9E07. */
  if (policy->profile == TC_TWIC_NEXGEN_CARD && selected.algorithm != TC_PIV_ALGORITHM_RSA_2048)
    return TC_PIV_UNSUPPORTED;
  *out = selected;
  return TC_PIV_OK;
}
#endif
