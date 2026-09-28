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
typedef struct { char byte; TC_X509_crl_job job; } crl_job_alignment;

size_t TC_X509_crl_prepare_size(void) { return sizeof(TC_X509_crl_job); }
size_t TC_X509_crl_prepare_alignment(void) { return offsetof(crl_job_alignment,job); }

static TC_TLV_result prepare_storage(const TC_source* source,
    const TC_X509_crl_target* targets, size_t count,
    const TC_X509_crl_prepare_options* options,
    const TC_X509_crl_prepare_workspace* w, size_t* work, TC_X509_crl_job** out)
{
  enum { WRITE_COUNT = 13, INPUT_COUNT = 4 };
  TC_bytes writes[WRITE_COUNT], inputs[INPUT_COUNT];
  if (!source || !source->read || !options || !w || !work || !out ||
      !w->state.data || !w->window.data || !w->window.capacity ||
      !w->metadata.data || !w->metadata.capacity ||
      (uintptr_t)w->state.data % TC_X509_crl_prepare_alignment()) return TC_TLV_ARGUMENT;
  if (w->state.capacity < sizeof(TC_X509_crl_job) || count > w->match_capacity ||
      source->length > options->max_input) return TC_TLV_LIMIT;
  writes[0] = (TC_bytes){w->state.data,w->state.capacity};
  writes[1] = (TC_bytes){w->window.data,w->window.capacity};
  writes[2] = (TC_bytes){w->metadata.data,w->metadata.capacity};
  writes[3] = (TC_bytes){w->entry.data,w->entry.capacity};
  writes[4] = (TC_bytes){w->issuer.data,w->issuer.capacity};
#define SPAN(slot, pointer, count_) do { \
  if (tc_pki_storage_span((pointer),(count_),sizeof *(pointer),&(slot)) != TC_TLV_OK) return TC_TLV_ARGUMENT; \
} while (0)
  SPAN(writes[5],w->parsing.frames,w->parsing.frame_capacity);
  SPAN(writes[6],w->parsing.extension_oids,w->parsing.extension_capacity);
  SPAN(writes[7],w->names.left,w->names.scalar_capacity);
  SPAN(writes[8],w->names.right,w->names.scalar_capacity);
  SPAN(writes[9],w->names.matched,w->names.attribute_capacity);
  SPAN(writes[10],w->matches,w->match_capacity);
  SPAN(writes[11],work,1);
  SPAN(writes[12],out,1);
  SPAN(inputs[0],source,1);
  SPAN(inputs[1],options,1);
  SPAN(inputs[2],w,1);
  SPAN(inputs[3],targets,count);
