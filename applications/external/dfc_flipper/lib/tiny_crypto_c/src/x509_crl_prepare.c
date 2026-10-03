/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509_crl_source.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_crl_source_internal.h"
#include "source_hash_internal.h"
#include "pki_signature_internal.h"
#include "pki_storage_internal.h"
#include "internal.h"

enum { CRL_JOB_EMPTY, CRL_JOB_SCANNING, CRL_JOB_HASHING, CRL_JOB_COMPLETE, CRL_JOB_FAILED };
enum { CRL_DIGEST_BYTES = 64 };
struct TC_X509_crl_job {
  TC_X509_crl_prepare_options options;
  TC_X509_crl_prepare_workspace workspace;
  tc_source_reader reader;
  tc_x509_crl_source_scan scan;
  tc_source_hash hash;
  TC_X509_crl_record record;
  TC_X509_crl_prepared prepared;
  uint8_t digest[CRL_DIGEST_BYTES];
  unsigned phase;
};
typedef struct {
  char byte;
  TC_X509_crl_job job;
} crl_job_alignment;

size_t TC_X509_crl_prepare_size(void)
{
  return sizeof(TC_X509_crl_job);
}
size_t TC_X509_crl_prepare_alignment(void)
{
  return offsetof(crl_job_alignment, job);
}

static TC_TLV_result prepare_storage(const TC_source* source, const TC_X509_crl_target* targets,
                                     size_t count, const TC_X509_crl_prepare_options* options,
                                     const TC_X509_crl_prepare_workspace* w, size_t* work,
                                     TC_X509_crl_job** out)
{
  TC_bytes writes[13];
  tc_pki_storage_plan plan;
  if (!source || !source->read || !options || !w || !work || !out || !w->state.data ||
      !w->window.data || !w->window.capacity || !w->metadata.data || !w->metadata.capacity ||
      (uintptr_t)w->state.data % TC_X509_crl_prepare_alignment())
    return TC_TLV_ARGUMENT;
  if (w->state.capacity < sizeof(TC_X509_crl_job) || count > w->match_capacity ||
      source->length > options->max_input)
    return TC_TLV_LIMIT;
  tc_pki_storage_plan_begin(&plan, writes, 13, *work);
  tc_pki_storage_plan_write(&plan, w->state.data, w->state.capacity, 1);
  tc_pki_storage_plan_write(&plan, w->window.data, w->window.capacity, 1);
  tc_pki_storage_plan_write(&plan, w->metadata.data, w->metadata.capacity, 1);
  tc_pki_storage_plan_write(&plan, w->entry.data, w->entry.capacity, 1);
  tc_pki_storage_plan_write(&plan, w->issuer.data, w->issuer.capacity, 1);
  TC_PKI_PLAN_WRITE(&plan, w->parsing.frames.data, w->parsing.frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, w->parsing.extension_oids, w->parsing.extension_capacity);
  TC_PKI_PLAN_WRITE(&plan, w->names.left, w->names.scalar_capacity);
  TC_PKI_PLAN_WRITE(&plan, w->names.right, w->names.scalar_capacity);
  TC_PKI_PLAN_WRITE(&plan, w->names.matched, w->names.attribute_capacity);
  TC_PKI_PLAN_WRITE(&plan, w->matches, w->match_capacity);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, source, 1);
  TC_PKI_PLAN_INPUT(&plan, options, 1);
  TC_PKI_PLAN_INPUT(&plan, w, 1);
  TC_PKI_PLAN_INPUT(&plan, targets, count);
  for (size_t i = 0; plan.status == TC_TLV_OK && i < count; ++i) {
    if (!targets[i].serial.data || !targets[i].serial.length || !targets[i].issuer.data ||
        !targets[i].issuer.length)
      return TC_TLV_ARGUMENT;
    tc_pki_storage_plan_input_span(&plan, targets[i].serial);
    tc_pki_storage_plan_input_span(&plan, targets[i].issuer);
  }
  return tc_pki_storage_plan_finish(&plan, work);
}

