/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_STRING_INTERNAL_H_
#define TC_PKI_STRING_INTERNAL_H_
#include "pki_octets_internal.h"
#include "string_internal.h"

typedef struct {
  tc_asn1_string_state decoder;
  tc_asn1_string_consume consume;
  void* context;
  size_t bytes;
} tc_pki_string_state;

static inline TC_TLV_result tc_pki_string_chunk(void* context, TC_bytes bytes)
{
  tc_pki_string_state* state = context;
  if (bytes.length > SIZE_MAX - state->bytes) return TC_TLV_LIMIT;
  state->bytes += bytes.length;
  /* Teletex needs an application-selected character mapping. */
  if (state->decoder.tag == 0x14) return TC_TLV_OK;
  return tc_asn1_string_feed(&state->decoder,bytes,state->consume,state->context);
}

/* X.690 8.23.3 encodes restricted strings as implicit OCTET STRINGs. Child
 * tags are 04/24; character boundaries need not coincide with chunk boundaries.
 * Callers preflight disjoint input/scratch. Callback output is provisional. */
static inline TC_TLV_result tc_pki_string_walk(TC_bytes encoded, unsigned tag,
    TC_TLV_profile profile, const TC_TLV_limits* limits, TC_TLV_frame* frames,
    size_t capacity, size_t* work, tc_asn1_string_consume consume, void* context,
    size_t* length)
{
  tc_pki_string_state state = {{tag,{0},0},consume,context,0};
  TC_TLV_result result;
  if (!length) return TC_TLV_ARGUMENT;
  if (tag != 0x0c && tag != 0x13 && tag != 0x14 && tag != 0x16 &&
      tag != 0x1a && tag != 0x1c && tag != 0x1e) return TC_TLV_INVALID;
  if (tag == 0x14 && consume) return TC_TLV_UNSUPPORTED;
  result = tc_pki_octets_implicit(encoded,tag,profile,limits,frames,capacity,work,
      tc_pki_string_chunk,&state);
  if (result != TC_TLV_OK) return result;
  if (state.decoder.used) return TC_TLV_INVALID;
  *length = state.bytes;
  return TC_TLV_OK;
}
#endif
