/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_HASH_INTERNAL_H_
#define TC_PKI_HASH_INTERNAL_H_
#include "hash_info_internal.h"
#include "pki_internal.h"

/* RFC 3370/5754 digest identifiers accept absent or NULL parameters.
 * This rule does not apply to signature-algorithm parameters. */
static inline TC_TLV_result tc_pki_hash_parameters_profile(const TC_DER_algorithm* algorithm,
    TC_TLV_profile profile)
{
  if (!algorithm || (algorithm->parameters.length && !algorithm->parameters.data))
    return TC_TLV_ARGUMENT;
  if (profile != TC_TLV_DER && profile != TC_TLV_BER) return TC_TLV_ARGUMENT;
  return !algorithm->parameters.length || tc_pki_null(algorithm->parameters,profile) == TC_TLV_OK ?
      TC_TLV_OK : TC_TLV_INVALID;
}

static inline TC_TLV_result tc_pki_hash_parameters(const TC_DER_algorithm* algorithm)
{ return tc_pki_hash_parameters_profile(algorithm,TC_TLV_DER); }

/* Resolve metadata independently of compiled hash implementations. Unknown
 * OIDs return UNSUPPORTED. Malformed OIDs and invalid parameters for known
 * algorithms return INVALID. */
static inline TC_TLV_result tc_pki_hash_algorithm_profile(const TC_DER_algorithm* algorithm,
    TC_TLV_profile profile, TC_hash_algorithm* out)
{
  TC_TLV_result result;
  if (!algorithm || !out || (algorithm->oid.length && !algorithm->oid.data))
    return TC_TLV_ARGUMENT;
  if (profile != TC_TLV_DER && profile != TC_TLV_BER) return TC_TLV_ARGUMENT;
  if (TC_DER_oid_contents(algorithm->oid.data,algorithm->oid.length) != TC_TLV_OK)
    return TC_TLV_INVALID;
  for (unsigned id = TC_HASH_SHA1; id <= TC_HASH_SHA512; ++id) {
    tc_hash_info info;
    if (!tc_hash_info_get((TC_hash_algorithm)id,&info) ||
        !tc_pki_equal(info.oid,algorithm->oid)) continue;
    result = tc_pki_hash_parameters_profile(algorithm,profile);
    if (result != TC_TLV_OK) return result;
    *out = (TC_hash_algorithm)id;
    return TC_TLV_OK;
  }
  return TC_TLV_UNSUPPORTED;
}
static inline TC_TLV_result tc_pki_hash_algorithm(const TC_DER_algorithm* algorithm,
    TC_hash_algorithm* out)
{ return tc_pki_hash_algorithm_profile(algorithm,TC_TLV_DER,out); }
#endif
