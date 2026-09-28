/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "rsa_read.h"
#include "rsa_validate.h"
#include "rsa_sign.h"
#include <tiny_crypto/der.h>
#include <tiny_crypto/key.h>

static TC_RSA_result sign_components(const TC_DER_rsa_private_key* parsed,
    const TC_signature_algorithm* operation,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words)
{
  const TC_RSA_crt crt = {
    {parsed->exponent1.data,parsed->exponent1.length},
    {parsed->exponent2.data,parsed->exponent2.length},
    {parsed->coefficient.data,parsed->coefficient.length}
  };
  /* Components borrow their magnitudes directly from the encoded key. */
  const TC_RSA_private_key key = {
    {{parsed->modulus.data,parsed->modulus.length},
     {parsed->public_exponent.data,parsed->public_exponent.length}},
    {parsed->private_exponent.data,parsed->private_exponent.length},
    {parsed->prime1.data,parsed->prime1.length},
    {parsed->prime2.data,parsed->prime2.length},
    &crt
  };
  const TC_RSA_result valid = example_validate_rsa_key(&key,random,random_context,
      scratch,scratch_words);
  if (valid != TC_RSA_OK) return valid;
  const TC_RSA_workspace workspace = {scratch,scratch_words};
  /* The validated modulus bounds this budget on 16-bit targets too. */
  const size_t crt_work = 32 * key.public_key.modulus.length + 1;
  TC_work_budget work = {(uint32_t)crt_work};
  const TC_RSA_result checked = TC_RSA_validate_crt(&key,&crt,&workspace,&work);
  if (checked != TC_RSA_OK) return checked;
  if (operation->scheme == TC_SIGNATURE_RSA_PSS)
    return example_sign_rsa_pss_sha256_digest(&key,digest,signature,signature_length,
        random,random_context,scratch,scratch_words);
  return example_sign_rsa_v15_digest(&key,operation->hash,digest,signature,signature_length,
      random,random_context,scratch,scratch_words);
}

static TC_RSA_result import_error(TC_TLV_result result)
{
  switch (result) {
    case TC_TLV_ARGUMENT: return TC_RSA_ARGUMENT;
    case TC_TLV_LIMIT: return TC_RSA_LIMIT;
    case TC_TLV_UNSUPPORTED: return TC_RSA_UNSUPPORTED;
    default: return TC_RSA_INVALID;
  }
}

TC_RSA_result example_sign_rsa_der(TC_bytes der, TC_hash_algorithm hash,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words)
{
  TC_DER_rsa_private_key parsed;
  const TC_signature_algorithm operation = {TC_SIGNATURE_RSA_V15,hash,TC_HASH_UNKNOWN,0};
  if (!der.data) return TC_RSA_ARGUMENT;
  const TC_TLV_result read = TC_DER_rsa_private(der.data,der.length,&parsed);
  if (read != TC_TLV_OK) return import_error(read);
  return sign_components(&parsed,&operation,digest,signature,signature_length,
      random,random_context,scratch,scratch_words);
}

static TC_RSA_result sign_pkcs8(TC_bytes der, const TC_signature_algorithm* operation,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words)
{
  TC_KEY_rsa_private_key parsed;
  if (!der.data) return TC_RSA_ARGUMENT;
  TC_TLV_result result = TC_KEY_rsa_private_read((TC_bytes){der.data,der.length},&parsed);
  if (result != TC_TLV_OK) return import_error(result);
  result = TC_KEY_rsa_private_signature_check(&parsed,operation);
  if (result != TC_TLV_OK) return import_error(result);
  return sign_components(&parsed.components,operation,digest,signature,signature_length,
      random,random_context,scratch,scratch_words);
}

TC_RSA_result example_sign_rsa_pkcs8(TC_bytes der, TC_hash_algorithm hash,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words)
{
  const TC_signature_algorithm operation = {TC_SIGNATURE_RSA_V15,hash,TC_HASH_UNKNOWN,0};
  return sign_pkcs8(der,&operation,digest,signature,signature_length,
      random,random_context,scratch,scratch_words);
}

TC_RSA_result example_sign_rsa_pkcs8_pss_sha256(TC_bytes der,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words)
{
  const TC_signature_algorithm operation = {TC_SIGNATURE_RSA_PSS,TC_HASH_SHA256,
    TC_HASH_SHA256,EXAMPLE_RSA_PSS_SHA256_BYTES};
  return sign_pkcs8(der,&operation,digest,signature,signature_length,
      random,random_context,scratch,scratch_words);
}
