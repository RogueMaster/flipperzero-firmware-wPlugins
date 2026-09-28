/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_CMS_DIGEST_INTERNAL_H_
#define TC_CMS_DIGEST_INTERNAL_H_
#include <tiny_crypto/cms.h>
#include "hash_info_internal.h"
#include "pki_internal.h"
#include "x509_path_internal.h"

/* Bind signedAttrs to a computed content digest. Hash content once per algorithm,
 * then reuse the digest across signers. Inputs are borrowed and disjoint from
 * work and matched. A match establishes neither a valid signature nor trust. */
static inline TC_TLV_result tc_cms_content_digest_check(const TC_CMS_signed_attributes* attributes,
    TC_bytes expected_type, TC_hash_algorithm algorithm, TC_bytes digest,
    size_t* work, int* matched)
{
  tc_hash_info info;
  if (!attributes || !work || !matched || !digest.data ||
      !expected_type.data || !expected_type.length || !attributes->content_type.data ||
      !attributes->content_type.length || !attributes->message_digest.data)
    return TC_TLV_ARGUMENT;
  if (!tc_hash_info_get(algorithm,&info)) return TC_TLV_UNSUPPORTED;
  if (digest.length != info.digest_length) return TC_TLV_ARGUMENT;
  if (tc_x509_path_charge(work,expected_type.length) != TC_TLV_OK ||
      tc_x509_path_charge(work,info.digest_length) != TC_TLV_OK) return TC_TLV_LIMIT;
  *matched = tc_pki_equal(expected_type,attributes->content_type) &&
    attributes->message_digest.length == info.digest_length &&
    TC_ct_equal(digest.data,attributes->message_digest.data,info.digest_length) == TC_OK;
  return TC_TLV_OK;
}
#endif