TC_TLV_result TC_X509_crl_prepare_begin(const TC_source* source, const TC_X509_crl_target* targets,
                                        size_t count, const TC_X509_crl_prepare_options* options,
                                        const TC_X509_crl_prepare_workspace* workspace,
                                        size_t* work, TC_X509_crl_job** out)
{
  TC_TLV_result result = prepare_storage(source, targets, count, options, workspace, work, out);
  if (result != TC_TLV_OK)
    return result;
  TC_X509_crl_job* job = (TC_X509_crl_job*)workspace->state.data;
  memset(job, 0, sizeof *job);
  job->options = *options;
  job->workspace = *workspace;
  job->phase = CRL_JOB_FAILED;
  if (tc_source_reader_init(&job->reader, source, workspace->window, options->max_read_bytes,
                            options->max_reads) != TC_RESULT_OK)
    return TC_TLV_ARGUMENT;
  tc_x509_crl_layout layout;
  result = tc_x509_crl_source_layout(&job->reader, &layout);
  if (result != TC_TLV_OK)
    return result;
  const tc_pki_tree_workspace tree = {workspace->parsing.frames.data,
                                      workspace->parsing.frames.capacity, work};
  result = tc_x509_crl_source_metadata(&job->reader, &layout, workspace->metadata,
                                       &options->parsing, &tree, &job->record.crl);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_extension_info_read(
      job->record.crl.extensions, &options->parsing, &tree, workspace->parsing.extension_oids,
      workspace->parsing.extension_capacity, &job->record.extensions);
  if (result != TC_TLV_OK)
    return result;
  /* Keep INVALID/UNSUPPORTED CRL extension policy in the record, as index init
   * does. The resolver skips such records (RFC 5280 section 5.2). */
  job->record.policy = tc_x509_crl_extension_policy(&job->record.extensions);
  if (job->record.policy != TC_TLV_OK && job->record.policy != TC_TLV_INVALID &&
      job->record.policy != TC_TLV_UNSUPPORTED)
    return job->record.policy;
  TC_signature_algorithm algorithm;
  result = tc_pki_signature_algorithm_read(&job->record.crl.signature_algorithm, TC_TLV_DER,
                                           &options->parsing, &tree, &algorithm);
  if (result != TC_TLV_OK)
    return result;
  TC_result hash = tc_source_hash_init(&job->hash, &job->reader, algorithm.hash, layout.tbs.offset,
                                       layout.tbs.length);
  if (hash != TC_RESULT_OK)
    return tc_source_status(hash);
  /* Entries of a CRL with unusable extensions cannot be interpreted. The zero
   * iterator yields no entries, so every target completes unmatched. */
  tc_x509_crl_source_revoked revoked = {0};
  if (job->record.policy == TC_TLV_OK)
    result = tc_x509_crl_source_revoked_init(&job->reader, layout.revoked, &job->record.crl,
                                             &job->record.extensions, options->max_entries,
                                             workspace->issuer, &revoked);
  if (result == TC_TLV_OK)
    result =
        tc_x509_crl_source_scan_init(&job->reader, &revoked, targets, count, workspace->matches,
                                     workspace->match_capacity, &job->scan);
  if (result != TC_TLV_OK) {
    TC_secure_zero(&job->hash, sizeof job->hash);
    return result;
  }
  job->prepared.targets = targets;
  job->prepared.matches = workspace->matches;
  job->prepared.count = count;
  job->prepared.hash = algorithm.hash;
  job->phase = CRL_JOB_SCANNING;
  *out = job;
  return TC_TLV_OK;
}

/* The job's workspace and targets stay unchanged while outputs are written. */
static void prepare_plan_inputs(tc_pki_storage_plan* plan, const TC_X509_crl_job* job)
{
  const TC_X509_crl_prepare_workspace* w = &job->workspace;
  tc_pki_storage_plan_input(plan, w->state.data, w->state.capacity, 1);
  tc_pki_storage_plan_input(plan, w->window.data, w->window.capacity, 1);
  tc_pki_storage_plan_input(plan, w->metadata.data, w->metadata.capacity, 1);
  tc_pki_storage_plan_input(plan, w->entry.data, w->entry.capacity, 1);
  tc_pki_storage_plan_input(plan, w->issuer.data, w->issuer.capacity, 1);
  TC_PKI_PLAN_INPUT(plan, w->parsing.frames.data, w->parsing.frames.capacity);
  TC_PKI_PLAN_INPUT(plan, w->parsing.extension_oids, w->parsing.extension_capacity);
  TC_PKI_PLAN_INPUT(plan, w->names.left, w->names.scalar_capacity);
  TC_PKI_PLAN_INPUT(plan, w->names.right, w->names.scalar_capacity);
  TC_PKI_PLAN_INPUT(plan, w->names.matched, w->names.attribute_capacity);
  TC_PKI_PLAN_INPUT(plan, w->matches, w->match_capacity);
  TC_PKI_PLAN_INPUT(plan, job->prepared.targets, job->prepared.count);
  for (size_t i = 0; plan->status == TC_TLV_OK && i < job->prepared.count; ++i) {
    tc_pki_storage_plan_input_span(plan, job->prepared.targets[i].serial);
    tc_pki_storage_plan_input_span(plan, job->prepared.targets[i].issuer);
  }
}

