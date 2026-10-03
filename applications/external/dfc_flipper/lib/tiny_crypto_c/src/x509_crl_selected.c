/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authenticating a selected CRL: signer path, signature, cRLSign usage, and
 * applying the CRL to a certificate (RFC 5280 section 6.3.3). */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_crl_internal.h"
#include "x509_path_internal.h"
#include "pki_status_internal.h"
#include "pki_source_internal.h"
#include "pki_signature_internal.h"
#include "pki_internal.h"
#include "pki_budget_internal.h"
#include "x509_time_internal.h"
#include <string.h>

/* Build the signer's path through restricted, requiring cRLSign, and report
 * trust->anchor_index as the anchor. */
static TC_X509_path_status crl_signer_path(const TC_X509_certificate* signer,
                                           const TC_X509_store_source* restricted,
                                           const tc_x509_crl_trust* trust,
                                           TC_X509_search_report* out)
{
  TC_X509_path_options signer_options = *trust->options;
  TC_X509_search_report found;
  signer_options.key_usage |= TC_KEY_USAGE_CRL_SIGN;
  TC_X509_path_status status =
      tc_x509_path_build_work(signer->encoded, restricted, &signer_options, trust->validation,
                              trust->search, trust->tree->work, &found);
  if (status != TC_X509_PATH_VALID)
    return status;
  found.anchor_index = trust->anchor_index;
  *out = found;
  return TC_X509_PATH_VALID;
}

/* Set *anchored when signer is the selected trust anchor itself: the same
 * subject name and public key, and a certificate that is not self-issued and
 * is valid at the policy time. A pinned issuing CA that signs its own CRLs
 * is such a signer. A self-issued anchor certificate keeps its
 * one-certificate path. tc_x509_crl_signer_check has already required
 * cRLSign (RFC 5280 section 6.3.3 (f), RFC 10007 section 4). Charges the
 * name comparisons and the key bytes. */
static TC_TLV_result crl_signer_is_anchor(const TC_X509_certificate* signer,
                                          const TC_X509_store_source* restricted,
                                          const tc_x509_crl_trust* trust, int* anchored)
{
  TC_X509_store_anchor anchor;
  memset(&anchor, 0, sizeof anchor);
  size_t* work = trust->tree->work;
  TC_TLV_result result = restricted->anchor(restricted->context, 0, work, &anchor);
  if (result != TC_TLV_OK)
    return result;
  *anchored = 0;
  if (!anchor.trust.name.length || anchor.trust.public_key.type != signer->public_key.type)
    return TC_TLV_OK;
  result = tc_pki_work_charge(work, signer->public_key.key.length);
  if (result != TC_TLV_OK || !tc_pki_equal(anchor.trust.public_key.key, signer->public_key.key))
    return result;
  const TC_X509_path_options* options = trust->options;
  int equal = 0, self_issued = 1, current = 0;
  result = TC_X509_name_equal(signer->subject, anchor.trust.name, &options->parsing,
                              &trust->validation->names, work, &equal);
  if (result == TC_TLV_OK && equal)
    result = TC_X509_name_equal(signer->subject, signer->issuer, &options->parsing,
                                &trust->validation->names, work, &self_issued);
  if (result == TC_TLV_OK && equal && !self_issued)
    result = tc_x509_time_window(&options->at, options->clock_skew_seconds, &signer->not_before,
                                 &signer->not_after, &current);
  if (result == TC_TLV_OK)
    *anchored = equal && !self_issued && current;
  return result;
}

