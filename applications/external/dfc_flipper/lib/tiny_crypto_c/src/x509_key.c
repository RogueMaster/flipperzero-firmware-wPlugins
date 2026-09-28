/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509
#include <tiny_crypto/x509.h>
#include "pki_internal.h"
#include "pki_key_internal.h"
#include <limits.h>
#include <string.h>

static const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 4, 1};

static int equals(TC_bytes oid, const uint8_t* bytes, size_t length)
{ return oid.length == length && !memcmp(oid.data, bytes, length); }


static TC_TLV_result integer(TC_TLV_reader* reader, TC_bytes* out)
{
  TC_TLV_element element;
  if (TC_TLV_next(reader, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  return TC_DER_positive_integer(element.encoded.data, element.encoded.length, out) == TC_TLV_OK ?
      TC_TLV_OK : TC_TLV_INVALID;
}


static TC_TLV_result bit_count(TC_bytes value, unsigned* bits)
{
  uint8_t first = value.data[0];
  unsigned count = 0;
  if (value.length > UINT_MAX / 8) return TC_TLV_LIMIT;
  while (first) { ++count; first >>= 1; }
  *bits = (unsigned)(value.length - 1) * 8 + count;
  return TC_TLV_OK;
}

static TC_TLV_result rsa(TC_X509_public_key* key)
{
  TC_DER_rsa_public_key parsed;
  if (TC_DER_rsa_public(key->key.data,key->key.length,&parsed) != TC_TLV_OK)
    return TC_TLV_INVALID;
  key->modulus = parsed.modulus;
  key->exponent = parsed.exponent;
  if (!(key->modulus.data[key->modulus.length - 1] & 1) ||
      !(key->exponent.data[key->exponent.length - 1] & 1) ||
      (key->exponent.length == 1 && key->exponent.data[0] < 3) ||
      key->exponent.length > key->modulus.length ||
      (key->exponent.length == key->modulus.length &&
       memcmp(key->exponent.data, key->modulus.data, key->modulus.length) >= 0)) return TC_TLV_INVALID;
  return bit_count(key->modulus, &key->bits);
}

static TC_TLV_result ec(TC_X509_public_key* key)
{
  size_t coordinate;
  uint8_t form = key->key.data[0];
  if (TC_DER_oid(key->algorithm.parameters.data, key->algorithm.parameters.length, &key->curve_oid) != TC_TLV_OK)
    return TC_TLV_INVALID;
  key->curve = tc_pki_curve(key->curve_oid, &key->bits);
  if (form != 2 && form != 3 && form != 4) return TC_TLV_INVALID;
  if (key->bits) {
    coordinate = (key->bits + 7u) / 8u;
    if (key->key.length != 1 + coordinate * (form == 4 ? 2u : 1u)) return TC_TLV_INVALID;
  } else if (key->key.length < 2 || (form == 4 && !(key->key.length & 1))) return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_subject_public_key(const uint8_t* data, size_t length, TC_X509_public_key* out)
{
  const TC_bytes ec_oid = tc_pki_ec_public_key_oid();
  static const uint8_t dsa_oid[] = {0x2a,0x86,0x48,0xce,0x38,4,1};
  TC_DER_public_key decoded;
  TC_X509_public_key key;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_DER_subject_public_key(data, length, &decoded);
  if (result != TC_TLV_OK) return result;
  memset(&key, 0, sizeof key);
  key.algorithm = decoded.algorithm; key.key = decoded.key;
  result = tc_pki_rsa_key_algorithm(&key.algorithm,&key.type);
  if (result != TC_TLV_OK) return result;
  if (key.type == TC_KEY_RSA || key.type == TC_KEY_RSA_PSS) {
    result = rsa(&key);
  } else if (equals(key.algorithm.oid, ec_oid.data, ec_oid.length)) {
    key.type = TC_KEY_EC;
    result = ec(&key);
  } else if (equals(key.algorithm.oid, dsa_oid, sizeof dsa_oid)) {
    TC_TLV_reader reader;
    TC_bytes value;
    key.type = TC_KEY_DSA;
    if (TC_TLV_reader_init(&reader, key.key.data, key.key.length, TC_TLV_DER, &limits) != TC_TLV_OK ||
        integer(&reader, &value) != TC_TLV_OK || reader.offset != key.key.length) return TC_TLV_INVALID;
    if (key.algorithm.parameters.length) {
      if (TC_DER_sequence(key.algorithm.parameters.data, key.algorithm.parameters.length, &value) != TC_TLV_OK ||
          TC_TLV_reader_init(&reader, value.data, value.length, TC_TLV_DER, &limits) != TC_TLV_OK ||
          integer(&reader, &value) != TC_TLV_OK) return TC_TLV_INVALID;
      result = bit_count(value, &key.bits);
      if (result != TC_TLV_OK) return result;
      if (integer(&reader, &value) != TC_TLV_OK || integer(&reader, &value) != TC_TLV_OK ||
          reader.offset != reader.input.length) return TC_TLV_INVALID;
    }
  } else if (key.algorithm.oid.length == 3 && key.algorithm.oid.data[0] == 0x2b &&
             key.algorithm.oid.data[1] == 0x65 && key.algorithm.oid.data[2] >= 110 && key.algorithm.oid.data[2] <= 113) {
    unsigned id = key.algorithm.oid.data[2];
    size_t bytes = id == 110 || id == 112 ? 32 : id == 111 ? 56 : 57;
    if (key.algorithm.parameters.length || key.key.length != bytes) return TC_TLV_INVALID;
    key.type = id == 110 ? TC_KEY_X25519 : id == 111 ? TC_KEY_X448 :
               id == 112 ? TC_KEY_ED25519 : TC_KEY_ED448;
    key.bits = id == 110 || id == 112 ? 255 : 448;
  }
  if (result != TC_TLV_OK) return result;
  *out = key;
  return TC_TLV_OK;
}
#endif
