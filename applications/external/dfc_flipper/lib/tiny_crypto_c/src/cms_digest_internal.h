/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_CMS_DIGEST_INTERNAL_H_
#define TC_CMS_DIGEST_INTERNAL_H_
#include <tiny_crypto/cms.h>
#include "hash_info_internal.h"
#include "pki_internal.h"
#include "x509_path_internal.h"

/* Entry validation for a content-digest comparison: algorithm is a known
 * SHA-1 or SHA-2 hash and digest has its length. Returns OK, UNSUPPORTED for
 * another algorithm, or ARGUMENT for a digest of the wrong length. Charges no
 * work. */
static inline TC_TLV_result tc_cms_content_digest_length_check(TC_hash_algorithm algorithm,
                                                               TC_bytes digest)
{
  tc_hash_info info;
  if (!tc_hash_info_get(algorithm, &info))
    return TC_TLV_UNSUPPORTED;
  return digest.length == info.digest_length ? TC_TLV_OK : TC_TLV_ARGUMENT;
}

/* Bind signedAttrs to a computed content digest (RFC 5652 section 11.2).
 * Hash content once per algorithm, then reuse the digest across signers. The
 * caller has validated the arguments: attributes came from
 * TC_CMS_signed_attributes_read or passed the public entry checks,
 * expected_type is nonempty and digest passed
 * tc_cms_content_digest_length_check. Inputs are borrowed and disjoint from
 * work and matched. Charges the expected type bytes and the digest length.
 * Signature and trust validation are separate steps. */
static inline TC_TLV_result
tc_cms_content_digest_compare(const TC_CMS_signed_attributes* attributes, TC_bytes expected_type,
                              TC_bytes digest, size_t* work, int* matched)
{
  if (tc_pki_work_charge(work, expected_type.length) != TC_TLV_OK ||
      tc_pki_work_charge(work, digest.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  *matched = tc_pki_equal(expected_type, attributes->content_type) &&
             attributes->message_digest.length == digest.length &&
             TC_ct_equal(digest, attributes->message_digest) == TC_OK;
  return TC_TLV_OK;
}
#endif