#undef SPAN
  size_t budget = *work;
  TC_TLV_result result;
  for (size_t i = 0; i < WRITE_COUNT; ++i) {
    if (!writes[i].data && writes[i].length) return TC_TLV_ARGUMENT;
    result = tc_pki_storage_input(writes,i,writes[i],&budget);
    if (result != TC_TLV_OK) return result;
  }
  for (size_t i = 0; i < INPUT_COUNT; ++i) {
    result = tc_pki_storage_input(writes,WRITE_COUNT,inputs[i],&budget);
    if (result != TC_TLV_OK) return result;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!targets[i].serial.data || !targets[i].serial.length ||
        !targets[i].issuer.data || !targets[i].issuer.length) return TC_TLV_ARGUMENT;
    result = tc_pki_storage_input(writes,WRITE_COUNT,targets[i].serial,&budget);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_storage_input(writes,WRITE_COUNT,targets[i].issuer,&budget);
    if (result != TC_TLV_OK) return result;
  }
  *work = budget;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_crl_prepare_begin(const TC_source* source,
    const TC_X509_crl_target* targets, size_t count,
    const TC_X509_crl_prepare_options* options,
    const TC_X509_crl_prepare_workspace* workspace, size_t* work, TC_X509_crl_job** out)
{
  TC_TLV_result result = prepare_storage(source,targets,count,options,workspace,work,out);
  if (result != TC_TLV_OK) return result;
  TC_X509_crl_job* job = (TC_X509_crl_job*)workspace->state.data;
  memset(job,0,sizeof *job);
  job->options = *options; job->workspace = *workspace; job->phase = CRL_JOB_FAILED;
  if (tc_source_reader_init(&job->reader,source,workspace->window,
      options->max_read_bytes,options->max_reads) != TC_RESULT_OK) return TC_TLV_ARGUMENT;
  tc_x509_crl_layout layout;
  result = tc_x509_crl_source_layout(&job->reader,&layout);
  if (result != TC_TLV_OK) return result;
  const tc_pki_tree_workspace tree = {workspace->parsing.frames,workspace->parsing.frame_capacity,work};
  result = tc_x509_crl_source_metadata(&job->reader,&layout,workspace->metadata,
    &options->parsing,&tree,&job->record.crl);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_extension_info_read(job->record.crl.extensions,&options->parsing,&tree,
    workspace->parsing.extension_oids,workspace->parsing.extension_capacity,&job->record.extensions);
  if (result != TC_TLV_OK) return result;
  job->record.policy = tc_x509_crl_extension_policy(&job->record.extensions);
  if (job->record.policy != TC_TLV_OK) return job->record.policy;
  TC_signature_algorithm algorithm;
  result = tc_pki_signature_algorithm_read(&job->record.crl.signature_algorithm,
    TC_TLV_DER,&options->parsing,&tree,&algorithm);
  if (result != TC_TLV_OK) return result;
  TC_result hash = tc_source_hash_init(&job->hash,&job->reader,algorithm.hash,layout.tbs.offset,layout.tbs.length);
  if (hash != TC_RESULT_OK) return hash == TC_RESULT_UNSUPPORTED ? TC_TLV_UNSUPPORTED : TC_TLV_ARGUMENT;
  tc_x509_crl_source_revoked revoked;
  result = tc_x509_crl_source_revoked_init(&job->reader,layout.revoked,&job->record.crl,
    &job->record.extensions,options->max_entries,workspace->issuer,&revoked);
  if (result == TC_TLV_OK) result = tc_x509_crl_source_scan_init(&job->reader,&revoked,
    targets,count,workspace->matches,workspace->match_capacity,&job->scan);
  if (result != TC_TLV_OK) { TC_secure_zero(&job->hash,sizeof job->hash); return result; }
  job->prepared.targets = targets; job->prepared.matches = workspace->matches;
  job->prepared.count = count; job->prepared.hash = algorithm.hash;
  job->phase = CRL_JOB_SCANNING;
  *out = job;
  return TC_TLV_OK;
}

static TC_TLV_result prepare_outputs(const TC_X509_crl_job* job,
    const TC_bytes* writes, size_t count, size_t* budget)
{
  const TC_X509_crl_prepare_workspace* w = &job->workspace;
  TC_TLV_result result;
  for (size_t i = 0; i < count; ++i) {
    result = tc_pki_storage_input(writes,i,writes[i],budget);
    if (result != TC_TLV_OK) return result;
  }
#define INPUT(pointer, count_) do { \
  TC_bytes span; \
  result = tc_pki_storage_span((pointer),(count_),sizeof *(pointer),&span); \
  if (result == TC_TLV_OK) result = tc_pki_storage_input(writes,count,span,budget); \
  if (result != TC_TLV_OK) return result; \
} while (0)
  INPUT(w->state.data,w->state.capacity);
  INPUT(w->window.data,w->window.capacity);
  INPUT(w->metadata.data,w->metadata.capacity);
  INPUT(w->entry.data,w->entry.capacity);
  INPUT(w->issuer.data,w->issuer.capacity);
  INPUT(w->parsing.frames,w->parsing.frame_capacity);
  INPUT(w->parsing.extension_oids,w->parsing.extension_capacity);
  INPUT(w->names.left,w->names.scalar_capacity);
  INPUT(w->names.right,w->names.scalar_capacity);
  INPUT(w->names.matched,w->names.attribute_capacity);
  INPUT(w->matches,w->match_capacity);
  INPUT(job->prepared.targets,job->prepared.count);
  for (size_t i = 0; i < job->prepared.count; ++i) {
    INPUT(job->prepared.targets[i].serial.data,job->prepared.targets[i].serial.length);
    INPUT(job->prepared.targets[i].issuer.data,job->prepared.targets[i].issuer.length);
  }
