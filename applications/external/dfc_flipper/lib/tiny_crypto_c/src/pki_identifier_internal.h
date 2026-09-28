/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_IDENTIFIER_INTERNAL_H_
#define TC_PKI_IDENTIFIER_INTERNAL_H_
#include "pki_extensions_internal.h"
#include "pki_names_internal.h"

/* Candidate selection, not signature or trust validation. Every supplied
 * identifier must match. An absent SKI cannot match a keyIdentifier hint.
 * Issuer alternatives currently support directoryName; unresolved other name
 * forms return UNSUPPORTED. Inputs are parsed, borrowed and disjoint from
 * scratch/output. Output changes only on OK; scratch/work are provisional. */
static inline TC_TLV_result tc_pki_authority_matches(
    const TC_X509_authority_key_identifier* authority, const TC_X509_certificate* candidate,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names, int* matched)
{
  enum { DIRECTORY_NAME = 4 };
  TC_TLV_result result;
  int order;
  if (!authority || !candidate || !limits || !tree || !tree->work || !matched)
    return TC_TLV_ARGUMENT;
  if (authority->issuer.length) {
    result = tc_pki_general_names_contents_check(authority->issuer,limits,tree);
    if (result != TC_TLV_OK) return result;
  }
  if (authority->has_key_identifier) {
    TC_bytes ski;
    result = tc_pki_subject_key_identifier(candidate,limits,tree->work,&ski);
    if (result != TC_TLV_OK) return result;
    if (!ski.data) { *matched = 0; return TC_TLV_OK; }
    result = tc_pki_span_compare(authority->key_identifier,ski,tree->work,&order);
    if (result != TC_TLV_OK) return result;
    if (order) { *matched = 0; return TC_TLV_OK; }
  }
  if (authority->serial.length) {
    TC_TLV_reader reader;
    TC_X509_general_name name;
    int found = 0, unsupported = 0;
    if (!names || !authority->issuer.length) return TC_TLV_ARGUMENT;
    result = tc_pki_span_compare(authority->serial,candidate->serial,tree->work,&order);
    if (result != TC_TLV_OK) return result;
    if (order || authority->serial_negative != candidate->serial_negative) {
      *matched = 0; return TC_TLV_OK;
    }
    if (tc_x509_path_charge(tree->work,authority->issuer.length) != TC_TLV_OK ||
        tc_x509_path_charge(tree->work,authority->issuer.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = TC_TLV_reader_init(&reader,authority->issuer.data,authority->issuer.length,TC_TLV_DER,limits);
    if (result != TC_TLV_OK) return result;
    while (!tc_pki_end(&reader)) {
      int equal;
      if (tc_x509_path_charge(tree->work,1) != TC_TLV_OK) return TC_TLV_LIMIT;
      result = TC_X509_general_name_next(&reader,tree->frames,tree->capacity,&name);
      if (result != TC_TLV_OK) return result;
      if (name.type != DIRECTORY_NAME) { unsupported = 1; continue; }
      /* authorityCertIssuer names the candidate's issuer, not its subject. */
      result = TC_X509_name_equal(name.value,candidate->issuer,limits,names,tree->work,&equal);
      if (result == TC_TLV_UNSUPPORTED) { unsupported = 1; continue; }
      if (result != TC_TLV_OK) return result;
      found |= equal;
    }
    if (!found && unsupported) return TC_TLV_UNSUPPORTED;
    *matched = found;
    return TC_TLV_OK;
  }
  if (authority->issuer.length) return TC_TLV_ARGUMENT;
  *matched = 1;
  return TC_TLV_OK;
}
#endif
