/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_KEY_INTERNAL_H_
#define TC_PKI_KEY_INTERNAL_H_
#include <tiny_crypto/key.h>

/* SubjectPublicKeyInfo algorithm OIDs (RFC 3279, RFC 4055, RFC 5480). */
static inline TC_bytes tc_pki_ec_public_key_oid(void)
{
  static const uint8_t oid[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 2, 1};
  return (TC_bytes){oid, sizeof oid};
}

static inline TC_bytes tc_pki_rsa_encryption_oid(void)
{
  static const uint8_t oid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 1};
  return (TC_bytes){oid, sizeof oid};
}

static inline TC_bytes tc_pki_rsa_pss_oid(void)
{
  static const uint8_t oid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 10};
  return (TC_bytes){oid, sizeof oid};
}

static inline TC_bytes tc_pki_dsa_oid(void)
{
  static const uint8_t oid[] = {0x2a, 0x86, 0x48, 0xce, 0x38, 4, 1};
  return (TC_bytes){oid, sizeof oid};
}

/* Classify an already decoded algorithm and check RSA parameters.
 * Other OIDs produce KEY_UNKNOWN. out changes only on OK. */
TC_TLV_result tc_pki_rsa_key_algorithm(const TC_DER_algorithm* algorithm, TC_key_type* out);

#endif
