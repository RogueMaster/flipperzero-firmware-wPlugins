/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509
#include "pki_signature_oid_internal.h"
#include <stddef.h>

#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define TC_SIGNATURE_OID_STORAGE PROGMEM
#if defined(__AVR_HAVE_RAMPZ__)
#define TC_SIGNATURE_OID_READ(i, offset)                                                           \
  pgm_read_byte_far(pgm_get_far_address(signature_oids) + (uint32_t)(i) * sizeof *signature_oids + \
                    (offset))
#else
#define TC_SIGNATURE_OID_READ(i, offset)                                                           \
  pgm_read_byte((const uint8_t*)&signature_oids[(i)] + (offset))
#endif
#else
#define TC_SIGNATURE_OID_STORAGE
#define TC_SIGNATURE_OID_READ(i, offset) (((const uint8_t*)&signature_oids[(i)])[(offset)])
#endif

typedef struct {
  uint8_t oid[9];
  uint8_t length;
  uint8_t kind;
  uint8_t hash;
} tc_pki_signature_oid_entry;

/* A zero hash marks algorithms without a built-in digest verifier. */
static const tc_pki_signature_oid_entry signature_oids[] TC_SIGNATURE_OID_STORAGE = {
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 2}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_UNKNOWN},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 4}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_UNKNOWN},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 5}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_SHA1},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 10}, 9, TC_PKI_SIGNATURE_RSA_PSS, TC_HASH_UNKNOWN},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 11}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_SHA256},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 12}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_SHA384},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 13}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_SHA512},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 14}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_SHA224},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 15}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_UNKNOWN},
    {{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 16}, 9, TC_PKI_SIGNATURE_RSA_V15, TC_HASH_UNKNOWN},
    {{0x2a, 0x86, 0x48, 0xce, 0x38, 4, 3}, 7, TC_PKI_SIGNATURE_DSA, TC_HASH_UNKNOWN},
    {{0x2a, 0x86, 0x48, 0xce, 0x3d, 4, 1}, 7, TC_PKI_SIGNATURE_ECDSA, TC_HASH_SHA1},
    {{0x2a, 0x86, 0x48, 0xce, 0x3d, 4, 3, 1}, 8, TC_PKI_SIGNATURE_ECDSA, TC_HASH_SHA224},
    {{0x2a, 0x86, 0x48, 0xce, 0x3d, 4, 3, 2}, 8, TC_PKI_SIGNATURE_ECDSA, TC_HASH_SHA256},
    {{0x2a, 0x86, 0x48, 0xce, 0x3d, 4, 3, 3}, 8, TC_PKI_SIGNATURE_ECDSA, TC_HASH_SHA384},
    {{0x2a, 0x86, 0x48, 0xce, 0x3d, 4, 3, 4}, 8, TC_PKI_SIGNATURE_ECDSA, TC_HASH_SHA512},
    {{0x2b, 0x65, 112}, 3, TC_PKI_SIGNATURE_ED25519, TC_HASH_UNKNOWN},
    {{0x2b, 0x65, 113}, 3, TC_PKI_SIGNATURE_ED448, TC_HASH_UNKNOWN}};

tc_pki_signature_oid_info tc_pki_signature_oid_classify(TC_bytes oid)
{
  const tc_pki_signature_oid_info unknown = {TC_PKI_SIGNATURE_UNKNOWN, TC_HASH_UNKNOWN};
  for (size_t i = 0; i < sizeof signature_oids / sizeof *signature_oids; ++i) {
    const uint8_t length = TC_SIGNATURE_OID_READ(i, offsetof(tc_pki_signature_oid_entry, length));
    if (oid.length != length)
      continue;
    size_t byte = 0;
    while (byte < length &&
           oid.data[byte] ==
               TC_SIGNATURE_OID_READ(i, offsetof(tc_pki_signature_oid_entry, oid) + byte))
      ++byte;
    if (byte == length)
      return (tc_pki_signature_oid_info){
          (tc_pki_signature_kind)TC_SIGNATURE_OID_READ(i,
                                                       offsetof(tc_pki_signature_oid_entry, kind)),
          (TC_hash_algorithm)TC_SIGNATURE_OID_READ(i, offsetof(tc_pki_signature_oid_entry, hash))};
  }
  return unknown;
}
#endif
