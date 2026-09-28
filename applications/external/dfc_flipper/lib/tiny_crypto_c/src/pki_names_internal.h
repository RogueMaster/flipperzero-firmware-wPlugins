/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_NAMES_INTERNAL_H_
#define TC_PKI_NAMES_INTERNAL_H_
#include "pki_tree_internal.h"

/* GeneralNames contents, also used by IMPLICIT authorityCertIssuer.
 * Input and writable ranges must be disjoint. */
static inline TC_TLV_result tc_pki_general_names_contents_check(TC_bytes contents,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree)
{
  TC_TLV_reader reader;
  TC_X509_general_name name;
  TC_TLV_result result;
  if (!tree || !tree->work) return TC_TLV_ARGUMENT;
  if (!contents.length) return TC_TLV_INVALID;
  /* The existing reader scans framing and then the name's value. */
  if (tc_x509_path_charge(tree->work,contents.length) != TC_TLV_OK ||
      tc_x509_path_charge(tree->work,contents.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = TC_TLV_reader_init(&reader,contents.data,contents.length,TC_TLV_DER,limits);
  if (result != TC_TLV_OK) return result;
  while (!tc_pki_end(&reader)) {
    if (tc_x509_path_charge(tree->work,1) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = TC_X509_general_name_next(&reader,tree->frames,tree->capacity,&name);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}
typedef TC_X509_distribution_name tc_pki_distribution_name;

/* Validate an RDN without its SET wrapper, including nested DER values. */
static inline TC_TLV_result tc_pki_rdn_contents_check(TC_bytes contents,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree)
{
  enum { RDN_SCANS = 3 };
  TC_TLV_reader attributes, check;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!tree || !tree->work) return TC_TLV_ARGUMENT;
  result = TC_TLV_reader_init(&attributes,contents.data,contents.length,TC_TLV_DER,limits);
  if (result != TC_TLV_OK) return result;
  check = attributes;
  while (!tc_pki_end(&check)) {
    result = tc_pki_tree_next(&check,tree,&element);
    if (result != TC_TLV_OK) return result;
  }
  /* Attribute framing, values and ordering comparisons each scan the input. */
  for (unsigned i = 0; i < RDN_SCANS; ++i)
    if (tc_x509_path_charge(tree->work,contents.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  return tc_x509_rdn_contents(&attributes);
}

/* DistributionPointName CHOICE, without the enclosing explicit [0].
 * The relative form borrows RDN contents; it does not append the issuer Name. */
static inline TC_TLV_result tc_pki_distribution_name_read(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    tc_pki_distribution_name* out)
{
  enum { FULL_NAME = 0xa0, RELATIVE_NAME = 0xa1 };
  TC_TLV_element element;
  tc_pki_distribution_name parsed;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_tree_read(encoded,TC_TLV_DER,limits,tree,&element);
  if (result != TC_TLV_OK) return result;
  if (element.encoded.length != encoded.length ||
      (!tc_pki_tag(&element,FULL_NAME) && !tc_pki_tag(&element,RELATIVE_NAME)))
    return TC_TLV_INVALID;
  parsed.encoded = element.encoded;
  parsed.contents = element.value;
  parsed.relative = tc_pki_tag(&element,RELATIVE_NAME);
  if (!parsed.relative) {
    result = tc_pki_general_names_contents_check(parsed.contents,limits,tree);
  } else {
    result = tc_pki_rdn_contents_check(parsed.contents,limits,tree);
  }
  if (result != TC_TLV_OK) return result;
  *out = parsed;
  return TC_TLV_OK;
}
#endif
