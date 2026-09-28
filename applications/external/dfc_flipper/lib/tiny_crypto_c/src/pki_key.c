/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_DER
#include "pki_key_internal.h"
#include "pki_hash_internal.h"
#include "pki_tree_internal.h"
#include "pki_signature_internal.h"

static const TC_TLV_limits pss_limits = {SIZE_MAX,SIZE_MAX,4,1};

static TC_TLV_result parameter_algorithm(TC_bytes encoded, TC_TLV_profile profile,
    const TC_TLV_limits* bounds, const tc_pki_tree_workspace* tree, TC_DER_algorithm* out)
{
  return tree ? tc_pki_tree_algorithm(encoded,profile,bounds,tree,out) :
      TC_DER_algorithm_identifier(encoded.data,encoded.length,out);
}

static TC_TLV_result hash_algorithm(TC_bytes encoded, TC_TLV_profile profile,
    const TC_TLV_limits* bounds, const tc_pki_tree_workspace* tree, TC_DER_algorithm* out)
{
  TC_DER_algorithm algorithm;
  TC_TLV_result result = parameter_algorithm(encoded,profile,bounds,tree,&algorithm);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_hash_parameters_profile(&algorithm,profile);
  if (result == TC_TLV_OK) *out = algorithm;
  return result;
}

TC_TLV_result tc_pki_pss_read_profile(TC_bytes encoded, TC_TLV_profile profile,
    const TC_TLV_limits* bounds, const tc_pki_tree_workspace* tree, tc_pki_pss_parameters* out)
{
  static const uint8_t mgf1[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1,8};
  static const uint8_t default_salt = 20;
  tc_hash_info sha1;
  tc_pki_pss_parameters parsed;
  TC_bytes contents, value;
  TC_TLV_reader reader;
  TC_TLV_element field;
  TC_TLV_result result;
  unsigned previous = 0;
  if (!out || !bounds || (profile != TC_TLV_DER && profile != TC_TLV_BER) ||
      (profile == TC_TLV_BER && !tree)) return TC_TLV_ARGUMENT;
  (void)tc_hash_info_get(TC_HASH_SHA1,&sha1);
  parsed.hash = (TC_DER_algorithm){sha1.oid,{NULL,0}};
  parsed.mgf_hash = parsed.hash;
  parsed.salt_length = (TC_bytes){&default_salt,1};
  if (tree) result = tc_pki_tree_open(encoded,0x30,profile,bounds,tree,&reader);
  else {
    result = TC_DER_sequence(encoded.data,encoded.length,&contents);
    if (result == TC_TLV_OK)
      result = TC_TLV_reader_init(&reader,contents.data,contents.length,TC_TLV_DER,bounds);
  }
  if (result != TC_TLV_OK) return result;
  while ((result = tree ? tc_pki_tree_next(&reader,tree,&field) : TC_TLV_next(&reader,&field)) == TC_TLV_OK) {
    unsigned tag = field.header.tag[0];
    if (field.header.tag_length != 1 || tag < 0xa0 || tag > 0xa3 || tag <= previous)
      return TC_TLV_INVALID;
    previous = tag;
    if (tag == 0xa0) {
      result = hash_algorithm(field.value,profile,bounds,tree,&parsed.hash);
      if (result != TC_TLV_OK) return result;
    } else if (tag == 0xa1) {
      TC_DER_algorithm mask;
      result = parameter_algorithm(field.value,profile,bounds,tree,&mask);
      if (result != TC_TLV_OK) return result;
      if (!tc_pki_equal(mask.oid,(TC_bytes){mgf1,sizeof mgf1})) return TC_TLV_UNSUPPORTED;
      result = hash_algorithm(mask.parameters,profile,bounds,tree,&parsed.mgf_hash);
      if (result != TC_TLV_OK) return result;
    } else {
      int negative;
      if (tree) {
        TC_TLV_element integer;
        result = tc_pki_tree_read(field.value,profile,bounds,tree,&integer);
        if (result != TC_TLV_OK) return result;
        if (!tc_pki_tag(&integer,2) || integer.encoded.length != field.value.length)
          return TC_TLV_INVALID;
        value = integer.value;
        result = TC_DER_integer_contents(value.data,value.length);
        if (result != TC_TLV_OK) return result;
        negative = (value.data[0] & 0x80) != 0;
      } else {
        result = TC_DER_integer(field.value.data,field.value.length,&value,&negative);
        if (result != TC_TLV_OK) return result;
      }
      if (negative) return TC_TLV_INVALID;
      if (tag == 0xa3 && (value.length != 1 || value.data[0] != 1)) return TC_TLV_INVALID;
      if (tag == 0xa2) parsed.salt_length = value;
    }
  }
  /* RFC 4055 requires readers to accept explicitly encoded algorithm defaults. */
  if (result != TC_TLV_END) return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_pki_pss_read(TC_bytes encoded, tc_pki_pss_parameters* out)
{
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_pss_read_profile(encoded,TC_TLV_DER,&pss_limits,NULL,out);
  return result == TC_TLV_OK || result == TC_TLV_UNSUPPORTED ? result : TC_TLV_INVALID;
}

TC_TLV_result tc_x509_pss_parameters(TC_bytes encoded)
{
  tc_pki_pss_parameters parsed;
  return tc_pki_pss_read(encoded,&parsed);
}

TC_TLV_result tc_pki_rsa_key_algorithm(const TC_DER_algorithm* algorithm,
                                      TC_key_type* out)
{
  static const uint8_t rsa_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1,1};
  static const uint8_t pss_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1,10};
  TC_key_type type = TC_KEY_UNKNOWN;
  if (!algorithm || !out) return TC_TLV_ARGUMENT;
  if (tc_pki_equal(algorithm->oid,(TC_bytes){rsa_oid,sizeof rsa_oid})) {
    type = TC_KEY_RSA;
    if (TC_DER_null(algorithm->parameters.data,algorithm->parameters.length) != TC_TLV_OK)
      return TC_TLV_INVALID;
  } else if (tc_pki_equal(algorithm->oid,(TC_bytes){pss_oid,sizeof pss_oid})) {
    type = TC_KEY_RSA_PSS;
    if (algorithm->parameters.length) {
      TC_TLV_result result = tc_x509_pss_parameters(algorithm->parameters);
      if (result != TC_TLV_OK) return result;
    }
  }
  *out = type;
  return TC_TLV_OK;
}

