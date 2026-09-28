/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_KEY_CHALLENGE
#include <tiny_crypto/key_challenge.h>
#if TC_ENABLE_RSA
#include <tiny_crypto/rsa.h>
#endif
#include "internal.h"

enum { KEY_CHALLENGE_ACTIVE = 0x4b455943u };

static size_t challenge_digest_length(TC_hash_algorithm hash)
{
  switch (hash) {
    case TC_HASH_SHA1: return 20;
    case TC_HASH_SHA224: return 28;
    case TC_HASH_SHA256: return 32;
    case TC_HASH_SHA384: return 48;
    case TC_HASH_SHA512: return 64;
    default: return 0;
  }
}

static TC_key_challenge_result challenge_parameters(
    const TC_X509_public_key* key, const TC_key_challenge_options* options,
    size_t* digest_length, size_t* challenge_length)
{
  const TC_signature_algorithm* signature = &options->signature;
  *digest_length = challenge_digest_length(signature->hash);
  const int pss = signature->scheme == TC_SIGNATURE_RSA_PSS;
  if (!*digest_length || (!pss && (signature->mgf_hash != TC_HASH_UNKNOWN || signature->salt_length)))
    return TC_KEY_CHALLENGE_UNSUPPORTED;
  if (key->type == TC_KEY_RSA && (signature->scheme == TC_SIGNATURE_RSA_V15 || pss)) {
#if !TC_ENABLE_RSA
    return TC_KEY_CHALLENGE_UNSUPPORTED;
#else
    if (key->modulus.length != key->bits / 8u ||
        (key->bits != 1024 && key->bits != 2048 && key->bits != 3072))
      return TC_KEY_CHALLENGE_UNSUPPORTED;
    *challenge_length = key->modulus.length;
    if (pss && (!challenge_digest_length(signature->mgf_hash) ||
        signature->salt_length > TC_KEY_CHALLENGE_MAX_DIGEST_BYTES ||
        signature->salt_length > *challenge_length - *digest_length - 2))
      return TC_KEY_CHALLENGE_UNSUPPORTED;
    return TC_KEY_CHALLENGE_OK;
#endif
  }
  if (key->type == TC_KEY_EC && signature->scheme == TC_SIGNATURE_ECDSA) {
    *challenge_length = *digest_length;
    return TC_KEY_CHALLENGE_OK;
  }
  return TC_KEY_CHALLENGE_UNSUPPORTED;
}

static int prepare_storage_valid(const TC_X509_public_key* key,
    const TC_key_challenge_options* options, TC_random_source random,
    const TC_key_challenge_workspace* workspace, const TC_work_budget* work,
    const TC_bytes* out)
{
  const TC_bytes fields[] = {key->algorithm.oid,key->algorithm.parameters,key->key,
    key->modulus,key->exponent,key->curve_oid};
  const TC_bytes controls[] = {
    {(const uint8_t*)key,sizeof *key},
    {(const uint8_t*)options,sizeof *options},
    {(const uint8_t*)work,sizeof *work},
    {(const uint8_t*)out,sizeof *out},
    {(const uint8_t*)random.context,random.context ? 1u : 0u}
  };
  for (size_t i = 0; i < sizeof controls / sizeof *controls; ++i) {
    if (!tc_internal_ranges_disjoint(workspace,sizeof *workspace,
        controls[i].data,controls[i].length)) return 0;
    if (i != 3 && !tc_internal_ranges_disjoint(out,sizeof *out,
        controls[i].data,controls[i].length)) return 0;
  }
  for (size_t i = 0; i < sizeof fields / sizeof *fields; ++i)
    if (!tc_internal_ranges_disjoint(workspace,sizeof *workspace,
          fields[i].data,fields[i].length) ||
        !tc_internal_ranges_disjoint(out,sizeof *out,fields[i].data,fields[i].length) ||
        !tc_internal_ranges_disjoint(work,sizeof *work,fields[i].data,fields[i].length))
      return 0;
  return 1;
}

void TC_key_challenge_clear(TC_key_challenge_workspace* workspace)
{
  if (workspace) TC_secure_zero(workspace,sizeof *workspace);
}

