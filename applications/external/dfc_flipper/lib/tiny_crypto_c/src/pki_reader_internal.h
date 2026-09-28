/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_READER_INTERNAL_H_
#define TC_PKI_READER_INTERNAL_H_
#include "pki_storage_internal.h"
#include "x509_path_internal.h"

enum { TC_PKI_READER_RANGES = 5,
  TC_PKI_READER_STORAGE_WORK = TC_PKI_READER_RANGES * (TC_PKI_READER_RANGES - 1) / 2 };

static inline TC_TLV_result tc_pki_reader_ranges_check(const TC_bytes* ranges, size_t count)
{
  size_t checks = SIZE_MAX;
  for (size_t i = 0; i < count; ++i) {
    TC_TLV_result result = tc_pki_storage_input(ranges,i,ranges[i],&checks);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

static inline TC_TLV_result tc_pki_reader_storage_check(TC_bytes encoded,
    const void* metadata, size_t metadata_size, TC_TLV_frame* frames,
    size_t frame_capacity, size_t* work, void* out, size_t out_size)
{
  TC_bytes ranges[TC_PKI_READER_RANGES];
  if (!metadata || !work || !out ||
      tc_pki_storage_span(encoded.data,encoded.length,1,&ranges[0]) != TC_TLV_OK ||
      tc_pki_storage_span(metadata,1,metadata_size,&ranges[1]) != TC_TLV_OK ||
      tc_pki_storage_span(frames,frame_capacity,sizeof *frames,&ranges[2]) != TC_TLV_OK ||
      tc_pki_storage_span(work,1,sizeof *work,&ranges[3]) != TC_TLV_OK ||
      tc_pki_storage_span(out,out_size,1,&ranges[4]) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  /* Check the work pointer before charging the caller's budget. */
  return tc_pki_reader_ranges_check(ranges,TC_PKI_READER_RANGES);
}

static inline TC_TLV_result tc_pki_reader_storage(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, void* out, size_t out_size)
{
  TC_TLV_result result = tc_pki_reader_storage_check(encoded,limits,sizeof *limits,
      frames,frame_capacity,work,out,out_size);
  return result == TC_TLV_OK ? tc_x509_path_charge(work,TC_PKI_READER_STORAGE_WORK) : result;
}

static inline TC_TLV_result tc_pki_reader_workspace_storage(TC_bytes encoded,
    const TC_TLV_limits* limits, const TC_X509_workspace* workspace,
    size_t* work, void* out, size_t out_size)
{
  enum { RANGE_COUNT = 7, CHECK_WORK = RANGE_COUNT * (RANGE_COUNT - 1) / 2 };
  TC_bytes ranges[RANGE_COUNT];
  if (!limits || !workspace || !work || !out ||
      tc_pki_storage_span(encoded.data,encoded.length,1,&ranges[0]) != TC_TLV_OK ||
      tc_pki_storage_span(limits,1,sizeof *limits,&ranges[1]) != TC_TLV_OK ||
      tc_pki_storage_span(workspace,1,sizeof *workspace,&ranges[2]) != TC_TLV_OK ||
      tc_pki_storage_span(workspace->frames,workspace->frame_capacity,sizeof *workspace->frames,&ranges[3]) != TC_TLV_OK ||
      tc_pki_storage_span(workspace->extension_oids,workspace->extension_capacity,sizeof *workspace->extension_oids,&ranges[4]) != TC_TLV_OK ||
      tc_pki_storage_span(work,1,sizeof *work,&ranges[5]) != TC_TLV_OK ||
      tc_pki_storage_span(out,out_size,1,&ranges[6]) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_reader_ranges_check(ranges,RANGE_COUNT);
  return result == TC_TLV_OK ? tc_x509_path_charge(work,CHECK_WORK) : result;
}
#endif