#undef INPUT
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_crl_prepare_step(TC_X509_crl_job* job, size_t max_entries,
    size_t max_bytes, size_t* work, int* complete)
{
  if (!job || !work || !complete || !max_entries || !max_bytes ||
      (job->phase != CRL_JOB_SCANNING && job->phase != CRL_JOB_HASHING)) return TC_TLV_ARGUMENT;
  const TC_bytes writes[] = {{(const uint8_t*)work,sizeof *work},{(const uint8_t*)complete,sizeof *complete}};
  size_t budget = *work;
  TC_TLV_result result = prepare_outputs(job,writes,2,&budget);
  if (result != TC_TLV_OK) return result;
  *work = budget;
  const tc_pki_tree_workspace tree = {job->workspace.parsing.frames,job->workspace.parsing.frame_capacity,work};
  int done = 0;
  if (job->phase == CRL_JOB_SCANNING) {
    result = tc_x509_crl_source_scan_step(&job->scan,max_entries,job->workspace.entry,
      &job->options.parsing,&tree,&job->workspace.names,job->workspace.parsing.extension_oids,
      job->workspace.parsing.extension_capacity,&done);
    if (result == TC_TLV_OK && done) job->phase = CRL_JOB_HASHING;
  }
  if (result == TC_TLV_OK && job->phase == CRL_JOB_HASHING) {
    const uint64_t remaining = job->hash.end - job->hash.offset;
    const size_t bytes = remaining < max_bytes ? (size_t)remaining : max_bytes;
    result = tc_x509_path_charge(work,bytes);
    TC_result hashed = TC_RESULT_OK;
    if (result == TC_TLV_OK) hashed = tc_source_hash_step(&job->hash,max_bytes,&done);
    if (hashed != TC_RESULT_OK) result = hashed == TC_RESULT_LIMIT ? TC_TLV_LIMIT : TC_TLV_IO;
    if (result == TC_TLV_OK && done) {
      tc_hash_info info;
      if (!tc_hash_info_get(job->prepared.hash,&info) ||
          tc_source_hash_final(&job->hash,(TC_buffer){job->digest,sizeof job->digest}) != TC_RESULT_OK)
        result = TC_TLV_INVALID;
      else {
        job->prepared.digest = (TC_bytes){job->digest,info.digest_length};
        job->record.crl.prepared = &job->prepared;
        job->phase = CRL_JOB_COMPLETE;
      }
    }
  }
  if (result != TC_TLV_OK) {
    job->phase = CRL_JOB_FAILED;
    TC_secure_zero(&job->hash,sizeof job->hash);
    return result;
  }
  *complete = job->phase == CRL_JOB_COMPLETE;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_crl_prepare_finish(const TC_X509_crl_job* job, TC_X509_crl_record* out)
{
  if (!job || !out || job->phase != CRL_JOB_COMPLETE ||
      !tc_internal_ranges_disjoint(job,sizeof *job,out,sizeof *out)) return TC_TLV_ARGUMENT;
  const TC_bytes writes = {(const uint8_t*)out,sizeof *out};
  size_t budget = SIZE_MAX;
  TC_TLV_result result = prepare_outputs(job,&writes,1,&budget);
  if (result != TC_TLV_OK) return result;
  *out = job->record;
  return TC_TLV_OK;
}

void TC_X509_crl_prepare_clear(TC_X509_crl_job* job)
{
  if (job) TC_secure_zero(job,sizeof *job);
}
#endif