TC_X509_path_status tc_x509_crl_signer_validate(const TC_X509_crl* crl,
                                                const TC_X509_certificate* signer,
                                                const tc_x509_crl_trust* trust,
                                                TC_X509_search_report* out)
{
  tc_pki_anchor_source selected;
  TC_X509_store_source restricted;
  TC_X509_search_report found;
  TC_X509_path_status status;
  TC_X509_signature_result signature;
  TC_TLV_result result;
  size_t initial_work;
  if (!crl || !signer || !trust || !trust->options || !trust->tree || !trust->tree->work ||
      !trust->validation || !trust->search || !out)
    return TC_X509_PATH_ERROR;
  size_t* work = trust->tree->work;
  result = tc_pki_source_select_anchor(trust->source, trust->anchor_index, &selected, &restricted);
  if (result != TC_TLV_OK)
    return tc_x509_path_status(result);
  initial_work = *work;
  signature = tc_x509_crl_signer_check(crl, signer, &trust->options->signatures,
                                       &trust->options->parsing, &trust->validation->names, work);
  if (signature != TC_X509_SIGNATURE_VALID)
    return tc_x509_path_status(tc_pki_signature_status(signature));
  status = crl_signer_path(signer, &restricted, trust, &found);
  if (status == TC_X509_PATH_INVALID) {
    /* RFC 5280 section 6.3.3 (f): the CRL issuer path ends at the trust
     * anchor of the target. A signer that is the anchor itself, such as a
     * pinned issuing CA whose own issuer is not held, has an empty path. */
    int anchored = 0;
    result = crl_signer_is_anchor(signer, &restricted, trust, &anchored);
    if (result != TC_TLV_OK)
      return tc_x509_path_status(result);
    if (anchored) {
      memset(&found, 0, sizeof found);
      found.anchor_index = trust->anchor_index;
      status = TC_X509_PATH_VALID;
    }
  }
  if (status != TC_X509_PATH_VALID)
    return status;
  found.validation.work_used = initial_work - *work;
  *out = found;
  return TC_X509_PATH_VALID;
}

TC_X509_signature_result tc_x509_crl_digest_signature(const TC_X509_crl* crl,
                                                      TC_hash_algorithm hash, TC_bytes digest,
                                                      const TC_X509_public_key* key,
                                                      const TC_X509_signature_provider* provider,
                                                      size_t* work)
{
  TC_signature_algorithm algorithm;
  TC_TLV_result result = tc_pki_signature_resolve(&crl->signature_algorithm, key, &algorithm);
  if (result != TC_TLV_OK)
    return tc_pki_signature_error(result);
  if (hash != algorithm.hash)
    return TC_X509_SIGNATURE_INVALID;
  return TC_X509_signature_verify_digest(digest, &algorithm, crl->signature, key, provider, work);
}

static TC_X509_signature_result crl_key_signature(const TC_X509_crl* crl,
                                                  const TC_X509_public_key* key,
                                                  const TC_X509_signature_provider* provider,
                                                  size_t* work)
{
  if (crl->prepared) {
    if (crl->encoded.length || crl->tbs.length || crl->revoked.length)
      return TC_X509_SIGNATURE_ERROR;
    return tc_x509_crl_digest_signature(crl, crl->prepared->hash, crl->prepared->digest, key,
                                        provider, work);
  }
  return TC_X509_signature_verify_message(&crl->tbs, 1, &crl->signature_algorithm, crl->signature,
                                          key, provider, work);
}

TC_X509_signature_result tc_x509_crl_anchor_check(const TC_X509_crl* crl,
                                                  const TC_X509_trust_anchor* anchor,
                                                  const TC_X509_signature_provider* provider,
                                                  const TC_TLV_limits* limits,
                                                  const TC_X509_name_workspace* names, size_t* work)
{
  int matched;
  if (!crl || !anchor || !limits || !names || !work)
    return TC_X509_SIGNATURE_ERROR;
  if (!provider || (crl->prepared ? !provider->verify_digest : !provider->verify))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  TC_TLV_result result =
      TC_X509_name_equal(crl->issuer, anchor->name, limits, names, work, &matched);
  if (result != TC_TLV_OK)
    return tc_pki_signature_error(result);
  if (!matched)
    return TC_X509_SIGNATURE_INVALID;
  return crl_key_signature(crl, &anchor->public_key, provider, work);
}

