/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_HASH_PARTS_INTERNAL_H_
#define TC_PKI_HASH_PARTS_INTERNAL_H_
#include "hash_dispatch_internal.h"
#include "pki_tree_internal.h"
#include "x509_path_internal.h"

/* Callers preflight storage; this bounds and hashes borrowed parts in order. */
static inline TC_TLV_result tc_pki_hash_parts(const TC_bytes* parts, size_t count,
    TC_hash_algorithm algorithm, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, tc_hash_workspace* scratch, uint8_t* digest)
{
  size_t length = 0;
  if (!tc_hash_available(algorithm)) return TC_TLV_UNSUPPORTED;
  if (count && !parts) return TC_TLV_ARGUMENT;
  if (tc_x509_path_charge(tree->work,count) != TC_TLV_OK) return TC_TLV_LIMIT;
  for (size_t i = 0; i < count; ++i) {
    if (parts[i].length && !parts[i].data) return TC_TLV_ARGUMENT;
    if (parts[i].length > limits->max_input - length ||
        parts[i].length > limits->max_value - length) return TC_TLV_LIMIT;
    length += parts[i].length;
  }
  if (tc_x509_path_charge(tree->work,length) != TC_TLV_OK) return TC_TLV_LIMIT;
  return tc_hash_digest_parts(algorithm,parts,count,digest,scratch) == TC_OK ?
      TC_TLV_OK : TC_TLV_ARGUMENT;
}
#endif
