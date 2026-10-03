/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509_path.h>
#if TC_ENABLE_X509_PATH
#include "pki_internal.h"
#include "x509_path_internal.h"
#include "pki_storage_internal.h"
#include "pki_source_internal.h"
#include <string.h>

static TC_TLV_result search_read(TC_bytes encoded, const TC_X509_path_options* options,
                                 const TC_X509_path_workspace* workspace, size_t* work,
                                 TC_X509_certificate* out)
{
  TC_X509_workspace parser = {workspace->frames, workspace->oids, workspace->oid_capacity};
  if (tc_pki_work_charge(work, encoded.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  return TC_X509_read(encoded, &options->parsing, &parser, out);
}

/* Read only the fixed TBSCertificate prefix needed to locate the subject,
 * so name matching can skip a full parse. Any framing error falls back to
 * tc_x509_certificate_read, which owns error status. */
static TC_TLV_result search_subject(TC_bytes encoded, const TC_TLV_limits* limits,
                                    TC_bytes* subject)
{
  TC_TLV_element element;
  TC_TLV_reader outer, tbs;
  TC_TLV_result result;
  if (limits->max_depth < 3 || limits->max_elements < 8)
    return TC_TLV_LIMIT;
  result = TC_TLV_read(encoded, TC_TLV_DER, limits, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, 0x30) || element.encoded.length != encoded.length)
    return TC_TLV_INVALID;
  if ((result = TC_TLV_reader_init(&outer, element.value, TC_TLV_DER, limits)) != TC_TLV_OK ||
      (result = tc_pki_field(&outer, 0x30, &element)) != TC_TLV_OK ||
      (result = TC_TLV_reader_init(&tbs, element.value, TC_TLV_DER, limits)) != TC_TLV_OK ||
      (result = TC_TLV_next(&tbs, &element)) != TC_TLV_OK)
    return result;
  /* Skip the optional version, then serialNumber. */
  if (tc_pki_tag(&element, 0xa0) && (result = TC_TLV_next(&tbs, &element)) != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, 2))
    return TC_TLV_INVALID;
  /* signature, issuer, validity, subject. */
  for (unsigned field = 0; field < 4; ++field)
    if ((result = tc_pki_field(&tbs, 0x30, &element)) != TC_TLV_OK)
      return result;
  *subject = element.encoded;
  return TC_TLV_OK;
}

/* Map a caller-supplied source's result. A source must not add work, and
 * its failures are errors: they say nothing about the certificates. */
static TC_X509_path_status source_status(TC_TLV_result status, size_t before, size_t* work)
{
  if (*work > before) {
    *work = 0;
    return TC_X509_PATH_ERROR;
  }
  if (status == TC_TLV_OK)
    return TC_X509_PATH_VALID;
  if (status == TC_TLV_LIMIT)
    return TC_X509_PATH_LIMIT;
  if (status == TC_TLV_UNSUPPORTED)
    return TC_X509_PATH_UNSUPPORTED;
  return TC_X509_PATH_ERROR;
}