TC_TLV_result tc_x509_crl_signer_key(const TC_X509_crl* crl, const TC_X509_certificate* signer,
                                     const TC_TLV_limits* limits,
                                     const TC_X509_name_workspace* names, size_t* work,
                                     TC_X509_public_key* key)
{
  TC_TLV_result result;
  int accepted;
  if (!crl || !signer || !limits || !names || !work || !key)
    return TC_TLV_ARGUMENT;
  result = TC_X509_name_equal(crl->issuer, signer->subject, limits, names, work, &accepted);
  if (result != TC_TLV_OK)
    return result;
  if (!accepted)
    return TC_TLV_INVALID;
  result = tc_x509_crl_signer_usage(signer, limits, work, &accepted);
  if (result != TC_TLV_OK)
    return result;
  if (!accepted)
    return TC_TLV_INVALID;
  if (tc_pki_work_charge(work, signer->spki.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  return TC_X509_subject_public_key(signer->spki, key);
}

TC_X509_signature_result tc_x509_crl_signer_check(const TC_X509_crl* crl,
                                                  const TC_X509_certificate* signer,
                                                  const TC_X509_signature_provider* provider,
                                                  const TC_TLV_limits* limits,
                                                  const TC_X509_name_workspace* names, size_t* work)
{
  if (!crl || !signer || !limits || !names || !work)
    return TC_X509_SIGNATURE_ERROR;
  if (!provider || (crl->prepared ? !provider->verify_digest : !provider->verify))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  TC_X509_public_key key;
  TC_TLV_result result = tc_x509_crl_signer_key(crl, signer, limits, names, work, &key);
  if (result != TC_TLV_OK)
    return tc_pki_signature_error(result);
  return crl_key_signature(crl, &key, provider, work);
}

TC_TLV_result tc_x509_crl_signer_usage(const TC_X509_certificate* signer,
                                       const TC_TLV_limits* limits, size_t* work, int* authorized)
{
  TC_X509_extension_summary extensions;
  TC_TLV_result result;
  if (!signer || !limits || !work || !authorized)
    return TC_TLV_ARGUMENT;
  result = tc_x509_extensions_summarize(signer, limits, work, &extensions);
  if (result != TC_TLV_OK)
    return result;
  const int present = tc_x509_summary_has(&extensions, TC_X509_SUMMARY_KEY_USAGE);
  /* RFC 10007 section 4, amending RFC 5280 section 6.3.3 step (f): a v3
   * issuer certificate must carry keyUsage with cRLSign. v1 and v2
   * certificates have no extensions and skip the check. */
  *authorized = (signer->version < 3 && !present) ||
                (present && !!(extensions.key_usage & TC_KEY_USAGE_CRL_SIGN));
  return TC_TLV_OK;
}

/* Reason coverage a selected CRL pair adds for query. */
static TC_TLV_result
crl_selected_coverage(const tc_x509_crl_selected* selected, const tc_x509_crl_query* query,
                      const TC_X509_revocation_time* time, const tc_x509_crl_decode* decode,
                      const TC_X509_crl_evidence* evidence, tc_x509_crl_coverage* coverage)
{
  /* The delta supplies the effective update interval. */
  TC_TLV_result result = tc_x509_crl_coverage_at(
      selected->delta ? selected->delta : selected->base,
      selected->delta ? selected->delta_info : selected->base_info, time, query->point,
      query->certificate->issuer, query->certificate_ca, decode, coverage);
  if (result != TC_TLV_OK)
    return result;
  return coverage->reasons & ~evidence->reasons ? TC_TLV_OK : TC_TLV_END;
}

/* Apply a selected CRL pair's entries for certificate to evidence. */
static TC_TLV_result crl_selected_evidence(const tc_x509_crl_selected* selected,
                                           const TC_X509_certificate* certificate, uint16_t reasons,
                                           const tc_x509_crl_decode* decode,
                                           TC_X509_crl_evidence* evidence)
{
  TC_X509_crl_match match;
  TC_TLV_result result = tc_x509_crl_selected_lookup(selected, certificate, decode, &match);
  if (result != TC_TLV_OK)
    return result;
  return tc_x509_crl_evidence_add(evidence, reasons, &match);
}

TC_TLV_result tc_x509_crl_apply(const tc_x509_crl_selected* selected,
                                const tc_x509_crl_query* query, const TC_X509_revocation_time* time,
                                const tc_x509_crl_decode* decode, TC_X509_crl_evidence* evidence)
{
  if (!decode)
    return TC_TLV_ARGUMENT;
  const TC_TLV_limits* limits = decode->limits;
  const tc_pki_tree_workspace* tree = decode->tree;
  const TC_X509_name_workspace* names = decode->names;
  TC_X509_revocation_status status;
  tc_x509_crl_coverage coverage;
  if (!selected || !selected->base || !selected->base_info || !query || !query->certificate ||
      !query->point || !time || !limits || !tree || !tree->work || !names ||
      (query->certificate_ca != 0 && query->certificate_ca != 1) ||
      !!selected->delta != !!selected->delta_info)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_crl_evidence_status(evidence, &status);
  if (result != TC_TLV_OK)
    return result;
  if (status != TC_X509_REVOCATION_UNDETERMINED)
    return TC_TLV_END;
  result = crl_selected_coverage(selected, query, time, decode, evidence, &coverage);
  if (result != TC_TLV_OK)
    return result;
  return crl_selected_evidence(selected, query->certificate, coverage.reasons, decode, evidence);
}

TC_TLV_result tc_x509_crl_selected_authenticate(const tc_x509_crl_selected* selected,
                                                const TC_X509_certificate* signer,
                                                const TC_X509_signature_provider* provider,
                                                const tc_x509_crl_decode* decode)
{
  if (!decode)
    return TC_TLV_ARGUMENT;
  const TC_TLV_limits* limits = decode->limits;
  const tc_pki_tree_workspace* tree = decode->tree;
  const TC_X509_name_workspace* names = decode->names;
  TC_TLV_result result;
  if (selected->base_info->present & TC_X509_CRL_EXT_DELTA)
    return TC_TLV_INVALID;
  if (selected->delta) {
    int compatible;
    result = tc_x509_crl_delta_compatible(selected->base, selected->base_info, selected->delta,
                                          selected->delta_info, decode, &compatible);
    if (result != TC_TLV_OK)
      return result;
    if (!compatible)
      return TC_TLV_INVALID;
  } else {
    result = tc_x509_crl_extension_policy(selected->base_info);
    if (result != TC_TLV_OK)
      return result;
  }
  /* Matching AKIDs alone do not prove that both signatures use the same key. */
  const TC_X509_crl* crls[] = {selected->base, selected->delta};
  for (size_t i = 0; i < sizeof crls / sizeof crls[0]; ++i) {
    if (!crls[i])
      continue;
    result = tc_pki_signature_status(
        tc_x509_crl_signer_check(crls[i], signer, provider, limits, names, tree->work));
    if (result != TC_TLV_OK)
      return result;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_selected_lookup(const tc_x509_crl_selected* selected,
                                          const TC_X509_certificate* certificate,
                                          const tc_x509_crl_decode* decode, TC_X509_crl_match* out)
{
  TC_X509_crl_match base, delta;
  TC_TLV_result result =
      tc_x509_crl_find(selected->base, selected->base_info, certificate, decode, &base);
  if (result != TC_TLV_OK)
    return result;
  if (selected->delta) {
    result = tc_x509_crl_find(selected->delta, selected->delta_info, certificate, decode, &delta);
    if (result != TC_TLV_OK)
      return result;
  }
  return tc_x509_crl_combine(&base, selected->delta ? &delta : NULL, out);
}
#endif