TC_TLV_result TC_KEY_rsa_private_read(TC_bytes encoded,
                                     TC_KEY_rsa_private_key* out)
{
  TC_KEY_rsa_private_key key;
  if (!out) return TC_TLV_ARGUMENT;
  TC_TLV_result result = TC_DER_private_key_info(encoded.data,encoded.length,&key.container);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_rsa_key_algorithm(&key.container.algorithm,&key.type);
  if (result != TC_TLV_OK) return result;
  if (key.type == TC_KEY_UNKNOWN) return TC_TLV_UNSUPPORTED;
  result = TC_DER_rsa_private(key.container.key.data,key.container.key.length,&key.components);
  if (result != TC_TLV_OK) return result;
  if (key.container.public_key.data) {
    TC_DER_rsa_public_key public_key;
    if (key.container.public_key_unused) return TC_TLV_INVALID;
    result = TC_DER_rsa_public(key.container.public_key.data,key.container.public_key.length,&public_key);
    if (result != TC_TLV_OK) return result;
    if (tc_pki_compare(public_key.modulus,key.components.modulus) ||
        tc_pki_compare(public_key.exponent,key.components.public_exponent)) return TC_TLV_INVALID;
  }
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_KEY_rsa_private_signature_check(const TC_KEY_rsa_private_key* key,
                                                const TC_signature_algorithm* signature)
{
  return key ? tc_pki_signature_usage_check(signature,key->type,key->container.algorithm.parameters) :
      TC_TLV_ARGUMENT;
}
#endif
