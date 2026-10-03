/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Revocation evidence: combining base and delta results, reason coverage and
 * delta-CRL compatibility. */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_crl_internal.h"
#include "pki_extensions_internal.h"

static int crl_effective_match_valid(const TC_X509_crl_match* match)
{
  return (match->found == 0 || match->found == 1) &&
         (match->has_invalidity_date == 0 || match->has_invalidity_date == 1) &&
         (!match->found ||
          (tc_pki_crl_reason_known(match->reason) && match->reason != TC_PKI_CRL_REASON_REMOVE));
}

TC_TLV_result tc_x509_crl_evidence_status(const TC_X509_crl_evidence* evidence,
                                          TC_X509_revocation_status* out)
{
  if (!evidence || !out || (evidence->reasons & ~TC_X509_CRL_ALL_REASONS) ||
      !crl_effective_match_valid(&evidence->revocation) ||
      (!evidence->reasons && evidence->revocation.found))
    return TC_TLV_ARGUMENT;
  *out = evidence->revocation.found                     ? TC_X509_REVOCATION_REVOKED
         : evidence->reasons == TC_X509_CRL_ALL_REASONS ? TC_X509_REVOCATION_GOOD
                                                        : TC_X509_REVOCATION_UNDETERMINED;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_evidence_equal(const TC_X509_crl_evidence* left,
                                         const TC_X509_crl_evidence* right, int* equal)
{
  TC_X509_revocation_status a, b;
  int order;
  if (!equal)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_crl_evidence_status(left, &a);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_evidence_status(right, &b);
  if (result != TC_TLV_OK)
    return result;
  if (a != b || left->reasons != right->reasons) {
    *equal = 0;
    return TC_TLV_OK;
  }
  if (a == TC_X509_REVOCATION_REVOKED) {
    const TC_X509_crl_match* x = &left->revocation;
    const TC_X509_crl_match* y = &right->revocation;
    if (x->reason != y->reason || x->has_invalidity_date != y->has_invalidity_date) {
      *equal = 0;
      return TC_TLV_OK;
    }
    result = TC_X509_time_compare(&x->revoked_at, &y->revoked_at, &order);
    if (result != TC_TLV_OK)
      return result;
    if (order) {
      *equal = 0;
      return TC_TLV_OK;
    }
    if (x->has_invalidity_date) {
      result = TC_X509_time_compare(&x->invalidity_date, &y->invalidity_date, &order);
      if (result != TC_TLV_OK)
        return result;
      if (order) {
        *equal = 0;
        return TC_TLV_OK;
      }
    }
  }
  *equal = 1;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_evidence_add(TC_X509_crl_evidence* evidence, uint16_t reasons,
                                       const TC_X509_crl_match* match)
{
  TC_X509_revocation_status status;
  TC_TLV_result result;
  if (!match || (reasons & ~TC_X509_CRL_ALL_REASONS))
    return TC_TLV_ARGUMENT;
  if (!crl_effective_match_valid(match))
    return TC_TLV_INVALID;
  result = tc_x509_crl_evidence_status(evidence, &status);
  if (result != TC_TLV_OK)
    return result;
  if (status != TC_X509_REVOCATION_UNDETERMINED)
    return TC_TLV_END;
  if (!(reasons & ~evidence->reasons))
    return TC_TLV_OK;
  if (match->found)
    evidence->revocation = *match;
  evidence->reasons |= reasons;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_combine(const TC_X509_crl_match* base, const TC_X509_crl_match* delta,
                                  TC_X509_crl_match* out)
{
  TC_X509_crl_match result;
  if (!base || !out || (base->found != 0 && base->found != 1) ||
      (delta && delta->found != 0 && delta->found != 1))
    return TC_TLV_ARGUMENT;
  if (base->found &&
      (!tc_pki_crl_reason_known(base->reason) || base->reason == TC_PKI_CRL_REASON_REMOVE))
    return TC_TLV_INVALID;
  if (delta && delta->found && !tc_pki_crl_reason_known(delta->reason))
    return TC_TLV_INVALID;
  result = delta && delta->found ? *delta : *base;
  if (!result.found || result.reason == TC_PKI_CRL_REASON_REMOVE)
    result = (TC_X509_crl_match){0};
  *out = result;
  return TC_TLV_OK;
}

/* CRL number contents are already validated nonnegative DER INTEGERs. */
TC_TLV_result tc_x509_crl_number_compare(TC_bytes left, TC_bytes right, size_t* work, int* order)
{
  if (!left.data || !right.data || !left.length || !right.length || !work || !order)
    return TC_TLV_ARGUMENT;
  if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  if (left.length > 1 && !left.data[0]) {
    ++left.data;
    --left.length;
  }
  if (right.length > 1 && !right.data[0]) {
    ++right.data;
    --right.length;
  }
  if (left.length != right.length) {
    *order = left.length < right.length ? -1 : 1;
    return TC_TLV_OK;
  }
  return tc_pki_span_compare(left, right, work, order);
}

TC_TLV_result
tc_x509_crl_scope_equal(const TC_X509_crl* left, const TC_X509_crl_extensions* left_info,
                        const TC_X509_crl* right, const TC_X509_crl_extensions* right_info,
                        const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
                        const TC_X509_name_workspace* names, int* equal)
{
  int matched, order;
  if (!left || !left_info || !right || !right_info || !limits || !tree || !tree->work || !names ||
      !equal)
    return TC_TLV_ARGUMENT;
  if ((left_info->present ^ right_info->present) & TC_X509_CRL_EXT_DISTRIBUTION) {
    *equal = 0;
    return TC_TLV_OK;
  }
  /* Distribution bytes differ cheaply for unrelated scopes. */
  TC_TLV_result result = tc_pki_span_compare(left_info->distribution_encoded,
                                             right_info->distribution_encoded, tree->work, &order);
  if (result != TC_TLV_OK)
    return result;
  if (order) {
    *equal = 0;
    return TC_TLV_OK;
  }
  result = TC_X509_name_equal(left->issuer, right->issuer, limits, names, tree->work, &matched);
  if (result != TC_TLV_OK)
    return result;
  *equal = matched;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_delta_compatible(const TC_X509_crl* base,
                                           const TC_X509_crl_extensions* base_info,
                                           const TC_X509_crl* delta,
                                           const TC_X509_crl_extensions* delta_info,
                                           const tc_x509_crl_decode* decode, int* compatible)
{
  if (!decode)
    return TC_TLV_ARGUMENT;
  const TC_TLV_limits* limits = decode->limits;
  const tc_pki_tree_workspace* tree = decode->tree;
  const TC_X509_name_workspace* names = decode->names;
  const unsigned shared = TC_X509_CRL_EXT_DISTRIBUTION | TC_X509_CRL_EXT_AUTHORITY;
  const unsigned delta_required = TC_X509_CRL_EXT_NUMBER | TC_X509_CRL_EXT_DELTA;
  TC_TLV_result result;
  int order, equal;
  if (!base || !delta || !base_info || !delta_info || !tree || !tree->work || !compatible)
    return TC_TLV_ARGUMENT;
  result = tc_x509_crl_extension_policy(base_info);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_extension_policy(delta_info);
  if (result != TC_TLV_OK)
    return result;
  if ((base_info->present & TC_X509_CRL_EXT_DELTA) ||
      !(base_info->present & TC_X509_CRL_EXT_NUMBER) ||
      (delta_info->present & delta_required) != delta_required ||
      (base_info->present & shared) != (delta_info->present & shared)) {
    *compatible = 0;
    return TC_TLV_OK;
  }
  result =
      tc_x509_crl_number_compare(base_info->number, delta_info->base_number, tree->work, &order);
  if (result != TC_TLV_OK)
    return result;
  if (order < 0) {
    *compatible = 0;
    return TC_TLV_OK;
  }
  result = tc_x509_crl_number_compare(base_info->number, delta_info->number, tree->work, &order);
  if (result != TC_TLV_OK)
    return result;
  if (order >= 0) {
    *compatible = 0;
    return TC_TLV_OK;
  }
  result = tc_x509_crl_scope_equal(base, base_info, delta, delta_info, limits, tree, names, &equal);
  if (result != TC_TLV_OK)
    return result;
  if (!equal) {
    *compatible = 0;
    return TC_TLV_OK;
  }
  if (base_info->authority.has_key_identifier != delta_info->authority.has_key_identifier) {
    *compatible = 0;
    return TC_TLV_OK;
  }
  const TC_bytes left[] = {base_info->authority.key_identifier, base_info->authority.issuer,
                           base_info->authority.serial};
  const TC_bytes right[] = {delta_info->authority.key_identifier, delta_info->authority.issuer,
                            delta_info->authority.serial};
  for (size_t i = 0; i < sizeof left / sizeof left[0]; ++i) {
    result = tc_pki_span_compare(left[i], right[i], tree->work, &order);
    if (result != TC_TLV_OK)
      return result;
    if (order) {
      *compatible = 0;
      return TC_TLV_OK;
    }
  }
  *compatible = 1;
  return TC_TLV_OK;
}
#endif