TC_TLV_result TC_X509_crl_prepare_step(TC_X509_crl_job* job, size_t max_entries, size_t max_bytes,
                                       size_t* work, int* complete)
{
  if (!job || !work || !complete || !max_entries || !max_bytes ||
      (job->phase != CRL_JOB_SCANNING && job->phase != CRL_JOB_HASHING))
    return TC_TLV_ARGUMENT;
  TC_bytes writes[2];
  tc_pki_storage_plan plan;
  tc_pki_storage_plan_begin(&plan, writes, 2, *work);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  TC_PKI_PLAN_WRITE(&plan, complete, 1);
  tc_pki_storage_plan_seal(&plan);
  prepare_plan_inputs(&plan, job);
  TC_TLV_result result = tc_pki_storage_plan_finish(&plan, work);
  if (result != TC_TLV_OK)
    return result;
  const tc_pki_tree_workspace tree = {job->workspace.parsing.frames.data,
                                      job->workspace.parsing.frames.capacity, work};
  int done = 0;
  if (job->phase == CRL_JOB_SCANNING) {
    result = tc_x509_crl_source_scan_step(
        &job->scan, max_entries, job->workspace.entry,
        &(tc_x509_crl_decode){&job->options.parsing, &tree, &job->workspace.names,
                              job->workspace.parsing.extension_oids,
                              job->workspace.parsing.extension_capacity},
        &done);
    if (result == TC_TLV_OK && done)
      job->phase = CRL_JOB_HASHING;
  }
  if (result == TC_TLV_OK && job->phase == CRL_JOB_HASHING) {
    const uint64_t remaining = job->hash.end - job->hash.offset;
    const size_t bytes = remaining < max_bytes ? (size_t)remaining : max_bytes;
    result = tc_pki_work_charge(work, bytes);
    TC_result hashed = TC_RESULT_OK;
    if (result == TC_TLV_OK)
      hashed = tc_source_hash_step(&job->hash, max_bytes, &done);
    if (hashed != TC_RESULT_OK)
      result = tc_source_status(hashed);
    if (result == TC_TLV_OK && done) {
      tc_hash_info info;
      if (!tc_hash_info_get(job->prepared.hash, &info) ||
          tc_source_hash_final(&job->hash, (TC_buffer){job->digest, sizeof job->digest}) !=
              TC_RESULT_OK)
        result = TC_TLV_INVALID;
      else {
        job->prepared.digest = (TC_bytes){job->digest, info.digest_length};
        job->record.crl.prepared = &job->prepared;
        job->phase = CRL_JOB_COMPLETE;
      }
    }
  }
  if (result != TC_TLV_OK) {
    job->phase = CRL_JOB_FAILED;
    TC_secure_zero(&job->hash, sizeof job->hash);
    return result;
  }
  *complete = job->phase == CRL_JOB_COMPLETE;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_crl_prepare_finish(const TC_X509_crl_job* job, TC_X509_crl_record* out)
{
  if (!job || !out || job->phase != CRL_JOB_COMPLETE ||
      !tc_internal_ranges_disjoint(job, sizeof *job, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  TC_bytes writes;
  tc_pki_storage_plan plan;
  tc_pki_storage_plan_begin(&plan, &writes, 1, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  tc_pki_storage_plan_seal(&plan);
  prepare_plan_inputs(&plan, job);
  TC_TLV_result result = tc_pki_storage_plan_finish(&plan, NULL);
  if (result != TC_TLV_OK)
    return result;
  *out = job->record;
  return TC_TLV_OK;
}

void TC_X509_crl_prepare_clear(TC_X509_crl_job* job)
{
  if (job)
    TC_secure_zero(job, sizeof *job);
}
#endif
