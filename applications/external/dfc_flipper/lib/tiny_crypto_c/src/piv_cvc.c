/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_PIV_CVC
#include <tiny_crypto/piv_cvc.h>
#include "internal.h"
#include "pki_internal.h"
#include <string.h>

static const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 8, 1};

static int equals(TC_bytes span, const uint8_t* bytes, size_t length)
{ return span.length == length && memcmp(span.data, bytes, length) == 0; }

static TC_TLV_result public_key(TC_bytes encoded, TC_PIV_CVC* cvc)
{
  TC_EC_curve curve;
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result = TC_TLV_reader_init(&reader, encoded.data, encoded.length,
                                            TC_TLV_ISO7816, &limits);
  if (result != TC_TLV_OK) return result;
  if (tc_pki_next(&reader, 6, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  result = TC_DER_oid_contents(element.value.data, element.value.length);
  if (result != TC_TLV_OK) return result;
  cvc->curve_oid = element.value;
  curve = tc_pki_curve(cvc->curve_oid, &cvc->key_bits);
  if (curve != TC_EC_P256 && curve != TC_EC_P384) return TC_TLV_UNSUPPORTED;
  if (tc_pki_next(&reader, 0x86, &element) != TC_TLV_OK || !tc_pki_end(&reader)) return TC_TLV_INVALID;
  if (element.value.length != 1 + cvc->key_bits / 4 || element.value.data[0] != 4)
    return TC_TLV_INVALID;
  cvc->public_key = element.value;
  return TC_TLV_OK;
}

static TC_TLV_result signature(TC_bytes encoded, TC_PIV_CVC* cvc)
{
  static const uint8_t sha256_ecdsa[] = {0x2a,0x86,0x48,0xce,0x3d,4,3,2};
  static const uint8_t sha384_ecdsa[] = {0x2a,0x86,0x48,0xce,0x3d,4,3,3};
  static const uint8_t sha256_rsa[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1,11};
  TC_DER_public_key decoded;
  /* DigitalSignature has the same AlgorithmIdentifier/BIT STRING layout as SPKI. */
  TC_TLV_result result = TC_DER_subject_public_key(encoded.data, encoded.length, &decoded);
  if (result != TC_TLV_OK) return result;
  cvc->signature_algorithm = decoded.algorithm;
  cvc->signature = decoded.key;
  if (cvc->role == TC_PIV_CVC_INTERMEDIATE) {
    if (!equals(cvc->signature_algorithm.oid, sha256_rsa, sizeof sha256_rsa)) return TC_TLV_UNSUPPORTED;
    return TC_DER_null(cvc->signature_algorithm.parameters.data,
                       cvc->signature_algorithm.parameters.length) == TC_TLV_OK ? TC_TLV_OK : TC_TLV_INVALID;
  }
  if (!equals(cvc->signature_algorithm.oid,
      cvc->key_bits == 256 ? sha256_ecdsa : sha384_ecdsa, sizeof sha256_ecdsa)) return TC_TLV_UNSUPPORTED;
  if (cvc->signature_algorithm.parameters.length) return TC_TLV_INVALID;
  result = TC_DER_ecdsa_signature(cvc->signature.data, cvc->signature.length, &cvc->ecdsa);
  if (result != TC_TLV_OK) return result;
  if (cvc->ecdsa.r.length > cvc->key_bits / 8 || cvc->ecdsa.s.length > cvc->key_bits / 8)
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_CVC_read(const uint8_t* data, size_t length, TC_PIV_CVC* out)
{
  TC_PIV_CVC cvc;
  TC_TLV_element element;
  TC_TLV_reader reader;
  TC_TLV_result result;
  if (!out || (!data && length) ||
      !tc_internal_ranges_disjoint(data,length,out,sizeof *out)) return TC_TLV_ARGUMENT;
  result = TC_TLV_read(data, length, TC_TLV_ISO7816, &limits, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x7f21) || element.encoded.length != length) return TC_TLV_INVALID;
  memset(&cvc, 0, sizeof cvc);
  cvc.signed_data.data = element.value.data;
  result = TC_TLV_reader_init(&reader, element.value.data, element.value.length, TC_TLV_ISO7816, &limits);
  if (result != TC_TLV_OK) return result;
  /* SP 800-73 Part 2, section 4.1.5 fixes both the tags and their order. */
  if (tc_pki_next(&reader, 0x5f29, &element) != TC_TLV_OK || element.value.length != 1) return TC_TLV_INVALID;
  if (element.value.data[0] != 0x80) return TC_TLV_UNSUPPORTED;
  if (tc_pki_next(&reader, 0x42, &element) != TC_TLV_OK || element.value.length != 8) return TC_TLV_INVALID;
  cvc.issuer = element.value;
  if (tc_pki_next(&reader, 0x5f20, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  cvc.subject = element.value;
  if (tc_pki_next(&reader, 0x7f49, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  result = public_key(element.value, &cvc);
  if (result != TC_TLV_OK) return result;
  if (tc_pki_next(&reader, 0x5f4c, &element) != TC_TLV_OK || element.value.length != 1) return TC_TLV_INVALID;
  cvc.role = element.value.data[0];
  if (cvc.role != TC_PIV_CVC_CARD_APPLICATION && cvc.role != TC_PIV_CVC_INTERMEDIATE)
    return TC_TLV_UNSUPPORTED;
  if (cvc.subject.length != (cvc.role == TC_PIV_CVC_CARD_APPLICATION ? 16u : 8u))
    return TC_TLV_INVALID;
  cvc.signed_data.length = reader.offset;
  if (tc_pki_next(&reader, 0x5f37, &element) != TC_TLV_OK || !tc_pki_end(&reader)) return TC_TLV_INVALID;
  result = signature(element.value, &cvc);
  if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  *out = cvc;
  return TC_TLV_OK;
}
#endif
