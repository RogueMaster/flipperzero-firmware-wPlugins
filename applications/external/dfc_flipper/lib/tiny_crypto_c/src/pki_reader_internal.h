/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_READER_INTERNAL_H_
#define TC_PKI_READER_INTERNAL_H_
#include "pki_storage_internal.h"
#include "pki_budget_internal.h"
#include <tiny_crypto/x509.h>

enum {
  TC_PKI_READER_RANGES = 5,
  TC_PKI_READER_STORAGE_WORK = TC_PKI_READER_RANGES * (TC_PKI_READER_RANGES - 1) / 2
};

/* Reader storage: every range, including read-only ones, is kept disjoint
 * from every other range, so all of them are recorded as writes. */
static inline TC_TLV_result tc_pki_reader_storage_check(TC_bytes encoded, const void* metadata,
                                                        size_t metadata_size, TC_TLV_frames frames,
                                                        size_t* work, void* out, size_t out_size)
{
  TC_bytes ranges[TC_PKI_READER_RANGES];
  tc_pki_storage_plan plan;
  if (!metadata || !work || !out)
    return TC_TLV_ARGUMENT;
  tc_pki_storage_plan_begin(&plan, ranges, TC_PKI_READER_RANGES, SIZE_MAX);
  tc_pki_storage_plan_write_span(&plan, encoded);
  tc_pki_storage_plan_write(&plan, metadata, 1, metadata_size);
  TC_PKI_PLAN_WRITE(&plan, frames.data, frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  tc_pki_storage_plan_write(&plan, out, out_size, 1);
  /* Check the work pointer before charging the caller's budget. */
  tc_pki_storage_plan_seal(&plan);
  return tc_pki_storage_plan_finish(&plan, NULL);
}

static inline TC_TLV_result tc_pki_reader_storage(TC_bytes encoded, const TC_TLV_limits* limits,
                                                  TC_TLV_frames frames, size_t* work, void* out,
                                                  size_t out_size)
{
  TC_TLV_result result =
      tc_pki_reader_storage_check(encoded, limits, sizeof *limits, frames, work, out, out_size);
  return result == TC_TLV_OK ? tc_pki_work_charge(work, TC_PKI_READER_STORAGE_WORK) : result;
}

/* Storage plan of a reader that fills an X.509 workspace: the input, the
 * limits, the workspace struct, both workspace arrays, an optional work
 * counter and out are kept pairwise disjoint. A NULL work records no range
 * for readers without a work budget. *used receives the comparisons made. */
static inline TC_TLV_result tc_pki_reader_workspace_check(TC_bytes encoded,
                                                          const TC_TLV_limits* limits,
                                                          const TC_X509_workspace* workspace,
                                                          const size_t* work, const void* out,
                                                          size_t out_size, size_t* used)
{
  TC_bytes ranges[7];
  tc_pki_storage_plan plan;
  if (!limits || !workspace || !out)
    return TC_TLV_ARGUMENT;
  tc_pki_storage_plan_begin(&plan, ranges, 7, SIZE_MAX);
  tc_pki_storage_plan_write_span(&plan, encoded);
  TC_PKI_PLAN_WRITE(&plan, limits, 1);
  TC_PKI_PLAN_WRITE(&plan, workspace, 1);
  TC_PKI_PLAN_WRITE(&plan, workspace->frames.data, workspace->frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, workspace->extension_oids, workspace->extension_capacity);
  if (work)
    TC_PKI_PLAN_WRITE(&plan, work, 1);
  tc_pki_storage_plan_write(&plan, out, out_size, 1);
  tc_pki_storage_plan_seal(&plan);
  *used = tc_pki_storage_plan_used(&plan);
  return tc_pki_storage_plan_finish(&plan, NULL);
}

static inline TC_TLV_result tc_pki_reader_workspace_storage(TC_bytes encoded,
                                                            const TC_TLV_limits* limits,
                                                            const TC_X509_workspace* workspace,
                                                            size_t* work, void* out,
                                                            size_t out_size)
{
  size_t used;
  if (!work)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result =
      tc_pki_reader_workspace_check(encoded, limits, workspace, work, out, out_size, &used);
  return result == TC_TLV_OK ? tc_pki_work_charge(work, used) : result;
}
#endif
