/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_CRL_REASON_INTERNAL_H_
#define TC_PKI_CRL_REASON_INTERNAL_H_
#include <tiny_crypto/der.h>
#include "pki_internal.h"

/* CRLReason values defined by RFC 5280 section 5.3.1 (7 is unassigned).
 * CRL entries and OCSP revokedInfo (RFC 6960 section 4.2.1) share them. */
enum {
  TC_PKI_CRL_REASON_UNSPECIFIED = 0,
  TC_PKI_CRL_REASON_UNUSED = 7,
  TC_PKI_CRL_REASON_REMOVE = 8,
  TC_PKI_CRL_REASON_LAST = 10
};

static inline int tc_pki_crl_reason_known(unsigned reason)
{
  return reason <= TC_PKI_CRL_REASON_LAST && reason != TC_PKI_CRL_REASON_UNUSED;
}

/* Read one complete DER CRLReason ENUMERATED element. INVALID for another
 * tag, trailing bytes, a non-minimal or multi-octet value, or an unassigned
 * reason. out changes only on OK. */
static inline TC_TLV_result tc_pki_crl_reason_read(TC_bytes encoded, unsigned* out)
{
  enum { ENUMERATED_TAG = 10 };
  const TC_TLV_limits limits = {encoded.length, encoded.length, 1, 0};
  TC_TLV_element element;
  if (!out)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = TC_TLV_read(encoded, TC_TLV_DER, &limits, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, ENUMERATED_TAG) || element.encoded.length != encoded.length ||
      TC_DER_integer_contents(element.value) != TC_TLV_OK || element.value.length != 1 ||
      !tc_pki_crl_reason_known(element.value.data[0]))
    return TC_TLV_INVALID;
  *out = element.value.data[0];
  return TC_TLV_OK;
}
#endif
