/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509
#include <tiny_crypto/x509.h>
#include "pki_internal.h"
#include "pki_key_internal.h"
#include <limits.h>
#include <string.h>

static TC_TLV_result integer(TC_TLV_reader* reader, TC_bytes* out)
{
  TC_TLV_element element;
  if (TC_TLV_next(reader, &element) != TC_TLV_OK)
    return TC_TLV_INVALID;
  return TC_DER_positive_integer(element.encoded, out) == TC_TLV_OK ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result bit_count(TC_bytes value, unsigned* bits)
{
  uint8_t first = value.data[0];
  unsigned count = 0;
  if (value.length > UINT_MAX / 8)
    return TC_TLV_LIMIT;
  while (first) {
    ++count;
    first >>= 1;
  }
  *bits = (unsigned)(value.length - 1) * 8 + count;
  return TC_TLV_OK;
}

static TC_TLV_result rsa(TC_X509_public_key* key)
{
  TC_DER_rsa_public_key parsed;
  if (TC_DER_rsa_public(key->key, &parsed) != TC_TLV_OK)
    return TC_TLV_INVALID;
  key->modulus = parsed.modulus;
  key->exponent = parsed.exponent;
  if (!(key->modulus.data[key->modulus.length - 1] & 1) ||
      !(key->exponent.data[key->exponent.length - 1] & 1) ||
      (key->exponent.length == 1 && key->exponent.data[0] < 3) ||
      key->exponent.length > key->modulus.length ||
      (key->exponent.length == key->modulus.length &&
       memcmp(key->exponent.data, key->modulus.data, key->modulus.length) >= 0))
    return TC_TLV_INVALID;
  return bit_count(key->modulus, &key->bits);
}

/* RFC 3279 section 2.3.2: DSAPublicKey ::= INTEGER, and the optional
 * Dss-Parms ::= SEQUENCE { p, q, g } sets the key size from p. */
static TC_TLV_result dsa(TC_X509_public_key* key)
{
  enum { DSS_PARMS_ELEMENTS = 4, DSS_PARMS_DEPTH = 1 };
  TC_bytes parameters = key->algorithm.parameters, value;
  TC_TLV_reader reader;
  TC_TLV_result result;
  if (TC_DER_positive_integer(key->key, &value) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (!parameters.length)
    return TC_TLV_OK;
  /* The fixed schema bounds the parameter parse. */
  const TC_TLV_limits schema = {parameters.length, parameters.length, DSS_PARMS_ELEMENTS,
                                DSS_PARMS_DEPTH};
  if (tc_pki_value_open(&reader, parameters, 0x30, &schema, 0) != TC_TLV_OK ||
      integer(&reader, &value) != TC_TLV_OK)
    return TC_TLV_INVALID;
  result = bit_count(value, &key->bits);
  if (result != TC_TLV_OK)
    return result;
  if (integer(&reader, &value) != TC_TLV_OK || integer(&reader, &value) != TC_TLV_OK ||
      !tc_pki_end(&reader))
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

static TC_TLV_result ec(TC_X509_public_key* key)
{
  size_t coordinate;
  uint8_t form = key->key.data[0];
  if (TC_DER_oid(key->algorithm.parameters, &key->curve_oid) != TC_TLV_OK)
    return TC_TLV_INVALID;
  key->curve = tc_pki_curve(key->curve_oid, &key->bits);
  if (form != 2 && form != 3 && form != 4)
    return TC_TLV_INVALID;
  if (key->bits) {
    coordinate = (key->bits + 7u) / 8u;
    if (key->key.length != 1 + coordinate * (form == 4 ? 2u : 1u))
      return TC_TLV_INVALID;
  } else if (key->key.length < 2 || (form == 4 && !(key->key.length & 1)))
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_subject_public_key(TC_bytes encoded, TC_X509_public_key* out)
{
  const TC_bytes ec_oid = tc_pki_ec_public_key_oid();
  TC_DER_public_key decoded;
  TC_X509_public_key key;
  TC_TLV_result result;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = TC_DER_subject_public_key(encoded, &decoded);
  if (result != TC_TLV_OK)
    return result;
  memset(&key, 0, sizeof key);
  key.algorithm = decoded.algorithm;
  key.key = decoded.key;
  result = tc_pki_rsa_key_algorithm(&key.algorithm, &key.type);
  if (result != TC_TLV_OK)
    return result;
  if (key.type == TC_KEY_RSA || key.type == TC_KEY_RSA_PSS) {
    result = rsa(&key);
  } else if (tc_pki_equal(key.algorithm.oid, ec_oid)) {
    key.type = TC_KEY_EC;
    result = ec(&key);
  } else if (tc_pki_equal(key.algorithm.oid, tc_pki_dsa_oid())) {
    key.type = TC_KEY_DSA;
    result = dsa(&key);
  } else if (key.algorithm.oid.length == 3 && key.algorithm.oid.data[0] == 0x2b &&
             key.algorithm.oid.data[1] == 0x65 && key.algorithm.oid.data[2] >= 110 &&
             key.algorithm.oid.data[2] <= 113) {
    unsigned id = key.algorithm.oid.data[2];
    size_t bytes = id == 110 || id == 112 ? 32 : id == 111 ? 56 : 57;
    if (key.algorithm.parameters.length || key.key.length != bytes)
      return TC_TLV_INVALID;
    key.type = id == 110   ? TC_KEY_X25519
               : id == 111 ? TC_KEY_X448
               : id == 112 ? TC_KEY_ED25519
                           : TC_KEY_ED448;
    key.bits = id == 110 || id == 112 ? 255 : 448;
  }
  if (result != TC_TLV_OK)
    return result;
  *out = key;
  return TC_TLV_OK;
}
#endif