TC_key_challenge_result TC_key_challenge_prepare(const TC_X509_public_key* key,
    const TC_key_challenge_options* options, TC_random_source random,
    TC_key_challenge_workspace* workspace, TC_work_budget* work, TC_bytes* out)
{
  size_t digest_length, challenge_length;
  TC_bytes challenge;
  if (!key || !options || !random.fill || !workspace || !work || !out ||
      !prepare_storage_valid(key,options,random,workspace,work,out))
    return TC_KEY_CHALLENGE_ERROR;
  TC_key_challenge_result result = challenge_parameters(
      key,options,&digest_length,&challenge_length);
  if (result != TC_KEY_CHALLENGE_OK) return result;
  const uint32_t encoding_work = key->type == TC_KEY_RSA ? (uint32_t)challenge_length : 0;
  const size_t salt_length = options->signature.scheme == TC_SIGNATURE_RSA_PSS ?
      options->signature.salt_length : 0;
  const uint32_t random_work = (uint32_t)(digest_length + salt_length);
  if (work->remaining < random_work ||
      work->remaining - random_work < encoding_work)
    return TC_KEY_CHALLENGE_LIMIT;

  TC_key_challenge_clear(workspace);
  work->remaining -= (uint32_t)digest_length;
  if (random.fill(random.context,workspace->digest,digest_length) != TC_OK) {
    TC_key_challenge_clear(workspace);
    return TC_KEY_CHALLENGE_ERROR;
  }
  if (key->type == TC_KEY_RSA) {
#if TC_ENABLE_RSA
    TC_RSA_result encoded;
    if (options->signature.scheme == TC_SIGNATURE_RSA_PSS) {
      uint8_t salt[TC_KEY_CHALLENGE_MAX_DIGEST_BYTES];
      work->remaining -= (uint32_t)salt_length;
      if (salt_length && random.fill(random.context,salt,salt_length) != TC_OK) {
        TC_secure_zero(salt,sizeof salt);
        TC_key_challenge_clear(workspace);
        return TC_KEY_CHALLENGE_ERROR;
      }
      const TC_RSA_pss_options rsa_options = {options->signature.hash,options->signature.mgf_hash,salt_length};
      encoded = TC_RSA_encode_pss_digest(&rsa_options,(TC_bytes){workspace->digest,digest_length},
          (TC_bytes){salt,salt_length},(TC_buffer){workspace->challenge,challenge_length},work);
      TC_secure_zero(salt,sizeof salt);
    } else {
      const TC_RSA_v15_options rsa_options = {options->signature.hash};
      encoded = TC_RSA_encode_v15_digest(&rsa_options,(TC_bytes){workspace->digest,digest_length},
          (TC_buffer){workspace->challenge,challenge_length},work);
    }
    if (encoded != TC_RSA_OK) {
      TC_key_challenge_clear(workspace);
      return encoded == TC_RSA_LIMIT ? TC_KEY_CHALLENGE_LIMIT :
          encoded == TC_RSA_UNSUPPORTED ? TC_KEY_CHALLENGE_UNSUPPORTED :
          TC_KEY_CHALLENGE_ERROR;
    }
    challenge = (TC_bytes){workspace->challenge,challenge_length};
#else
    TC_key_challenge_clear(workspace);
    return TC_KEY_CHALLENGE_UNSUPPORTED;
#endif
  } else challenge = (TC_bytes){workspace->digest,digest_length};
  workspace->signature = options->signature;
  workspace->digest_length = digest_length;
  workspace->state = KEY_CHALLENGE_ACTIVE;
  *out = challenge;
  return TC_KEY_CHALLENGE_OK;
}

TC_key_challenge_result TC_key_challenge_verify(const TC_X509_public_key* key,
    TC_bytes signature, const TC_X509_signature_provider* provider,
    TC_key_challenge_workspace* workspace, TC_work_budget* work)
{
  TC_key_challenge_result result = TC_KEY_CHALLENGE_ERROR;
  if (!workspace) return result;
  if (workspace->state != KEY_CHALLENGE_ACTIVE) return result;
  if (!key || !provider || !work || !signature.data || !signature.length ||
      !tc_internal_ranges_disjoint(workspace,sizeof *workspace,key,sizeof *key) ||
      !tc_internal_ranges_disjoint(workspace,sizeof *workspace,provider,sizeof *provider) ||
      !tc_internal_ranges_disjoint(workspace,sizeof *workspace,work,sizeof *work) ||
      !tc_internal_ranges_disjoint(workspace,sizeof *workspace,signature.data,signature.length))
    goto cleanup;
  size_t available = work->remaining;
  const TC_X509_signature_result verified = TC_X509_signature_verify_digest(
      (TC_bytes){workspace->digest,workspace->digest_length},
      &workspace->signature,signature,key,provider,&available);
  work->remaining = (uint32_t)available;
  switch (verified) {
    case TC_X509_SIGNATURE_VALID: result = TC_KEY_CHALLENGE_OK; break;
    case TC_X509_SIGNATURE_INVALID: result = TC_KEY_CHALLENGE_INVALID; break;
    case TC_X509_SIGNATURE_UNSUPPORTED: result = TC_KEY_CHALLENGE_UNSUPPORTED; break;
    case TC_X509_SIGNATURE_LIMIT: result = TC_KEY_CHALLENGE_LIMIT; break;
    default: break;
  }
cleanup:
  TC_key_challenge_clear(workspace);
  return result;
}
#endif