TC_X509_path_status tc_x509_path_search_source(TC_bytes target, const TC_X509_store_source* source,
                                               const TC_X509_path_options* options,
                                               const TC_X509_path_workspace* validation,
                                               const TC_X509_search_workspace* search, size_t* work,
                                               TC_X509_search_report* out)
{
  TC_X509_certificate certificate;
  TC_X509_path_status failure = TC_X509_PATH_INVALID, status;
  TC_TLV_result parsed;
  size_t depth = 1, capacity, initial_work;
  if (!source || !options || !validation || !search || !work || !out ||
      (source->candidate_count && !source->candidate) ||
      (source->anchor_count && !source->anchor) ||
      (search->capacity && (!search->path || !search->frames)))
    return TC_X509_PATH_ERROR;
  capacity =
      search->capacity < options->max_certificates ? search->capacity : options->max_certificates;
  if (!capacity)
    return TC_X509_PATH_LIMIT;
  if (!source->anchor_count)
    return TC_X509_PATH_INVALID;
  initial_work = *work;
  if (target.length > options->max_input)
    return TC_X509_PATH_LIMIT;
  parsed = search_read(target, options, validation, work, &certificate);
  if (parsed != TC_TLV_OK)
    return tc_x509_path_status(parsed);
  search->path[capacity - 1] = target;
  search->frames[0] = (TC_X509_search_frame){0, 0, target.length, certificate.issuer};
  while (depth) {
    TC_X509_search_frame* frame = &search->frames[depth - 1];
    TC_bytes* path = search->path + capacity - depth;
    int equal;
    if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
      return TC_X509_PATH_LIMIT;
    if (frame->anchor < source->anchor_count) {
      size_t anchor = frame->anchor++;
      size_t before = *work;
      TC_X509_store_anchor trust = {0};
      TC_X509_search_report found;
      parsed = source->anchor(source->context, anchor, work, &trust);
      status = source_status(parsed, before, work);
      if (status != TC_X509_PATH_VALID)
        return status;
      if (!trust.trust.name.data || !trust.trust.name.length)
        return TC_X509_PATH_ERROR;
      parsed = TC_X509_name_equal(frame->issuer, trust.trust.name, &options->parsing,
                                  &validation->names, work, &equal);
      if (parsed != TC_TLV_OK) {
        if (parsed == TC_TLV_ARGUMENT)
          return TC_X509_PATH_ERROR;
        tc_x509_path_remember(tc_x509_path_status(parsed), &failure);
        continue;
      }
      if (!equal)
        continue;
      status = tc_x509_path_validate_anchor(path, depth, &trust, options, validation, work,
                                            &found.validation);
      if (status == TC_X509_PATH_VALID) {
        found.path = path;
        found.count = depth;
        found.anchor_index = anchor;
        found.validation.work_used = initial_work - *work;
        *out = found;
        return status;
      }
      if (status == TC_X509_PATH_ERROR)
        return status;
      tc_x509_path_remember(status, &failure);
    } else if (frame->candidate < source->candidate_count) {
      TC_bytes candidate = {NULL, 0};
      TC_bytes subject;
      int subject_checked = 0;
      size_t i, before = *work;
      parsed = source->candidate(source->context, frame->candidate++, work, &candidate);
      status = source_status(parsed, before, work);
      if (status != TC_X509_PATH_VALID)
        return status;
      if (!candidate.data || !candidate.length)
        return TC_X509_PATH_ERROR;
      /* Compare encodings so duplicate records under distinct entry IDs cannot form cycles. */
      for (i = 0; i < depth; ++i) {
        if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
          return TC_X509_PATH_LIMIT;
        if (candidate.length != path[i].length)
          continue;
        if (tc_pki_work_charge(work, candidate.length) != TC_TLV_OK)
          return TC_X509_PATH_LIMIT;
        if (!candidate.length || !memcmp(candidate.data, path[i].data, candidate.length))
          break;
      }
      if (i != depth)
        continue;
      if (search_subject(candidate, &options->parsing, &subject) == TC_TLV_OK) {
        parsed = TC_X509_name_equal(frame->issuer, subject, &options->parsing, &validation->names,
                                    work, &equal);
        if (parsed != TC_TLV_OK) {
          if (parsed == TC_TLV_ARGUMENT)
            return TC_X509_PATH_ERROR;
          tc_x509_path_remember(tc_x509_path_status(parsed), &failure);
          continue;
        }
        if (!equal)
          continue;
        subject_checked = 1;
      }
      parsed = search_read(candidate, options, validation, work, &certificate);
      if (parsed != TC_TLV_OK) {
        if (parsed == TC_TLV_ARGUMENT)
          return TC_X509_PATH_ERROR;
        tc_x509_path_remember(tc_x509_path_status(parsed), &failure);
        continue;
      }
      if (!subject_checked) {
        parsed = TC_X509_name_equal(frame->issuer, certificate.subject, &options->parsing,
                                    &validation->names, work, &equal);
        if (parsed != TC_TLV_OK) {
          if (parsed == TC_TLV_ARGUMENT)
            return TC_X509_PATH_ERROR;
          tc_x509_path_remember(tc_x509_path_status(parsed), &failure);
          continue;
        }
        if (!equal)
          continue;
      }
      if (depth == capacity || candidate.length > options->max_input - frame->bytes) {
        failure = TC_X509_PATH_LIMIT;
        continue;
      }
      path[-1] = candidate;
      search->frames[depth++] =
          (TC_X509_search_frame){0, 0, frame->bytes + candidate.length, certificate.issuer};
    } else {
      --depth;
    }
  }
  return failure;
}

