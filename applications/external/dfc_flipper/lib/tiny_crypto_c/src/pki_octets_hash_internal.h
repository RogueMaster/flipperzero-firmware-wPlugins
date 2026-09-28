/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_OCTETS_HASH_INTERNAL_H_
#define TC_PKI_OCTETS_HASH_INTERNAL_H_
#include "pki_octets_internal.h"
#include "hash_dispatch_internal.h"

typedef struct {
  TC_hash_algorithm algorithm;
  tc_hash_workspace* workspace;
  size_t* work;
} tc_pki_octets_hash_state;

static TC_TLV_result tc_pki_octets_hash_update(void* context, TC_bytes bytes)
{
  tc_pki_octets_hash_state* state = context;
  if (tc_x509_path_charge(state->work,bytes.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  return tc_hash_update(state->algorithm,state->workspace,bytes) == TC_OK ? TC_TLV_OK : TC_TLV_INVALID;
}

/* Hash OCTET STRING values without flattening constructed BER content.
 * Digest has the selected hash's output size and is written only on success.
 * All writable storage is disjoint from inputs and other writable storage. */
static inline TC_TLV_result tc_pki_octets_hash(TC_bytes encoded,
    TC_TLV_profile profile, const TC_TLV_limits* limits, TC_TLV_frame* frames,
    size_t capacity, TC_hash_algorithm algorithm, tc_hash_workspace* workspace,
    size_t* work, uint8_t* digest)
{
  tc_pki_octets_hash_state state = {algorithm,workspace,work};
  TC_TLV_result result;
  if (!workspace || !work || !digest) return TC_TLV_ARGUMENT;
  if (!tc_hash_available(algorithm)) return TC_TLV_UNSUPPORTED;
  if (tc_hash_init(algorithm,workspace) != TC_OK) return TC_TLV_INVALID;
  result = tc_pki_octets(encoded,profile,limits,frames,capacity,work,
      tc_pki_octets_hash_update,&state);
  if (result == TC_TLV_OK && tc_hash_final(algorithm,workspace,digest) != TC_OK)
    result = TC_TLV_INVALID;
  TC_secure_zero(workspace,sizeof *workspace);
  return result;
}
#endif
