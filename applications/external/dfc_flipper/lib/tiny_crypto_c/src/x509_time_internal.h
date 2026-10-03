/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_TIME_INTERNAL_H_
#define TC_X509_TIME_INTERNAL_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/x509_revocation.h>
/* A framed UTC/GeneralizedTime element. Calendar output changes only on OK. */
TC_TLV_result tc_x509_time_value(const TC_TLV_element* element, TC_X509_time* out);
/* within receives 1 when not_before - skew <= at <= not_after + skew. Every
 * time must be valid and not_before must not follow not_after, otherwise the
 * result is INVALID. within changes only on OK. */
TC_TLV_result tc_x509_time_window(const TC_X509_time* at, uint32_t skew_seconds,
                                  const TC_X509_time* not_before, const TC_X509_time* not_after,
                                  int* within);

/* Freshness of CRL or OCSP evidence under the shared revocation time rule
 * (see TC_X509_revocation_time). FUTURE: thisUpdate > at + skew. STALE:
 * thisUpdate is more than a nonzero max_age before at - skew, or nextUpdate
 * <= at - skew. NO_NEXT_UPDATE: next_update is NULL and no earlier state
 * applies. INVALID for an unconvertible time or nextUpdate before thisUpdate.
 * out changes only on OK. */
typedef enum {
  TC_X509_FRESH_CURRENT,
  TC_X509_FRESH_FUTURE,
  TC_X509_FRESH_STALE,
  TC_X509_FRESH_NO_NEXT_UPDATE
} tc_x509_freshness;
TC_TLV_result tc_x509_freshness_at(const TC_X509_revocation_time* time,
                                   const TC_X509_time* this_update, const TC_X509_time* next_update,
                                   tc_x509_freshness* out);
#endif