enum {
  SEARCH_PATH_WRITE = TC_X509_PATH_STORAGE_COUNT,
  SEARCH_FRAMES_WRITE,
  SEARCH_RESULT_WRITE,
  SEARCH_WORK_WRITE,
  SEARCH_WRITE_COUNT
};
TC_X509_path_status tc_x509_path_build_work(TC_bytes target, const TC_X509_store_source* source,
                                            const TC_X509_path_options* options,
                                            const TC_X509_path_workspace* validation,
                                            const TC_X509_search_workspace* search, size_t* work,
                                            TC_X509_search_report* out)
{
  TC_bytes writes[SEARCH_WRITE_COUNT];
  tc_pki_source_guard checked = {source, writes, SEARCH_WRITE_COUNT};
  TC_X509_store_source guarded;
  TC_TLV_result result;
  size_t initial_work;
  if (!source || !options || !validation || !search || !work || !out ||
      (options->flags & ~(unsigned)TC_X509_PATH_SUPPORTED_FLAGS) ||
      (source->candidate_count && !source->candidate) || (source->anchor_count && !source->anchor))
    return TC_X509_PATH_ERROR;
  initial_work = *work;
  {
    tc_pki_storage_plan plan;
    /* Preflight uses private bookkeeping: work may itself alias an input. */
    tc_pki_storage_plan_begin(&plan, writes, SEARCH_WRITE_COUNT, *work);
    tc_x509_path_storage_plan(&plan, validation);
    TC_PKI_PLAN_WRITE(&plan, search->path, search->capacity);
    TC_PKI_PLAN_WRITE(&plan, search->frames, search->capacity);
    TC_PKI_PLAN_WRITE(&plan, out, 1);
    TC_PKI_PLAN_WRITE(&plan, work, 1);
    tc_pki_storage_plan_seal(&plan);
    TC_PKI_PLAN_INPUT(&plan, source, 1);
    TC_PKI_PLAN_INPUT(&plan, options, 1);
    TC_PKI_PLAN_INPUT(&plan, validation, 1);
    TC_PKI_PLAN_INPUT(&plan, search, 1);
    tc_pki_storage_plan_input_span(&plan, target);
    tc_x509_path_options_plan_inputs(&plan, options);
    result = tc_pki_storage_plan_finish(&plan, work);
    if (result != TC_TLV_OK)
      return tc_x509_path_status(result);
  }
  guarded = tc_pki_source_guard_bind(&checked);
  {
    TC_X509_path_status status =
        tc_x509_path_search_source(target, &guarded, options, validation, search, work, out);
    if (status == TC_X509_PATH_VALID)
      out->validation.work_used = initial_work - *work;
    return status;
  }
}

TC_X509_path_status TC_X509_path_build(TC_bytes target, const TC_X509_store_source* source,
                                       const TC_X509_path_options* options,
                                       const TC_X509_path_workspace* validation,
                                       const TC_X509_search_workspace* search,
                                       TC_X509_search_report* out)
{
  size_t work;
  if (!options)
    return TC_X509_PATH_ERROR;
  work = options->max_work;
  return tc_x509_path_build_work(target, source, options, validation, search, &work, out);
}

#endif
