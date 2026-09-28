/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_DISTRIBUTION_INTERNAL_H_
#define TC_PKI_DISTRIBUTION_INTERNAL_H_
#include "pki_names_internal.h"
#include "pki_bits_internal.h"

typedef struct {
  tc_pki_distribution_name name;
  TC_bytes issuer;
  uint16_t reasons;
  int has_reasons;
} tc_pki_distribution_point;

/* Nonempty CRLDistributionPoints SEQUENCE. Inputs and scratch are disjoint. */
static inline TC_TLV_result tc_pki_distribution_points_init(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree, TC_TLV_reader* out)
{
  TC_TLV_reader parsed;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_tree_open(encoded,0x30,TC_TLV_DER,limits,tree,&parsed);
  if (result != TC_TLV_OK) return result;
  if (tc_pki_end(&parsed)) return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

/* Reader and output change only on OK. Issuer contains GeneralNames contents.
 * PKIX issuer-DN restrictions and relative-name resolution are separate checks. */
static inline TC_TLV_result tc_pki_distribution_point_next(TC_TLV_reader* reader,
    const tc_pki_tree_workspace* tree, tc_pki_distribution_point* out)
{
  enum { NAME = 0xa0, REASONS = 0x81, ISSUER = 0xa2, FIELD_MASK = 0x1f };
  tc_pki_distribution_point parsed = {0};
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_TLV_result result;
  unsigned previous = 0;
  if (!reader || !out || reader->profile != TC_TLV_DER) return TC_TLV_ARGUMENT;
  next = *reader;
  result = tc_pki_tree_next(&next,tree,&element);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_open(element.encoded,0x30,TC_TLV_DER,&next.limits,tree,&fields);
  if (result != TC_TLV_OK) return result;
  while (!tc_pki_end(&fields)) {
    result = tc_pki_tree_next(&fields,tree,&element);
    if (result != TC_TLV_OK) return result;
    if (element.header.tag_length != 1) return TC_TLV_INVALID;
    const unsigned tag = element.header.tag[0];
    const unsigned field = (tag & FIELD_MASK) + 1;
    if (field <= previous) return TC_TLV_INVALID;
    previous = field;
    switch (tag) {
      case NAME:
        result = tc_pki_distribution_name_read(element.value,&next.limits,tree,&parsed.name);
        break;
      case REASONS:
        result = tc_pki_reason_flags(element.value,&parsed.reasons);
        parsed.has_reasons = 1;
        break;
      case ISSUER:
        result = tc_pki_general_names_contents_check(element.value,&next.limits,tree);
        parsed.issuer = element.value;
        break;
      default: return TC_TLV_INVALID;
    }
    if (result != TC_TLV_OK) return result;
  }
  if (!parsed.name.encoded.length && !parsed.issuer.length) return TC_TLV_INVALID;
  *reader = next;
  *out = parsed;
  return TC_TLV_OK;
}
/* RFC 5280 4.2.1.13 restricts cRLIssuer to the CRL's issuer DN.
 * Returns the encoded Name for exact comparison with the CRL issuer field. */
static inline TC_TLV_result tc_pki_distribution_issuer_name(TC_bytes issuer,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree, TC_bytes* out)
{
  enum { DIRECTORY_NAME = 0xa4 };
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_general_names_contents_check(issuer,limits,tree);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_read(issuer,TC_TLV_DER,limits,tree,&element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element,DIRECTORY_NAME) || element.encoded.length != issuer.length)
    return TC_TLV_INVALID;
  *out = element.value;
  return TC_TLV_OK;
}
#endif
