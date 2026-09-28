/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_crl_internal.h"
#include "x509_time_internal.h"
#include "pki_extensions_internal.h"
#include "pki_names_internal.h"
#include "pki_bits_internal.h"
#include "pki_distribution_internal.h"
#include "pki_status_internal.h"
#include "pki_source_internal.h"
#include "pki_reader_internal.h"
#include "x509_crl_source_internal.h"
#include "pki_signature_internal.h"
#include "hash_dispatch_internal.h"

TC_TLV_result tc_x509_crl_content_equal(const tc_x509_crl* left,
    const tc_x509_crl* right, size_t* work, int* equal)
{
  if (!left || !right || !work || !equal) return TC_TLV_ARGUMENT;
  int order;
  TC_TLV_result result;
  if (!left->prepared && !right->prepared) {
    result = tc_pki_span_compare(left->tbs,right->tbs,work,&order);
  } else {
    if (!left->prepared) { const tc_x509_crl* swap = left; left = right; right = swap; }
    const TC_X509_crl_prepared* prepared = left->prepared;
    tc_hash_info info;
    if (!tc_hash_info_get(prepared->hash,&info)) return TC_TLV_UNSUPPORTED;
    if (!prepared->digest.data || prepared->digest.length != info.digest_length)
      return TC_TLV_ARGUMENT;
    TC_bytes digest;
    enum { MAX_DIGEST_BYTES = 64 };
    uint8_t computed[MAX_DIGEST_BYTES];
    if (right->prepared) {
      /* Different retained hashes cannot establish content equality. */
      if (right->prepared->hash != prepared->hash) return TC_TLV_UNSUPPORTED;
      digest = right->prepared->digest;
      if (!digest.data || digest.length != info.digest_length) return TC_TLV_ARGUMENT;
    } else {
      if (!tc_hash_available(prepared->hash)) return TC_TLV_UNSUPPORTED;
      result = tc_x509_path_charge(work,right->tbs.length);
      if (result != TC_TLV_OK) return result;
      tc_hash_workspace hash;
      if (tc_hash_digest_parts(prepared->hash,&right->tbs,1,computed,&hash) != TC_OK)
        return TC_TLV_ARGUMENT;
      digest = (TC_bytes){computed,info.digest_length};
    }
    result = tc_pki_span_compare(prepared->digest,digest,work,&order);
  }
  if (result == TC_TLV_OK) *equal = order == 0;
  return result;
}

TC_TLV_result TC_X509_crl_read(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_X509_crl* out)
{
  TC_TLV_result result = tc_pki_reader_storage(encoded,limits,frames,frame_capacity,work,out,sizeof *out);
  if (result != TC_TLV_OK) return result;
  const tc_pki_tree_workspace tree = {frames,frame_capacity,work};
  return tc_x509_crl_read(encoded,limits,&tree,out);
}

TC_TLV_result TC_X509_crl_extensions_read(TC_bytes encoded, const TC_TLV_limits* limits,
    const TC_X509_workspace* workspace, size_t* work, TC_X509_crl_extensions* out)
{
  TC_TLV_result result = tc_pki_reader_workspace_storage(encoded,limits,workspace,work,out,sizeof *out);
  if (result != TC_TLV_OK) return result;
  const tc_pki_tree_workspace tree = {workspace->frames,workspace->frame_capacity,work};
  return tc_x509_crl_extension_info_read(encoded,limits,&tree,workspace->extension_oids,workspace->extension_capacity,out);
}

TC_TLV_result tc_x509_crl_record_read(TC_bytes encoded, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, TC_bytes* oids, size_t oid_capacity,
    TC_X509_crl_record* record)
{
  if (!record) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_crl_read(encoded,limits,tree,&record->crl);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_extension_info_read(record->crl.extensions,limits,tree,
      oids,oid_capacity,&record->extensions);
  if (result != TC_TLV_OK) return result;
  record->policy = tc_x509_crl_extension_policy(&record->extensions);
  return record->policy == TC_TLV_OK || record->policy == TC_TLV_INVALID ||
      record->policy == TC_TLV_UNSUPPORTED ? TC_TLV_OK : record->policy;
}

