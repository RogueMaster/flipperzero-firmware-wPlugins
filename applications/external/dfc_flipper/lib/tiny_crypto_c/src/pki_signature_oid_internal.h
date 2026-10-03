/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_SIGNATURE_OID_INTERNAL_H_
#define TC_PKI_SIGNATURE_OID_INTERNAL_H_
#include <tiny_crypto/x509.h>
#include "pki_internal.h"

typedef enum {
  TC_PKI_SIGNATURE_UNKNOWN,
  TC_PKI_SIGNATURE_RSA_V15,
  TC_PKI_SIGNATURE_RSA_PSS,
  TC_PKI_SIGNATURE_ECDSA,
  TC_PKI_SIGNATURE_DSA,
  TC_PKI_SIGNATURE_ED25519,
  TC_PKI_SIGNATURE_ED448
} tc_pki_signature_kind;

typedef struct {
  tc_pki_signature_kind kind;
  TC_hash_algorithm hash;
} tc_pki_signature_oid_info;

/* Unknown OIDs remain available to external signature providers. */
tc_pki_signature_oid_info tc_pki_signature_oid_classify(TC_bytes oid);

/* AlgorithmIdentifier parameters for signature schemes without structured
 * parameters. RSASSA-PSS parameters are parsed by their own reader. Unknown
 * schemes are left to external providers. */
static inline TC_TLV_result tc_pki_signature_parameters_check(tc_pki_signature_kind kind,
                                                              TC_bytes parameters,
                                                              TC_TLV_profile profile)
{
  switch (kind) {
  case TC_PKI_SIGNATURE_RSA_V15:
    /* RFC 4055 section 5: readers accept absent or NULL parameters. */
    return !parameters.length || tc_pki_null(parameters, profile) == TC_TLV_OK ? TC_TLV_OK
                                                                               : TC_TLV_INVALID;
  case TC_PKI_SIGNATURE_ECDSA:
  case TC_PKI_SIGNATURE_DSA:
  case TC_PKI_SIGNATURE_ED25519:
  case TC_PKI_SIGNATURE_ED448:
    /* RFC 5758 section 3, RFC 3279 section 2.2.2, RFC 8410 section 3: absent. */
    return parameters.length ? TC_TLV_INVALID : TC_TLV_OK;
  default:
    return TC_TLV_OK;
  }
}
#endif
