/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/piv_certificate.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "pki_internal.h"

enum { CERTIFICATE_TAG = 0x70, CERTINFO_TAG = 0x71, INTERMEDIATE_TAG = 0x7f21,
  ERROR_DETECTION_TAG = 0xfe, CONTAINER_TAG = 0x53,
  CERTIFICATE_MAX = 1856, INTERMEDIATE_MAX = 601 };
static const TC_TLV_limits limits = {SIZE_MAX,SIZE_MAX,4,1};

static TC_TLV_result read_container(TC_bytes input,
    TC_PIV_certificate_profile profile, TC_PIV_certificate* out)
{
  TC_TLV_element element;
  TC_TLV_reader reader;
  TC_TLV_result result = TC_TLV_read(input.data,input.length,TC_TLV_ISO7816,&limits,&element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element,CONTAINER_TAG) || element.encoded.length != input.length)
    return TC_TLV_INVALID;
  result = TC_TLV_reader_init(&reader,element.value.data,element.value.length,TC_TLV_ISO7816,&limits);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_next(&reader,CERTIFICATE_TAG,&element);
  if (result != TC_TLV_OK) return result;
  if (!element.value.length) return TC_TLV_INVALID;
  if (element.value.length > CERTIFICATE_MAX) return TC_TLV_LIMIT;
  out->certificate = element.value;
  result = tc_pki_next(&reader,CERTINFO_TAG,&element);
  if (result != TC_TLV_OK) return result;
  if (element.value.length != 1 || element.value.data[0] > 1) return TC_TLV_INVALID;
  out->compression = element.value.data[0] ? TC_PIV_CERTIFICATE_GZIP : TC_PIV_CERTIFICATE_PLAIN;
  if (profile == TC_PIV_CERTIFICATE_TWIC)
    return tc_pki_end(&reader) ? TC_TLV_OK : TC_TLV_INVALID;
  result = TC_TLV_next(&reader,&element);
  if (result != TC_TLV_OK) return result;
  if (profile == TC_PIV_CERTIFICATE_SM_SIGNER && tc_pki_tag(&element,INTERMEDIATE_TAG)) {
    if (!element.value.length) return TC_TLV_INVALID;
    if (element.value.length > INTERMEDIATE_MAX) return TC_TLV_LIMIT;
    out->intermediate_cvc = element.encoded;
    result = TC_TLV_next(&reader,&element);
    if (result != TC_TLV_OK) return result;
  }
  return tc_pki_tag(&element,ERROR_DETECTION_TAG) && !element.value.length &&
      tc_pki_end(&reader) ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result TC_PIV_certificate_read(TC_bytes input,
    TC_PIV_certificate_profile profile, TC_PIV_certificate* out)
{
  TC_PIV_certificate decoded = {{NULL,0},{NULL,0},TC_PIV_CERTIFICATE_PLAIN};
  if (!out || (!input.data && input.length) ||
      !tc_internal_ranges_disjoint(input.data,input.length,out,sizeof *out) ||
      (profile != TC_PIV_CERTIFICATE_SLOT && profile != TC_PIV_CERTIFICATE_TWIC &&
       profile != TC_PIV_CERTIFICATE_SM_SIGNER)) return TC_TLV_ARGUMENT;
  TC_TLV_result result = read_container(input,profile,&decoded);
  if (result == TC_TLV_OK) *out = decoded;
  return result == TC_TLV_MORE || result == TC_TLV_END ? TC_TLV_INVALID : result;
}
#endif