TC_TLV_result TC_X509_crl_index_init(const TC_bytes* encoded, size_t count,
    const TC_TLV_limits* limits, const TC_X509_workspace* workspace, size_t* work,
    TC_X509_crl_record* records, size_t capacity, TC_X509_crl_index* out)
{
  enum { WRITE_COUNT = 5, INPUT_COUNT = 3 };
  TC_bytes writes[WRITE_COUNT], inputs[INPUT_COUNT];
  if (!limits || !workspace || !work || !out ||
      tc_pki_storage_span(workspace->frames,workspace->frame_capacity,sizeof *workspace->frames,&writes[0]) != TC_TLV_OK ||
      tc_pki_storage_span(workspace->extension_oids,workspace->extension_capacity,sizeof *workspace->extension_oids,&writes[1]) != TC_TLV_OK ||
      tc_pki_storage_span(work,1,sizeof *work,&writes[2]) != TC_TLV_OK ||
      tc_pki_storage_span(records,capacity,sizeof *records,&writes[3]) != TC_TLV_OK ||
      tc_pki_storage_span(out,1,sizeof *out,&writes[4]) != TC_TLV_OK ||
      tc_pki_storage_span(encoded,count,sizeof *encoded,&inputs[0]) != TC_TLV_OK ||
      tc_pki_storage_span(limits,1,sizeof *limits,&inputs[1]) != TC_TLV_OK ||
      tc_pki_storage_span(workspace,1,sizeof *workspace,&inputs[2]) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  size_t budget = *work;
  TC_TLV_result result;
  for (size_t i = 0; i < WRITE_COUNT; ++i) {
    result = tc_pki_storage_input(writes,i,writes[i],&budget);
    if (result != TC_TLV_OK) return result;
  }
  for (size_t i = 0; i < INPUT_COUNT; ++i) {
    result = tc_pki_storage_input(writes,WRITE_COUNT,inputs[i],&budget);
    if (result != TC_TLV_OK) return result;
  }
  /* Check later records before any earlier record can overwrite them. */
  for (size_t i = 0; i < count; ++i) {
    result = tc_pki_storage_input(writes,WRITE_COUNT,encoded[i],&budget);
    if (result != TC_TLV_OK) return result;
  }
  *work = budget;
  if (count > capacity) return TC_TLV_LIMIT;
  const tc_pki_tree_workspace tree = {workspace->frames,workspace->frame_capacity,work};
  for (size_t i = 0; i < count; ++i) {
    result = tc_x509_crl_record_read(encoded[i],limits,&tree,
        workspace->extension_oids,workspace->extension_capacity,&records[i]);
    if (result != TC_TLV_OK) return result;
  }
  *out = (TC_X509_crl_index){records,count,0};
  return TC_TLV_OK;
}

static TC_TLV_result crl_selected_authenticate(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names);
static TC_TLV_result crl_selected_lookup(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* certificate, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t capacity, tc_x509_crl_match* out);

static TC_X509_path_status crl_signer_path(const TC_X509_certificate* signer,
    const TC_X509_store_source* restricted, size_t anchor_index,
    const TC_X509_path_options* options, const TC_X509_path_workspace* validation,
    const TC_X509_search_workspace* search, size_t* work, TC_X509_search_result* out)
{
  TC_X509_path_options signer_options = *options;
  TC_X509_search_result found;
  signer_options.key_usage |= TC_KEY_USAGE_CRL_SIGN;
  TC_X509_path_status status = tc_x509_path_build_work(signer->encoded,restricted,
      &signer_options,validation,search,work,&found);
  if (status != TC_X509_PATH_VALID) return status;
  found.anchor_index = anchor_index;
  *out = found;
  return TC_X509_PATH_VALID;
}

static TC_TLV_result crl_selected_path(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const TC_X509_store_source* restricted,
    size_t anchor_index, const TC_X509_path_options* options,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    size_t* work, TC_X509_search_result* out)
{
  const tc_pki_tree_workspace tree = {validation->frames,validation->frame_capacity,work};
  TC_TLV_result result = crl_selected_authenticate(selected,signer,&options->signatures,
      &options->parsing,&tree,&validation->names);
  if (result != TC_TLV_OK) return result;
  return tc_x509_path_result_status(crl_signer_path(signer,restricted,anchor_index,
      options,validation,search,work,out));
}

TC_TLV_result tc_x509_crl_selected_validate(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const TC_X509_store_source* source, size_t anchor_index,
    const TC_X509_path_options* options, const TC_X509_path_workspace* validation,
    const TC_X509_search_workspace* search, size_t* work, TC_X509_search_result* out)
{
  tc_pki_anchor_source anchor;
  TC_X509_store_source restricted;
  TC_X509_search_result found;
  if (!selected || !selected->base || !selected->base_info || !signer || !options ||
      !validation || !search || !work || !out ||
      !!selected->delta != !!selected->delta_info) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_source_select_anchor(source,anchor_index,&anchor,&restricted);
  if (result != TC_TLV_OK) return result;
  const size_t initial_work = *work;
  result = crl_selected_path(selected,signer,&restricted,anchor_index,options,
      validation,search,work,&found);
  if (result != TC_TLV_OK) return result;
  found.validation.work_used = initial_work - *work;
  *out = found;
  return TC_TLV_OK;
}

TC_X509_path_status tc_x509_crl_signer_validate(const tc_x509_crl* crl,
    const TC_X509_certificate* signer, const TC_X509_store_source* source, size_t anchor_index,
    const TC_X509_path_options* options, const TC_X509_path_workspace* validation,
    const TC_X509_search_workspace* search, size_t* work, TC_X509_search_result* out)
{
  tc_pki_anchor_source selected;
  TC_X509_store_source restricted;
  TC_X509_search_result found;
  TC_X509_path_status status;
  TC_X509_signature_result signature;
  TC_TLV_result result;
  size_t initial_work;
  if (!crl || !signer || !options || !validation || !search || !work || !out)
    return TC_X509_PATH_ERROR;
  result = tc_pki_source_select_anchor(source,anchor_index,&selected,&restricted);
  if (result != TC_TLV_OK) return tc_x509_path_status(result);
  initial_work = *work;
  signature = tc_x509_crl_signer_check(crl,signer,&options->signatures,
      &options->parsing,&validation->names,work);
  if (signature != TC_X509_SIGNATURE_VALID)
    return tc_x509_path_status(tc_pki_signature_status(signature));
  status = crl_signer_path(signer,&restricted,anchor_index,options,validation,search,work,&found);
  if (status != TC_X509_PATH_VALID) return status;
  found.validation.work_used = initial_work - *work;
  *out = found;
  return TC_X509_PATH_VALID;
}

static TC_X509_signature_result crl_digest_signature(const tc_x509_crl* crl,
    TC_hash_algorithm hash, TC_bytes digest, const TC_X509_public_key* key,
    const TC_X509_signature_provider* provider, size_t* work)
{
  TC_signature_algorithm algorithm;
  TC_TLV_result result = tc_pki_signature_resolve(&crl->signature_algorithm,key,&algorithm);
  if (result != TC_TLV_OK) return tc_pki_signature_error(result);
  if (hash != algorithm.hash) return TC_X509_SIGNATURE_INVALID;
  return TC_X509_signature_verify_digest(digest,&algorithm,crl->signature,key,provider,work);
}

static TC_X509_signature_result crl_key_signature(const tc_x509_crl* crl,
    const TC_X509_public_key* key, const TC_X509_signature_provider* provider, size_t* work)
{
  if (crl->prepared) {
    if (crl->encoded.length || crl->tbs.length || crl->revoked.length)
      return TC_X509_SIGNATURE_ERROR;
    return crl_digest_signature(crl,crl->prepared->hash,crl->prepared->digest,key,provider,work);
  }
  return TC_X509_signature_verify_message(&crl->tbs,1,&crl->signature_algorithm,
    crl->signature,key,provider,work);
}

TC_X509_signature_result tc_x509_crl_anchor_check(const tc_x509_crl* crl,
    const TC_X509_trust_anchor* anchor, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names, size_t* work)
{
  int matched;
  if (!crl || !anchor || !limits || !names || !work) return TC_X509_SIGNATURE_ERROR;
  if (!provider || (crl->prepared ? !provider->verify_digest : !provider->verify))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  TC_TLV_result result = TC_X509_name_equal(crl->issuer,anchor->name,limits,names,work,&matched);
  if (result != TC_TLV_OK) return tc_pki_signature_error(result);
  if (!matched) return TC_X509_SIGNATURE_INVALID;
  return crl_key_signature(crl,&anchor->public_key,provider,work);
}

static TC_TLV_result crl_signer_key(const tc_x509_crl* crl,
    const TC_X509_certificate* signer, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* names, size_t* work, TC_X509_public_key* key)
{
  TC_TLV_result result;
  int accepted;
  if (!crl || !signer || !limits || !names || !work || !key) return TC_TLV_ARGUMENT;
  result = TC_X509_name_equal(crl->issuer,signer->subject,limits,names,work,&accepted);
  if (result != TC_TLV_OK) return result;
  if (!accepted) return TC_TLV_INVALID;
  result = tc_x509_crl_signer_usage(signer,limits,work,&accepted);
  if (result != TC_TLV_OK) return result;
  if (!accepted) return TC_TLV_INVALID;
  if (tc_x509_path_charge(work,signer->spki.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  return TC_X509_subject_public_key(signer->spki.data,signer->spki.length,key);
}

TC_X509_signature_result tc_x509_crl_signer_check(const tc_x509_crl* crl,
    const TC_X509_certificate* signer, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names, size_t* work)
{
  if (!crl || !signer || !limits || !names || !work) return TC_X509_SIGNATURE_ERROR;
  if (!provider || (crl->prepared ? !provider->verify_digest : !provider->verify))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  TC_X509_public_key key;
  TC_TLV_result result = crl_signer_key(crl,signer,limits,names,work,&key);
  if (result != TC_TLV_OK) return tc_pki_signature_error(result);
  return crl_key_signature(crl,&key,provider,work);
}

TC_X509_signature_result tc_x509_crl_signer_digest_check(const tc_x509_crl* crl,
    TC_hash_algorithm hash, TC_bytes digest, const TC_X509_certificate* signer,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* names, size_t* work)
{
  if (!crl || !signer || !limits || !names || !work) return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify_digest) return TC_X509_SIGNATURE_UNSUPPORTED;
  TC_X509_public_key key;
  TC_TLV_result result = crl_signer_key(crl,signer,limits,names,work,&key);
  if (result != TC_TLV_OK) return tc_pki_signature_error(result);
  return crl_digest_signature(crl,hash,digest,&key,provider,work);
}

TC_TLV_result tc_x509_crl_signer_usage(const TC_X509_certificate* signer,
    const TC_TLV_limits* limits, size_t* work, int* authorized)
{
  enum { KEY_USAGE = 15 };
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result result;
  uint16_t usage = 0;
  int present = 0;
  if (!signer || !limits || !work || !authorized) return TC_TLV_ARGUMENT;
  result = tc_pki_extensions_init(&reader,signer,limits,work);
  if (result != TC_TLV_OK) return result;
  while ((result = tc_pki_extension_next(&reader,work,&extension)) == TC_TLV_OK) {
    if (tc_pki_extension_id(&extension) != KEY_USAGE) continue;
    result = tc_pki_key_usage_value(extension.value,&present,&usage);
    if (result != TC_TLV_OK) return result;
  }
  if (result != TC_TLV_END) return result;
  *authorized = !present || !!(usage & TC_KEY_USAGE_CRL_SIGN);
  return TC_TLV_OK;
}

enum { CRL_REASON_UNUSED = 7, CRL_REASON_REMOVE = 8, CRL_REASON_LAST = 10 };
static int crl_reason_known(unsigned reason)
{ return reason <= CRL_REASON_LAST && reason != CRL_REASON_UNUSED; }

static int crl_effective_match_valid(const tc_x509_crl_match* match)
{
  return (match->found == 0 || match->found == 1) &&
      (match->has_invalidity_date == 0 || match->has_invalidity_date == 1) &&
      (!match->found || (crl_reason_known(match->reason) && match->reason != CRL_REASON_REMOVE));
}

TC_TLV_result tc_x509_crl_evidence_status(const tc_x509_crl_evidence* evidence,
    tc_x509_crl_status* out)
{
  if (!evidence || !out || (evidence->reasons & ~TC_X509_CRL_ALL_REASONS) ||
      !crl_effective_match_valid(&evidence->revocation) ||
      (!evidence->reasons && evidence->revocation.found)) return TC_TLV_ARGUMENT;
  *out = evidence->revocation.found ? TC_X509_CRL_REVOKED :
      evidence->reasons == TC_X509_CRL_ALL_REASONS ? TC_X509_CRL_UNREVOKED : TC_X509_CRL_UNDETERMINED;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_evidence_equal(const tc_x509_crl_evidence* left,
    const tc_x509_crl_evidence* right, int* equal)
{
  tc_x509_crl_status a, b;
  int order;
  if (!equal) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_crl_evidence_status(left,&a);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_evidence_status(right,&b);
  if (result != TC_TLV_OK) return result;
  if (a != b || left->reasons != right->reasons) { *equal = 0; return TC_TLV_OK; }
  if (a == TC_X509_CRL_REVOKED) {
    const tc_x509_crl_match* x = &left->revocation;
    const tc_x509_crl_match* y = &right->revocation;
    if (x->reason != y->reason || x->has_invalidity_date != y->has_invalidity_date) {
      *equal = 0; return TC_TLV_OK;
    }
    result = TC_X509_time_compare(&x->revoked_at,&y->revoked_at,&order);
    if (result != TC_TLV_OK) return result;
    if (order) { *equal = 0; return TC_TLV_OK; }
    if (x->has_invalidity_date) {
      result = TC_X509_time_compare(&x->invalidity_date,&y->invalidity_date,&order);
      if (result != TC_TLV_OK) return result;
      if (order) { *equal = 0; return TC_TLV_OK; }
    }
  }
  *equal = 1;
  return TC_TLV_OK;
}

static TC_TLV_result crl_selected_coverage(const tc_x509_crl_selected* selected,
    const tc_x509_crl_query* query, const TC_X509_time* at, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    const tc_x509_crl_evidence* evidence, tc_x509_crl_coverage* coverage)
{
  /* The delta supplies the effective update interval. */
  TC_TLV_result result = tc_x509_crl_coverage_at(selected->delta ? selected->delta : selected->base,
      selected->delta ? selected->delta_info : selected->base_info,at,
      query->point,query->certificate->issuer,query->certificate_ca,limits,tree,names,coverage);
  if (result != TC_TLV_OK) return result;
  return coverage->reasons & ~evidence->reasons ? TC_TLV_OK : TC_TLV_END;
}

static TC_TLV_result crl_selected_evidence(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* certificate, uint16_t reasons, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t oid_capacity, tc_x509_crl_evidence* evidence)
{
  tc_x509_crl_match match;
  TC_TLV_result result = crl_selected_lookup(selected,certificate,limits,tree,
      names,oids,oid_capacity,&match);
  if (result != TC_TLV_OK) return result;
  return tc_x509_crl_evidence_add(evidence,reasons,&match);
}

TC_TLV_result tc_x509_crl_apply(const tc_x509_crl_selected* selected,
    const tc_x509_crl_query* query, const TC_X509_time* at, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t oid_capacity, tc_x509_crl_evidence* evidence)
{
  tc_x509_crl_status status;
  tc_x509_crl_coverage coverage;
  if (!selected || !selected->base || !selected->base_info || !query ||
      !query->certificate || !query->point || !at || !limits || !tree || !tree->work || !names ||
      (query->certificate_ca != 0 && query->certificate_ca != 1) ||
      !!selected->delta != !!selected->delta_info) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_crl_evidence_status(evidence,&status);
  if (result != TC_TLV_OK) return result;
  if (status != TC_X509_CRL_UNDETERMINED) return TC_TLV_END;
  result = crl_selected_coverage(selected,query,at,limits,tree,names,evidence,&coverage);
  if (result != TC_TLV_OK) return result;
  return crl_selected_evidence(selected,query->certificate,coverage.reasons,
      limits,tree,names,oids,oid_capacity,evidence);
}

TC_TLV_result tc_x509_crl_process(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const tc_x509_crl_query* query,
    const TC_X509_store_source* source, size_t anchor_index,
    const TC_X509_path_options* options, const TC_X509_path_workspace* validation,
    const TC_X509_search_workspace* search, size_t* work,
    tc_x509_crl_evidence* evidence, TC_X509_search_result* out)
{
  tc_pki_anchor_source anchor;
  TC_X509_store_source restricted;
  TC_X509_search_result found;
  tc_x509_crl_coverage coverage;
  tc_x509_crl_status status;
  TC_TLV_result result;
  if (!selected || !selected->base || !selected->base_info || !signer || !query ||
      !query->certificate || !query->point || !options || !validation || !search || !work || !out ||
      (query->certificate_ca != 0 && query->certificate_ca != 1) ||
      !!selected->delta != !!selected->delta_info) return TC_TLV_ARGUMENT;
  result = tc_x509_crl_evidence_status(evidence,&status);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_source_select_anchor(source,anchor_index,&anchor,&restricted);
  if (result != TC_TLV_OK) return result;
  if (status != TC_X509_CRL_UNDETERMINED) return TC_TLV_END;
  const size_t initial_work = *work;
  const tc_pki_tree_workspace tree = {validation->frames,validation->frame_capacity,work};
  result = crl_selected_coverage(selected,query,&options->at,&options->parsing,
      &tree,&validation->names,evidence,&coverage);
  if (result != TC_TLV_OK) return result;
  result = crl_selected_path(selected,signer,&restricted,anchor_index,options,
      validation,search,work,&found);
  if (result != TC_TLV_OK) return result;
  /* Entry scans can be large; defer them until a signer path succeeds. */
  result = crl_selected_evidence(selected,query->certificate,coverage.reasons,
      &options->parsing,&tree,&validation->names,validation->oids,validation->oid_capacity,evidence);
  if (result != TC_TLV_OK) return result;
  found.validation.work_used = initial_work - *work;
  *out = found;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_evidence_add(tc_x509_crl_evidence* evidence,
    uint16_t reasons, const tc_x509_crl_match* match)
{
  tc_x509_crl_status status;
  TC_TLV_result result;
  if (!match || (reasons & ~TC_X509_CRL_ALL_REASONS)) return TC_TLV_ARGUMENT;
  if (!crl_effective_match_valid(match)) return TC_TLV_INVALID;
  result = tc_x509_crl_evidence_status(evidence,&status);
  if (result != TC_TLV_OK) return result;
  if (status != TC_X509_CRL_UNDETERMINED) return TC_TLV_END;
  if (!(reasons & ~evidence->reasons)) return TC_TLV_OK;
  if (match->found) evidence->revocation = *match;
  evidence->reasons |= reasons;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_combine(const tc_x509_crl_match* base,
    const tc_x509_crl_match* delta, tc_x509_crl_match* out)
{
  tc_x509_crl_match result;
  if (!base || !out || (base->found != 0 && base->found != 1) ||
      (delta && delta->found != 0 && delta->found != 1)) return TC_TLV_ARGUMENT;
  if (base->found && (!crl_reason_known(base->reason) || base->reason == CRL_REASON_REMOVE))
    return TC_TLV_INVALID;
  if (delta && delta->found && !crl_reason_known(delta->reason)) return TC_TLV_INVALID;
  result = delta && delta->found ? *delta : *base;
  if (!result.found || result.reason == CRL_REASON_REMOVE) result = (tc_x509_crl_match){0};
  *out = result;
  return TC_TLV_OK;
}

/* CRL number contents are already validated nonnegative DER INTEGERs. */
TC_TLV_result tc_x509_crl_number_compare(TC_bytes left, TC_bytes right,
    size_t* work, int* order)
{
  if (!left.data || !right.data || !left.length || !right.length || !work || !order)
    return TC_TLV_ARGUMENT;
  if (tc_x509_path_charge(work,1) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (left.length > 1 && !left.data[0]) { ++left.data; --left.length; }
  if (right.length > 1 && !right.data[0]) { ++right.data; --right.length; }
  if (left.length != right.length) {
    *order = left.length < right.length ? -1 : 1;
    return TC_TLV_OK;
  }
  return tc_pki_span_compare(left,right,work,order);
}

TC_TLV_result tc_x509_crl_scope_equal(const tc_x509_crl* left,
    const tc_x509_crl_extension_info* left_info, const tc_x509_crl* right,
    const tc_x509_crl_extension_info* right_info, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* equal)
{
  int matched, order;
  if (!left || !left_info || !right || !right_info || !limits || !tree || !tree->work || !names || !equal)
    return TC_TLV_ARGUMENT;
  if ((left_info->present ^ right_info->present) & TC_CRL_EXT_DISTRIBUTION) {
    *equal = 0; return TC_TLV_OK;
  }
  TC_TLV_result result = TC_X509_name_equal(left->issuer,right->issuer,limits,names,tree->work,&matched);
  if (result != TC_TLV_OK) return result;
  if (!matched) { *equal = 0; return TC_TLV_OK; }
  result = tc_pki_span_compare(left_info->distribution_encoded,right_info->distribution_encoded,
      tree->work,&order);
  if (result != TC_TLV_OK) return result;
  *equal = order == 0;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_delta_compatible(const tc_x509_crl* base,
    const tc_x509_crl_extension_info* base_info, const tc_x509_crl* delta,
    const tc_x509_crl_extension_info* delta_info, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* compatible)
{
  const unsigned shared = TC_CRL_EXT_DISTRIBUTION | TC_CRL_EXT_AUTHORITY;
  const unsigned delta_required = TC_CRL_EXT_NUMBER | TC_CRL_EXT_DELTA;
  TC_TLV_result result;
  int order, equal;
  if (!base || !delta || !base_info || !delta_info || !tree || !tree->work || !compatible)
    return TC_TLV_ARGUMENT;
  result = tc_x509_crl_extension_policy(base_info);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_extension_policy(delta_info);
  if (result != TC_TLV_OK) return result;
  if ((base_info->present & TC_CRL_EXT_DELTA) || !(base_info->present & TC_CRL_EXT_NUMBER) ||
      (delta_info->present & delta_required) != delta_required ||
      (base_info->present & shared) != (delta_info->present & shared)) {
    *compatible = 0; return TC_TLV_OK;
  }
  result = tc_x509_crl_number_compare(base_info->number,delta_info->base_number,tree->work,&order);
  if (result != TC_TLV_OK) return result;
  if (order < 0) { *compatible = 0; return TC_TLV_OK; }
  result = tc_x509_crl_number_compare(base_info->number,delta_info->number,tree->work,&order);
  if (result != TC_TLV_OK) return result;
  if (order >= 0) { *compatible = 0; return TC_TLV_OK; }
  result = tc_x509_crl_scope_equal(base,base_info,delta,delta_info,limits,tree,names,&equal);
  if (result != TC_TLV_OK) return result;
  if (!equal) { *compatible = 0; return TC_TLV_OK; }
  if (base_info->authority.has_key_identifier != delta_info->authority.has_key_identifier) {
    *compatible = 0; return TC_TLV_OK;
  }
  const TC_bytes left[] = {base_info->authority.key_identifier,
    base_info->authority.issuer,base_info->authority.serial};
  const TC_bytes right[] = {delta_info->authority.key_identifier,
    delta_info->authority.issuer,delta_info->authority.serial};
  for (size_t i = 0; i < sizeof left / sizeof left[0]; ++i) {
    result = tc_pki_span_compare(left[i],right[i],tree->work,&order);
    if (result != TC_TLV_OK) return result;
    if (order) { *compatible = 0; return TC_TLV_OK; }
  }
  *compatible = 1;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_selected_find(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const TC_X509_certificate* certificate,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t capacity, tc_x509_crl_match* out)
{
  TC_TLV_result result;
  if (!selected || !selected->base || !selected->base_info || !signer || !certificate ||
      !limits || !tree || !tree->work || !names || !out ||
      !!selected->delta != !!selected->delta_info) return TC_TLV_ARGUMENT;
  result = crl_selected_authenticate(selected,signer,provider,limits,tree,names);
  if (result != TC_TLV_OK) return result;
  return crl_selected_lookup(selected,certificate,limits,tree,names,oids,capacity,out);
}

static TC_TLV_result crl_selected_authenticate(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names)
{
  TC_TLV_result result;
  if (selected->base_info->present & TC_CRL_EXT_DELTA) return TC_TLV_INVALID;
  if (selected->delta) {
    int compatible;
    result = tc_x509_crl_delta_compatible(selected->base,selected->base_info,
        selected->delta,selected->delta_info,limits,tree,names,&compatible);
    if (result != TC_TLV_OK) return result;
    if (!compatible) return TC_TLV_INVALID;
  } else {
    result = tc_x509_crl_extension_policy(selected->base_info);
    if (result != TC_TLV_OK) return result;
  }
  /* Matching AKIDs alone do not prove that both signatures use the same key. */
  const tc_x509_crl* crls[] = {selected->base,selected->delta};
  for (size_t i = 0; i < sizeof crls / sizeof crls[0]; ++i) {
    if (!crls[i]) continue;
    result = tc_pki_signature_status(tc_x509_crl_signer_check(crls[i],signer,provider,
        limits,names,tree->work));
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

static TC_TLV_result crl_selected_lookup(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* certificate, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t capacity, tc_x509_crl_match* out)
{
  tc_x509_crl_match base, delta;
  TC_TLV_result result = tc_x509_crl_find(selected->base,selected->base_info,certificate,
      limits,tree,names,oids,capacity,&base);
  if (result != TC_TLV_OK) return result;
  if (selected->delta) {
    result = tc_x509_crl_find(selected->delta,selected->delta_info,certificate,
        limits,tree,names,oids,capacity,&delta);
    if (result != TC_TLV_OK) return result;
  }
  return tc_x509_crl_combine(&base,selected->delta ? &delta : NULL,out);
}

static TC_TLV_result crl_prepared_find(const TC_X509_crl_prepared* prepared,
    const TC_X509_certificate* certificate, size_t* work, tc_x509_crl_match* out)
{
  if (!work || (prepared->count && (!prepared->targets || !prepared->matches)) ||
      prepared->count > SIZE_MAX / sizeof *prepared->targets ||
      prepared->count > SIZE_MAX / sizeof *prepared->matches) return TC_TLV_ARGUMENT;
  for (size_t i = 0; i < prepared->count; ++i) {
    const TC_X509_crl_target* target = &prepared->targets[i];
    if (!target->serial.data || !target->serial.length || !target->issuer.data || !target->issuer.length)
      return TC_TLV_ARGUMENT;
    if (tc_x509_path_charge(work,1) != TC_TLV_OK ||
        tc_x509_path_charge(work,target->serial.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (!tc_pki_equal(target->serial,certificate->serial)) continue;
    if (tc_x509_path_charge(work,target->issuer.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (!tc_pki_equal(target->issuer,certificate->issuer)) continue;
    *out = prepared->matches[i];
    return TC_TLV_OK;
  }
  return TC_TLV_UNSUPPORTED;
}

TC_TLV_result tc_x509_crl_find(const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const TC_X509_certificate* certificate,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names, TC_bytes* oids, size_t capacity,
    tc_x509_crl_match* out)
{
  tc_x509_crl_revoked_reader reader;
  tc_x509_crl_revoked_entry entry;
  tc_x509_crl_match parsed = {0};
  TC_TLV_result result;
  if (!certificate || !out || !certificate->serial.data || !certificate->serial.length ||
      !certificate->issuer.data || !certificate->issuer.length) return TC_TLV_ARGUMENT;
  if (crl && crl->prepared) {
    if (!tree || crl->encoded.length || crl->tbs.length || crl->revoked.length ||
        (crl->version != 1 && crl->version != 2) || !crl->issuer.data || !crl->issuer.length)
      return TC_TLV_ARGUMENT;
    result = tc_x509_crl_extension_policy(extensions);
    if (result != TC_TLV_OK) return result;
    return crl_prepared_find(crl->prepared,certificate,tree->work,out);
  }
  const tc_x509_crl_serial_query query = {certificate->serial,certificate->issuer};
  result = tc_x509_crl_revoked_init(crl,extensions,limits,tree,&reader);
  if (result != TC_TLV_OK) return result;
  while ((result = tc_x509_crl_revoked_next(&reader,tree,oids,capacity,&entry)) == TC_TLV_OK) {
    result = tc_x509_crl_match_update(&entry,&query,limits,tree,names,&parsed);
    if (result != TC_TLV_OK) return result;
  }
  if (result != TC_TLV_END) return result;
  *out = parsed;
  return TC_TLV_OK;
}

static TC_TLV_result crl_query_matches(const tc_x509_crl_revoked_entry* entry,
    const tc_x509_crl_serial_query* certificate, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* matched)
{
  enum { DIRECTORY_NAME = 0xa4 };
  TC_TLV_reader issuers;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!entry || !certificate || !tree || !tree->work || !matched ||
      !entry->entry.serial.length || !certificate->serial.length) return TC_TLV_ARGUMENT;
  /* Serial contents retain DER sign padding, so equality needs no conversion. */
  if (tc_x509_path_charge(tree->work,entry->entry.serial.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (!tc_pki_equal(entry->entry.serial,certificate->serial)) { *matched = 0; return TC_TLV_OK; }
  if (!entry->issuer.names.length)
    return TC_X509_name_equal(entry->issuer.name,certificate->issuer,limits,names,tree->work,matched);
  result = TC_TLV_reader_init(&issuers,entry->issuer.names.data,entry->issuer.names.length,TC_TLV_DER,limits);
  if (result != TC_TLV_OK) return result;
  while (!tc_pki_end(&issuers)) {
    result = tc_pki_tree_next(&issuers,tree,&element);
    if (result != TC_TLV_OK) return result;
    if (!tc_pki_tag(&element,DIRECTORY_NAME)) continue;
    if (tc_x509_path_charge(tree->work,element.value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (tc_pki_equal(element.value,certificate->issuer)) { *matched = 1; return TC_TLV_OK; }
  }
  *matched = 0;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_entry_matches(const tc_x509_crl_revoked_entry* entry,
    const TC_X509_certificate* certificate, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* matched)
{
  if (!certificate) return TC_TLV_ARGUMENT;
  const tc_x509_crl_serial_query query = {certificate->serial,certificate->issuer};
  return crl_query_matches(entry,&query,limits,tree,names,matched);
}

TC_TLV_result tc_x509_crl_match_update(const tc_x509_crl_revoked_entry* entry,
    const tc_x509_crl_serial_query* query, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    tc_x509_crl_match* match)
{
  if (!match) return TC_TLV_ARGUMENT;
  int matched;
  TC_TLV_result result = crl_query_matches(entry,query,limits,tree,names,&matched);
  if (result != TC_TLV_OK || !matched) return result;
  if (match->found) return TC_TLV_INVALID;
  match->found = 1;
  match->reason = entry->extensions.reason;
  match->revoked_at = entry->entry.revoked_at;
  match->has_invalidity_date = !!(entry->extensions.present & TC_CRL_ENTRY_INVALIDITY);
  match->invalidity_date = entry->extensions.invalidity_date;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_revoked_init(const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, tc_x509_crl_revoked_reader* out)
{
  tc_x509_crl_revoked_reader parsed = {0};
  TC_TLV_result result;
  if (!crl || !extensions || !out || (crl->version != 1 && crl->version != 2) ||
      !crl->issuer.data || !crl->issuer.length) return TC_TLV_ARGUMENT;
  result = tc_x509_crl_extension_policy(extensions);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_entries_init(crl->revoked,limits,tree,&parsed.entries);
  if (result != TC_TLV_OK) return result;
  parsed.extensions = extensions; parsed.version = crl->version;
  parsed.issuer.name = crl->issuer;
  *out = parsed;
  return TC_TLV_OK;
}

/* Entry metadata has already validated the complete GeneralNames contents. */
static TC_TLV_result entry_issuer_has_dn(TC_bytes names,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree)
{
  enum { DIRECTORY_NAME = 0xa4 };
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result = TC_TLV_reader_init(&reader,names.data,names.length,TC_TLV_DER,limits);
  if (result != TC_TLV_OK) return result;
  while (!tc_pki_end(&reader)) {
    result = tc_pki_tree_next(&reader,tree,&element);
    if (result != TC_TLV_OK) return result;
    if (tc_pki_tag(&element,DIRECTORY_NAME)) return TC_TLV_OK;
  }
  return TC_TLV_INVALID;
}

TC_TLV_result tc_x509_crl_entry_resolve(const tc_x509_crl_entry* entry,
    const tc_x509_crl_extension_info* extensions, const tc_x509_crl_entry_issuer* issuer,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_x509_crl_revoked_entry* out)
{
  if (!entry || !extensions || !issuer || !out) return TC_TLV_ARGUMENT;
  tc_x509_crl_revoked_entry parsed = {0};
  parsed.entry = *entry;
  parsed.issuer = *issuer;
  TC_TLV_result result = tc_x509_crl_entry_info_read(entry->extensions,limits,tree,
    oids,capacity,&parsed.extensions);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_entry_policy(extensions,&parsed.extensions);
  if (result != TC_TLV_OK) return result;
  if (parsed.extensions.present & TC_CRL_ENTRY_ISSUER) {
    result = entry_issuer_has_dn(parsed.extensions.issuer,limits,tree);
    if (result != TC_TLV_OK) return result;
    parsed.issuer.name = (TC_bytes){NULL,0};
    parsed.issuer.names = parsed.extensions.issuer;
  }
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_revoked_next(tc_x509_crl_revoked_reader* reader,
    const tc_pki_tree_workspace* tree, TC_bytes* oids, size_t capacity,
    tc_x509_crl_revoked_entry* out)
{
  tc_x509_crl_revoked_reader next;
  tc_x509_crl_revoked_entry parsed = {0};
  TC_TLV_result result;
  if (!reader || !reader->extensions || !out) return TC_TLV_ARGUMENT;
  next = *reader;
  result = tc_x509_crl_entry_next(&next.entries,next.version,tree,&parsed.entry);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_entry_resolve(&parsed.entry,next.extensions,&next.issuer,
    &next.entries.limits,tree,oids,capacity,&parsed);
  if (result != TC_TLV_OK) return result;
  next.issuer = parsed.issuer;
  *reader = next; *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_entry_policy(const tc_x509_crl_extension_info* crl,
    const tc_x509_crl_entry_info* entry)
{
  const unsigned known = TC_CRL_ENTRY_REASON | TC_CRL_ENTRY_INVALIDITY | TC_CRL_ENTRY_ISSUER;
  if (!crl || !entry || (entry->present & ~known) || (entry->critical & ~entry->present))
    return TC_TLV_ARGUMENT;
  if (entry->unknown_critical_oid.length) return TC_TLV_UNSUPPORTED;
  if (entry->critical & (TC_CRL_ENTRY_REASON | TC_CRL_ENTRY_INVALIDITY)) return TC_TLV_INVALID;
  if ((entry->present & TC_CRL_ENTRY_ISSUER) &&
      (!(entry->critical & TC_CRL_ENTRY_ISSUER) ||
       !(crl->present & TC_CRL_EXT_DISTRIBUTION) || !crl->distribution.indirect))
    return TC_TLV_INVALID;
  if ((entry->present & TC_CRL_ENTRY_REASON) && entry->reason == CRL_REASON_REMOVE &&
      !(crl->present & TC_CRL_EXT_DELTA)) return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_extension_policy(const tc_x509_crl_extension_info* info)
{
  const unsigned known = TC_CRL_EXT_NUMBER | TC_CRL_EXT_DELTA | TC_CRL_EXT_AUTHORITY |
      TC_CRL_EXT_DISTRIBUTION | TC_CRL_EXT_FRESHEST | TC_CRL_EXT_ISSUER_ALT;
  const unsigned must_be_critical = TC_CRL_EXT_DELTA | TC_CRL_EXT_DISTRIBUTION;
  const unsigned must_be_noncritical = TC_CRL_EXT_NUMBER | TC_CRL_EXT_FRESHEST;
  if (!info || (info->present & ~known) || (info->critical & ~info->present)) return TC_TLV_ARGUMENT;
  if (info->unknown_critical_oid.length) return TC_TLV_UNSUPPORTED;
  if ((info->critical & must_be_noncritical) ||
      ((info->present & must_be_critical) != (info->critical & must_be_critical)))
    return TC_TLV_INVALID;
  if ((info->present & (TC_CRL_EXT_DELTA | TC_CRL_EXT_FRESHEST)) ==
      (TC_CRL_EXT_DELTA | TC_CRL_EXT_FRESHEST)) return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_fresh_at(const tc_x509_crl* crl,
    const TC_X509_time* at, tc_x509_crl_freshness* out)
{
  TC_TLV_result result;
  int from_start, to_end = 0, interval;
  if (!crl || !at || !out || (crl->has_next_update != 0 && crl->has_next_update != 1))
    return TC_TLV_ARGUMENT;
  result = TC_X509_time_compare(at,&crl->this_update,&from_start);
  if (result != TC_TLV_OK) return result;
  if (crl->has_next_update) {
    result = TC_X509_time_compare(&crl->this_update,&crl->next_update,&interval);
    if (result != TC_TLV_OK) return result;
    if (interval > 0) return TC_TLV_INVALID;
    result = TC_X509_time_compare(at,&crl->next_update,&to_end);
    if (result != TC_TLV_OK) return result;
  }
  if (from_start < 0) *out = TC_X509_CRL_FUTURE;
  else if (!crl->has_next_update) *out = TC_X509_CRL_NO_NEXT_UPDATE;
  else *out = to_end < 0 ? TC_X509_CRL_CURRENT : TC_X509_CRL_STALE;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_coverage_at(const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const TC_X509_time* at,
    const tc_pki_distribution_point* point, TC_bytes certificate_issuer, int certificate_ca,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names, tc_x509_crl_coverage* out)
{
  tc_x509_crl_coverage coverage = {0};
  TC_TLV_result result;
  if (!crl || !extensions || !at || !point || !limits || !tree || !tree->work || !names || !out ||
      (certificate_ca != 0 && certificate_ca != 1)) return TC_TLV_ARGUMENT;
  if (tc_x509_path_charge(tree->work,1) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = tc_x509_crl_extension_policy(extensions);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_fresh_at(crl,at,&coverage.freshness);
  if (result != TC_TLV_OK) return result;
  if (coverage.freshness == TC_X509_CRL_CURRENT) {
    const tc_x509_crl_distribution* idp = extensions->present & TC_CRL_EXT_DISTRIBUTION ?
        &extensions->distribution : NULL;
    result = tc_x509_crl_scope_reasons(crl,idp,point,certificate_issuer,certificate_ca,
        limits,tree,names,&coverage.reasons);
    if (result != TC_TLV_OK) return result;
  }
  *out = coverage;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_scope_reasons(const tc_x509_crl* crl,
    const tc_x509_crl_distribution* idp, const tc_pki_distribution_point* point,
    TC_bytes certificate_issuer, int certificate_ca, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, uint16_t* out)
{
  TC_TLV_result result;
  int matched;
  uint16_t reasons = TC_X509_CRL_ALL_REASONS;
  if (!out || !crl || !point || !tree || !tree->work ||
      (certificate_ca != 0 && certificate_ca != 1)) return TC_TLV_ARGUMENT;
  if (idp && (idp->attribute_only || (idp->user_only && certificate_ca) ||
      (idp->ca_only && !certificate_ca))) { *out = 0; return TC_TLV_OK; }
  result = tc_x509_crl_issuer_matches(crl,idp,point,certificate_issuer,limits,tree,names,&matched);
  if (result != TC_TLV_OK) return result;
  if (!matched) { *out = 0; return TC_TLV_OK; }
  result = tc_x509_crl_name_matches(crl,idp,point,certificate_issuer,limits,tree,names,&matched);
  if (result != TC_TLV_OK) return result;
  if (!matched) { *out = 0; return TC_TLV_OK; }
  if (idp && idp->has_reasons) reasons &= idp->reasons;
  if (point->has_reasons) reasons &= point->reasons;
  *out = reasons;
  return TC_TLV_OK;
}

typedef struct {
  TC_TLV_reader names;
  TC_bytes base, suffix;
  int single_name, done;
} distribution_cursor;
typedef struct {
  unsigned type;
  TC_bytes encoded, value, suffix;
} distribution_identity;

static TC_TLV_result distribution_cursor_init(const tc_pki_distribution_name* name,
    TC_bytes base, const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    distribution_cursor* out)
{
  distribution_cursor parsed = {0};
  TC_TLV_result result;
  parsed.single_name = name->relative;
  if (parsed.single_name) {
    result = tc_pki_rdn_contents_check(name->contents,limits,tree);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_tree_name(base,TC_TLV_DER,limits,tree);
    parsed.base = base; parsed.suffix = name->contents;
  } else {
    result = tc_pki_general_names_contents_check(name->contents,limits,tree);
    if (result != TC_TLV_OK) return result;
    result = TC_TLV_reader_init(&parsed.names,name->contents.data,name->contents.length,TC_TLV_DER,limits);
  }
  if (result != TC_TLV_OK) return result;
  *out = parsed;
  return TC_TLV_OK;
}

static TC_TLV_result distribution_cursor_next(distribution_cursor* cursor,
    const tc_pki_tree_workspace* tree, distribution_identity* out)
{
  enum { DIRECTORY_NAME = 4, CHOICE_MASK = 0x1f };
  distribution_identity parsed = {0};
  TC_TLV_element element;
  TC_TLV_result result;
  if (cursor->single_name) {
    if (cursor->done) return TC_TLV_END;
    if (tc_x509_path_charge(tree->work,1) != TC_TLV_OK) return TC_TLV_LIMIT;
    parsed.type = DIRECTORY_NAME; parsed.value = cursor->base;
    parsed.suffix = cursor->suffix; cursor->done = 1;
  } else {
    result = tc_pki_tree_next(&cursor->names,tree,&element);
    if (result != TC_TLV_OK) return result;
    parsed.type = element.header.tag[0] & CHOICE_MASK;
    parsed.encoded = element.encoded; parsed.value = element.value;
  }
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_name_matches(const tc_x509_crl* crl,
    const tc_x509_crl_distribution* idp, const tc_pki_distribution_point* point,
    TC_bytes certificate_issuer, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* matched)
{
  enum { DIRECTORY_NAME = 4 };
  distribution_cursor left, right, right_start;
  distribution_identity a, b;
  TC_bytes base = certificate_issuer;
  tc_pki_distribution_name target;
  TC_TLV_result result;
  if (!crl || !point || !tree || !tree->work || !matched) return TC_TLV_ARGUMENT;
  if (!idp || !idp->name.encoded.length) { *matched = 1; return TC_TLV_OK; }
  target = point->name;
  if (!target.encoded.length) {
    target = (tc_pki_distribution_name){ {NULL,0},point->issuer,0 };
  } else if (target.relative && point->issuer.length) {
    result = tc_pki_distribution_issuer_name(point->issuer,limits,tree,&base);
    if (result != TC_TLV_OK) return result;
  }
  result = distribution_cursor_init(&idp->name,crl->issuer,limits,tree,&left);
  if (result != TC_TLV_OK) return result;
  if (!point->name.encoded.length && !point->issuer.length) {
    /* Issuer-wide fallback uses the original DN without a GeneralName copy. */
    result = tc_pki_tree_name(certificate_issuer,TC_TLV_DER,limits,tree);
    right_start = (distribution_cursor){0};
    right_start.single_name = 1;
    right_start.base = certificate_issuer;
  } else result = distribution_cursor_init(&target,base,limits,tree,&right_start);
  if (result != TC_TLV_OK) return result;
  while ((result = distribution_cursor_next(&left,tree,&a)) == TC_TLV_OK) {
    right = right_start;
    while ((result = distribution_cursor_next(&right,tree,&b)) == TC_TLV_OK) {
      int equal;
      if (a.type != b.type) continue;
      if (a.type == DIRECTORY_NAME) {
        result = tc_pki_name_appended_equal(a.value,a.suffix,b.value,b.suffix,limits,names,tree,&equal);
        if (result != TC_TLV_OK) return result;
      } else {
        /* IDP locators use the certificate's original encoding (5.2.5). */
        if (tc_x509_path_charge(tree->work,a.encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
        equal = tc_pki_equal(a.encoded,b.encoded);
      }
      if (equal) { *matched = 1; return TC_TLV_OK; }
    }
    if (result != TC_TLV_END) return result;
  }
  if (result != TC_TLV_END) return result;
  *matched = 0;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_issuer_matches(const tc_x509_crl* crl,
    const tc_x509_crl_distribution* idp, const tc_pki_distribution_point* point,
    TC_bytes certificate_issuer, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* matched)
{
  TC_bytes issuer;
  TC_TLV_result result;
  if (!crl || !point || !tree || !tree->work || !matched || !crl->issuer.length)
    return TC_TLV_ARGUMENT;
  if (!point->issuer.length)
    return TC_X509_name_equal(certificate_issuer,crl->issuer,limits,names,tree->work,matched);
  result = tc_pki_distribution_issuer_name(point->issuer,limits,tree,&issuer);
  if (result != TC_TLV_OK) return result;
  if (!idp || !idp->indirect) {
    *matched = 0;
    return TC_TLV_OK;
  }
  /* An explicit cRLIssuer must retain the CRL issuer's encoding (4.2.1.13). */
  if (tc_x509_path_charge(tree->work,issuer.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  *matched = tc_pki_equal(issuer,crl->issuer);
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_distribution_read(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    tc_x509_crl_distribution* out)
{
  enum { NAME = 0xa0, USER_ONLY = 0x81, CA_ONLY = 0x82, REASONS = 0x83,
    INDIRECT = 0x84, ATTRIBUTE_ONLY = 0x85, FIELD_MASK = 0x1f };
  tc_x509_crl_distribution parsed = {0};
  TC_TLV_reader fields;
  TC_TLV_element element;
  TC_TLV_result result;
  unsigned previous = 0;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_tree_open(encoded,0x30,TC_TLV_DER,limits,tree,&fields);
  if (result != TC_TLV_OK) return result;
  if (tc_pki_end(&fields)) return TC_TLV_INVALID;
  while (!tc_pki_end(&fields)) {
    result = tc_pki_tree_next(&fields,tree,&element);
    if (result != TC_TLV_OK) return result;
    if (element.header.tag_length != 1) return TC_TLV_INVALID;
    const unsigned tag = element.header.tag[0];
    const unsigned field = (tag & FIELD_MASK) + 1;
    if (field <= previous) return TC_TLV_INVALID;
    previous = field;
    if (tag == NAME) {
      result = tc_pki_distribution_name_read(element.value,limits,tree,&parsed.name);
      if (result != TC_TLV_OK) return result;
    } else if (tag == REASONS) {
      result = tc_pki_reason_flags(element.value,&parsed.reasons);
      if (result != TC_TLV_OK) return result;
      parsed.has_reasons = 1;
    } else {
      /* DEFAULT FALSE is omitted in DER; present booleans must be TRUE. */
      if (element.value.length != 1 || element.value.data[0] != 0xff) return TC_TLV_INVALID;
      switch (tag) {
        case USER_ONLY: parsed.user_only = 1; break;
        case CA_ONLY: parsed.ca_only = 1; break;
        case INDIRECT: parsed.indirect = 1; break;
        case ATTRIBUTE_ONLY: parsed.attribute_only = 1; break;
        default: return TC_TLV_INVALID;
      }
    }
  }
  if (parsed.user_only + parsed.ca_only + parsed.attribute_only > 1) return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

static TC_TLV_result crl_scalar(TC_bytes encoded, unsigned tag, TC_TLV_element* out)
{
  const TC_TLV_limits limits = {encoded.length,encoded.length,1,0};
  TC_TLV_result result = TC_TLV_read(encoded.data,encoded.length,TC_TLV_DER,&limits,out);
  if (result != TC_TLV_OK) return result;
  return tc_pki_tag(out,tag) && out->encoded.length == encoded.length ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result tc_x509_crl_number_read(TC_bytes encoded, TC_bytes* out)
{
  TC_bytes value;
  int negative;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_DER_integer(encoded.data,encoded.length,&value,&negative);
  if (result != TC_TLV_OK) return result;
  if (negative) return TC_TLV_INVALID;
  *out = value;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_reason_read(TC_bytes encoded, unsigned* out)
{
  enum { ENUMERATED_TAG = 10 };
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = crl_scalar(encoded,ENUMERATED_TAG,&element);
  if (result != TC_TLV_OK) return result;
  if (TC_DER_integer_contents(element.value.data,element.value.length) != TC_TLV_OK ||
      element.value.length != 1 || !crl_reason_known(element.value.data[0])) return TC_TLV_INVALID;
  *out = element.value.data[0];
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_invalidity_date_read(TC_bytes encoded, TC_X509_time* out)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = crl_scalar(encoded,0x18,&element);
  if (result != TC_TLV_OK) return result;
  return tc_x509_time_value(&element,out);
}

typedef struct {
  int entry;
  const TC_TLV_limits* limits;
  const tc_pki_tree_workspace* tree;
  tc_x509_crl_extension_info* info;
  tc_x509_crl_entry_info* entry_info;
} crl_extension_context;

static TC_TLV_result crl_extension_value(void* context, const TC_X509_extension* extension)
{
  enum { ISSUER_ALT_NAME = 18, CRL_NUMBER = 20, REASON_CODE = 21, INVALIDITY_DATE = 24,
    DELTA_CRL = 27, ISSUING_DISTRIBUTION_POINT = 28, CERTIFICATE_ISSUER = 29,
    AUTHORITY_KEY_IDENTIFIER = 35, FRESHEST_CRL = 46 };
  crl_extension_context* state = context;
  const unsigned id = tc_pki_extension_id(extension);
  const int number = !state->entry && (id == CRL_NUMBER || id == DELTA_CRL);
  const int authority = !state->entry && id == AUTHORITY_KEY_IDENTIFIER;
  const int names = state->entry ? id == CERTIFICATE_ISSUER : id == ISSUER_ALT_NAME;
  tc_x509_crl_extension_info* info = state->info;
  tc_x509_crl_entry_info* entry_info = state->entry_info;
  if (entry_info) {
    unsigned flag = 0;
    switch (id) {
      case REASON_CODE: flag = TC_CRL_ENTRY_REASON; break;
      case INVALIDITY_DATE: flag = TC_CRL_ENTRY_INVALIDITY; break;
      case CERTIFICATE_ISSUER: flag = TC_CRL_ENTRY_ISSUER; break;
      default:
        if (extension->critical && !entry_info->unknown_critical_oid.length)
          entry_info->unknown_critical_oid = extension->oid;
        break;
    }
    entry_info->present |= flag;
    if (extension->critical) entry_info->critical |= flag;
  }
  if (info) {
    unsigned flag = 0;
    switch (id) {
      case CRL_NUMBER: flag = TC_CRL_EXT_NUMBER; break;
      case DELTA_CRL: flag = TC_CRL_EXT_DELTA; break;
      case AUTHORITY_KEY_IDENTIFIER: flag = TC_CRL_EXT_AUTHORITY; break;
      case ISSUING_DISTRIBUTION_POINT: flag = TC_CRL_EXT_DISTRIBUTION; break;
      case FRESHEST_CRL: flag = TC_CRL_EXT_FRESHEST; break;
      case ISSUER_ALT_NAME: flag = TC_CRL_EXT_ISSUER_ALT; break;
      default:
        if (extension->critical && !info->unknown_critical_oid.length)
          info->unknown_critical_oid = extension->oid;
        break;
    }
    info->present |= flag;
    if (extension->critical) info->critical |= flag;
  }
  if (!state->entry && id == FRESHEST_CRL) {
    TC_TLV_reader points;
    tc_pki_distribution_point point;
    TC_TLV_result result = tc_pki_distribution_points_init(extension->value,
        state->limits,state->tree,&points);
    if (result != TC_TLV_OK) return result;
    while (!tc_pki_end(&points)) {
      result = tc_pki_distribution_point_next(&points,state->tree,&point);
      if (result != TC_TLV_OK) return result;
      /* RFC 5280 5.2.6 permits only names in a CRL's FreshestCRL. */
      if (point.has_reasons || point.issuer.length) return TC_TLV_INVALID;
    }
    if (info) info->freshest = extension->value;
    return TC_TLV_OK;
  }
  if (!state->entry && id == ISSUING_DISTRIBUTION_POINT) {
    tc_x509_crl_distribution distribution;
    TC_TLV_result result = tc_x509_crl_distribution_read(extension->value,state->limits,state->tree,&distribution);
    if (result == TC_TLV_OK && info) {
      info->distribution = distribution;
      info->distribution_encoded = extension->value;
    }
    return result;
  }
  if (!number && !authority && !names &&
      (!state->entry || (id != REASON_CODE && id != INVALIDITY_DATE))) return TC_TLV_OK;
  if (tc_x509_path_charge(state->tree->work,extension->value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (authority) {
    TC_X509_authority_key_identifier identifier;
    TC_TLV_result result = TC_X509_authority_key_identifier_read(extension->value.data,
        extension->value.length,state->limits,&identifier);
    if (result != TC_TLV_OK) return result;
    if (identifier.issuer.length) {
      result = tc_pki_general_names_contents_check(identifier.issuer,state->limits,state->tree);
      if (result != TC_TLV_OK) return result;
    }
    if (info) info->authority = identifier;
    return TC_TLV_OK;
  }
  if (names) {
    TC_bytes contents;
    TC_TLV_result result = TC_DER_sequence(extension->value.data,extension->value.length,&contents);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_general_names_contents_check(contents,state->limits,state->tree);
    if (result == TC_TLV_OK && info) info->issuer_alt = contents;
    if (result == TC_TLV_OK && entry_info) entry_info->issuer = contents;
    return result;
  }
  if (number) {
    TC_bytes value;
    TC_TLV_result result = tc_x509_crl_number_read(extension->value,&value);
    if (result == TC_TLV_OK && info) {
      if (id == CRL_NUMBER) info->number = value;
      else info->base_number = value;
    }
    return result;
  }
  if (id == REASON_CODE) {
    unsigned reason;
    TC_TLV_result result = tc_x509_crl_reason_read(extension->value,&reason);
    if (result == TC_TLV_OK && entry_info) entry_info->reason = reason;
    return result;
  }
  TC_X509_time date;
  TC_TLV_result result = tc_x509_crl_invalidity_date_read(extension->value,&date);
  if (result == TC_TLV_OK && entry_info) entry_info->invalidity_date = date;
  return result;
}

TC_TLV_result tc_x509_crl_entry_info_read(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_x509_crl_entry_info* out)
{
  tc_x509_crl_entry_info parsed = {0};
  crl_extension_context context = {1,limits,tree,NULL,&parsed};
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_extensions_visit(encoded,limits,tree,oids,capacity,crl_extension_value,&context);
  if (result != TC_TLV_OK) return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_extension_info_read(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_x509_crl_extension_info* out)
{
  tc_x509_crl_extension_info parsed = {0};
  crl_extension_context context = {0,limits,tree,&parsed,NULL};
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_extensions_visit(encoded,limits,tree,oids,capacity,crl_extension_value,&context);
  if (result != TC_TLV_OK) return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_extensions_check(const tc_x509_crl* crl,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity)
{
  TC_TLV_reader entries;
  TC_TLV_result result;
  if (!crl || !tree) return TC_TLV_ARGUMENT;
  crl_extension_context context = {0,limits,tree,NULL,NULL};
  result = tc_pki_extensions_visit(crl->extensions,limits,tree,oids,capacity,crl_extension_value,&context);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_entries_init(crl->revoked,limits,tree,&entries);
  if (result != TC_TLV_OK) return result;
  context.entry = 1;
  while (!tc_pki_end(&entries)) {
    tc_x509_crl_entry entry;
    result = tc_x509_crl_entry_next(&entries,crl->version,tree,&entry);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_extensions_visit(entry.extensions,limits,tree,oids,capacity,crl_extension_value,&context);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_entries_init(TC_bytes encoded, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, TC_TLV_reader* out)
{
  TC_TLV_reader parsed;
  TC_TLV_result result;
  if (!out || !tree || !tree->work) return TC_TLV_ARGUMENT;
  if (!encoded.data && !encoded.length)
    return TC_TLV_reader_init(out,NULL,0,TC_TLV_DER,limits);
  result = tc_pki_tree_open(encoded,0x30,TC_TLV_DER,limits,tree,&parsed);
  if (result != TC_TLV_OK) return result;
  /* RFC 5280 omits revokedCertificates when no certificates are revoked. */
  if (tc_pki_end(&parsed)) return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_entry_next(TC_TLV_reader* reader, unsigned version,
    const tc_pki_tree_workspace* tree, tc_x509_crl_entry* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  tc_x509_crl_entry parsed = {0};
  TC_TLV_result result;
  if (!reader || !out || reader->profile != TC_TLV_DER || (version != 1 && version != 2))
    return TC_TLV_ARGUMENT;
  next = *reader;
  result = tc_pki_tree_next(&next,tree,&element);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_open(element.encoded,0x30,TC_TLV_DER,&reader->limits,tree,&fields);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_field(&fields,2,tree,&element);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_integer(element.encoded.data,element.encoded.length,&parsed.serial,&parsed.serial_negative);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_next(&fields,tree,&element);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  result = tc_x509_time_value(&element,&parsed.revoked_at);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_end(&fields)) {
    if (version != 2) return TC_TLV_INVALID;
    result = tc_pki_tree_field(&fields,0x30,tree,&element);
    if (result != TC_TLV_OK) return result;
    if (!element.value.length || !tc_pki_end(&fields)) return TC_TLV_INVALID;
    parsed.extensions = element.encoded;
  }
  *reader = next; *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_metadata_read(const tc_x509_crl_fields* fields,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree, tc_x509_crl* out)
{
  TC_TLV_element element;
  tc_x509_crl parsed = {0};
  unsigned unused;
  if (!fields || !out || !tree || !tree->work) return TC_TLV_ARGUMENT;
  const TC_bytes encoded[] = {fields->algorithm,fields->signature,fields->version,
    fields->inner_algorithm,fields->issuer,fields->this_update,fields->next_update,fields->extensions};
  for (size_t i = 0; i < sizeof encoded / sizeof encoded[0]; ++i) {
    if (!encoded[i].length) continue;
    TC_TLV_result framed = tc_pki_tree_read(encoded[i],TC_TLV_DER,limits,tree,&element);
    if (framed != TC_TLV_OK) return framed;
    if (element.encoded.length != encoded[i].length) return TC_TLV_INVALID;
  }
  TC_TLV_result result = tc_pki_tree_algorithm(fields->algorithm,TC_TLV_DER,
    limits,tree,&parsed.signature_algorithm);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_bit_string(fields->signature.data,fields->signature.length,&parsed.signature,&unused);
  if (result != TC_TLV_OK) return result;
  if (unused || !parsed.signature.length) return TC_TLV_INVALID;
  parsed.version = 1;
  if (fields->version.length) {
    uint32_t version;
    result = TC_DER_uint32(fields->version.data,fields->version.length,&version);
    if (result != TC_TLV_OK) return result;
    if (version != 1) return TC_TLV_INVALID;
    parsed.version = 2;
  }
  if (tc_x509_path_charge(tree->work,fields->inner_algorithm.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (!tc_pki_equal(fields->inner_algorithm,fields->algorithm)) return TC_TLV_INVALID;
  result = TC_TLV_read(fields->issuer.data,fields->issuer.length,TC_TLV_DER,limits,&element);
  if (result != TC_TLV_OK) return result;
  if (!element.value.length) return TC_TLV_INVALID;
  parsed.issuer = fields->issuer;
  result = tc_pki_tree_name(parsed.issuer,TC_TLV_DER,limits,tree);
  if (result != TC_TLV_OK) return result;
  result = TC_TLV_read(fields->this_update.data,fields->this_update.length,TC_TLV_DER,limits,&element);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_time_value(&element,&parsed.this_update);
  if (result != TC_TLV_OK) return result;
  if (fields->next_update.length) {
    result = TC_TLV_read(fields->next_update.data,fields->next_update.length,TC_TLV_DER,limits,&element);
    if (result != TC_TLV_OK) return result;
    result = tc_x509_time_value(&element,&parsed.next_update);
    if (result != TC_TLV_OK) return result;
    parsed.has_next_update = 1;
  }
  if (fields->extensions.length && parsed.version != 2) return TC_TLV_INVALID;
  parsed.extensions = fields->extensions;
  *out = parsed;
  return TC_TLV_OK;
}

/* Layout ranges were checked against input by the source reader. */
static TC_bytes crl_borrow_field(TC_bytes input, tc_source_span span)
{
  const TC_bytes view = {span.length ? input.data + (size_t)span.offset : NULL,(size_t)span.length};
  return view;
}

TC_TLV_result tc_x509_crl_read(TC_bytes input, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, tc_x509_crl* out)
{
  TC_TLV_reader outer, entries;
  tc_x509_crl parsed;
  if (!out || !tree || !tree->work) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_tree_open(input,0x30,TC_TLV_DER,limits,tree,&outer);
  if (result != TC_TLV_OK) return result;
  uint8_t window[TC_TLV_HEADER_BYTES];
  TC_source source = {tc_source_memory_read,&input,input.length};
  tc_source_reader reader;
  tc_x509_crl_layout layout;
  if (tc_source_reader_init(&reader,&source,(TC_buffer){window,sizeof window},
      UINT64_MAX,UINT64_MAX) != TC_RESULT_OK) return TC_TLV_ARGUMENT;
  result = tc_x509_crl_source_layout(&reader,&layout);
  if (result != TC_TLV_OK) return result;
  const tc_x509_crl_fields fields = {
    crl_borrow_field(input,layout.algorithm),crl_borrow_field(input,layout.signature),
    crl_borrow_field(input,layout.version),crl_borrow_field(input,layout.inner_algorithm),
    crl_borrow_field(input,layout.issuer),crl_borrow_field(input,layout.this_update),
    crl_borrow_field(input,layout.next_update),crl_borrow_field(input,layout.extensions)};
  result = tc_x509_crl_metadata_read(&fields,limits,tree,&parsed);
  if (result != TC_TLV_OK) return result;
  parsed.encoded = input;
  parsed.tbs = crl_borrow_field(input,layout.tbs);
  parsed.revoked = crl_borrow_field(input,layout.revoked);
  result = tc_x509_crl_entries_init(parsed.revoked,limits,tree,&entries);
  if (result != TC_TLV_OK) return result;
  while (!tc_pki_end(&entries)) {
    tc_x509_crl_entry entry;
    result = tc_x509_crl_entry_next(&entries,parsed.version,tree,&entry);
    if (result != TC_TLV_OK) return result;
  }
  *out = parsed;
  return TC_TLV_OK;
}
#endif
