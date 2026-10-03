/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_NAMES_INTERNAL_H_
#define TC_PKI_NAMES_INTERNAL_H_
#include "pki_tree_internal.h"

/* GeneralNames contents, also used by IMPLICIT authorityCertIssuer.
 * Input and writable ranges must be disjoint. */
static inline TC_TLV_result tc_pki_general_names_contents_check(TC_bytes contents,
                                                                const TC_TLV_limits* limits,
                                                                const tc_pki_tree_workspace* tree)
{
  TC_X509_general_names_reader reader;
  TC_X509_general_name name;
  TC_TLV_result result;
  if (!tree || !tree->work)
    return TC_TLV_ARGUMENT;
  if (!contents.length)
    return TC_TLV_INVALID;
  /* The reader scans framing and then the name's value. */
  if (tc_pki_work_charge(tree->work, contents.length) != TC_TLV_OK ||
      tc_pki_work_charge(tree->work, contents.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_X509_general_names_contents_init(&reader, contents, limits,
                                               (TC_TLV_frames){tree->frames, tree->capacity});
  if (result != TC_TLV_OK)
    return result;
  while (!tc_pki_end(&reader.reader)) {
    if (tc_pki_work_charge(tree->work, 1) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    result = TC_X509_general_name_next(&reader, &name);
    if (result != TC_TLV_OK)
      return result;
  }
  return TC_TLV_OK;
}

/* Validate an RDN without its SET wrapper, including nested DER values. */
static inline TC_TLV_result tc_pki_rdn_contents_check(TC_bytes contents,
                                                      const TC_TLV_limits* limits,
                                                      const tc_pki_tree_workspace* tree)
{
  enum { RDN_SCANS = 3 };
  TC_TLV_reader attributes, check;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!tree || !tree->work)
    return TC_TLV_ARGUMENT;
  result = TC_TLV_reader_init(&attributes, contents, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  check = attributes;
  while (!tc_pki_end(&check)) {
    result = tc_pki_tree_next(&check, tree, &element);
    if (result != TC_TLV_OK)
      return result;
  }
  /* Attribute framing, values and ordering comparisons each scan the input. */
  for (unsigned i = 0; i < RDN_SCANS; ++i)
    if (tc_pki_work_charge(tree->work, contents.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
  return tc_x509_rdn_contents(&attributes);
}

/* DistributionPointName CHOICE, without the enclosing explicit [0].
 * The relative form borrows RDN contents. Callers resolve it against the issuer Name. */
static inline TC_TLV_result tc_pki_distribution_name_read(TC_bytes encoded,
                                                          const TC_TLV_limits* limits,
                                                          const tc_pki_tree_workspace* tree,
                                                          TC_X509_distribution_name* out)
{
  enum { FULL_NAME = 0xa0, RELATIVE_NAME = 0xa1 };
  TC_TLV_element element;
  TC_X509_distribution_name parsed;
  TC_TLV_result result;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = tc_pki_tree_read(encoded, TC_TLV_DER, limits, tree, &element);
  if (result != TC_TLV_OK)
    return result;
  if (element.encoded.length != encoded.length ||
      (!tc_pki_tag(&element, FULL_NAME) && !tc_pki_tag(&element, RELATIVE_NAME)))
    return TC_TLV_INVALID;
  parsed.encoded = element.encoded;
  parsed.contents = element.value;
  parsed.relative = tc_pki_tag(&element, RELATIVE_NAME);
  if (!parsed.relative) {
    result = tc_pki_general_names_contents_check(parsed.contents, limits, tree);
  } else {
    result = tc_pki_rdn_contents_check(parsed.contents, limits, tree);
  }
  if (result != TC_TLV_OK)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}
/* Charge two passes over a Name, check that it is a SEQUENCE of RDNs and
 * open reader at its first RDN. The second pass pays for the RDN matching
 * that follows. reader changes only on OK. */
TC_TLV_result tc_x509_name_validate(TC_bytes input, const TC_TLV_limits* limits, size_t* work,
                                    TC_TLV_reader* reader);
/* Read the next AttributeTypeAndValue from an RDN reader. With tree set, the
 * BER-tolerant tree walker reads it and charges tree work. Without tree, each
 * DER element costs one unit plus its encoded length from work. */
TC_TLV_result tc_x509_name_next_attribute(TC_TLV_reader* reader, size_t* work,
                                          TC_X509_name_attribute* attribute,
                                          const tc_pki_tree_workspace* tree);
#endif
