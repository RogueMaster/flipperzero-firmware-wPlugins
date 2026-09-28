/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_EAC_CVC_H_
#define TINY_CRYPTO_EAC_CVC_H_
#include <tiny_crypto/der.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_EAC_RSA_V15, TC_EAC_RSA_PSS, TC_EAC_ECDSA, TC_EAC_ECDH } TC_EAC_algorithm;
typedef struct {
  TC_bytes oid, modulus, exponent, p, a, b, generator, order, point, cofactor;
  TC_EAC_algorithm algorithm;
  unsigned hash_bits;
  int has_domain;
} TC_EAC_CVC_public_key;
typedef struct { unsigned year; uint8_t month, day; } TC_EAC_date;
typedef enum { TC_EAC_TERMINAL, TC_EAC_DV_FOREIGN, TC_EAC_DV_DOMESTIC, TC_EAC_CVCA } TC_EAC_role;
typedef enum { TC_EAC_IS = 1, TC_EAC_AT = 2, TC_EAC_ST = 3 } TC_EAC_terminal_type;
typedef struct {
  TC_bytes encoded, signed_data, issuer, holder, authorization_oid, authorization;
  TC_bytes extensions, signature;
  TC_EAC_CVC_public_key public_key;
  TC_EAC_date effective, expiration;
  TC_EAC_role role;
  TC_EAC_terminal_type terminal_type;
} TC_EAC_CVC;
typedef struct { TC_TLV_frame* frames; size_t frame_capacity; } TC_EAC_CVC_workspace;

/* One TR-03110 certificate. Spans borrow input; out is unchanged on failure.
 * No signature, trust-chain, or current-time verification is performed. */
TC_TLV_result TC_EAC_CVC_read(const uint8_t* data, size_t length,
    const TC_TLV_limits* limits, TC_EAC_CVC_workspace* workspace, TC_EAC_CVC* out);
TC_TLV_result TC_EAC_CVC_public_key_read(const uint8_t* data, size_t length,
    const TC_TLV_limits* limits, TC_EAC_CVC_public_key* out);

/* issuer must have resolved parameters. inherited supplies the subject's EC
 * domain when absent from its certificate. Missing context is ARGUMENT.
 * Checks encoding widths, not curve membership or signature mathematics. */
TC_TLV_result TC_EAC_CVC_check_encoding(const TC_EAC_CVC* certificate,
    const TC_EAC_CVC_public_key* issuer, const TC_EAC_CVC_public_key* inherited);

typedef struct { TC_bytes oid, fields; } TC_EAC_CVC_extension;
/* Pass certificate.extensions, or {NULL,0} when absent. The iterator counts
 * templates, OIDs, and immediate fields against max_elements. Field contents
 * remain opaque; read checks the full tree's depth and element budgets. */
TC_TLV_result TC_EAC_CVC_extensions_init(TC_TLV_reader* reader, TC_bytes encoded,
    const TC_TLV_limits* limits);
TC_TLV_result TC_EAC_CVC_extension_next(TC_TLV_reader* reader, TC_EAC_CVC_extension* out);
#ifdef __cplusplus
}
#endif
#endif
