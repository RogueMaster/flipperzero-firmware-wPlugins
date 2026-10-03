/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/piv_certificate.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "pki_internal.h"
#include "piv_container_internal.h"

enum {
  CERTIFICATE_TAG = 0x70,
  CERTINFO_TAG = 0x71,
  MSCUID_TAG = 0x72,
  INTERMEDIATE_TAG = 0x7f21,
  ERROR_DETECTION_TAG = 0xfe,
  MSCUID_MAX = 38,
  INTERMEDIATE_MAX = 601
};
static const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 4, 1};

static TC_TLV_result read_container(TC_bytes input, TC_PIV_certificate_profile profile,
                                    size_t max_certificate_bytes, TC_PIV_certificate* out)
{
  TC_TLV_element element;
  TC_TLV_reader reader;
  TC_bytes contents;
  TC_TLV_result result = tc_piv_container_contents(input, &limits, &contents);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_reader_init(&reader, contents, TC_TLV_ISO7816, &limits);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_next(&reader, CERTIFICATE_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!element.value.length)
    return TC_TLV_INVALID;
  /* SP 800-73-5 Part 1 Tables 11, 16-18 and 21-40 footnotes make 1856 bytes a
   * recommended size, so the caller sets the bound. */
  if (element.value.length > max_certificate_bytes)
    return TC_TLV_LIMIT;
  out->certificate = element.value;
  result = tc_pki_next(&reader, CERTINFO_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (element.value.length != 1 || element.value.data[0] > 1)
    return TC_TLV_INVALID;
  out->compression = element.value.data[0] ? TC_PIV_CERTIFICATE_GZIP : TC_PIV_CERTIFICATE_PLAIN;
  /* TWIC Part 2 v5 4.7.1 lists 70 and 71 and calls the structure similar to
   * the PIV one without the MSCUID. NEXGEN cards end it with the empty FE of
   * the PIV form. */
  if (profile == TC_PIV_CERTIFICATE_TWIC && tc_pki_end(&reader))
    return TC_TLV_OK;
  result = TC_TLV_next(&reader, &element);
  if (result != TC_TLV_OK)
    return result;
  /* SP 800-73-5 Part 1 Table 43 places the intermediate CVC before FE. */
  if (profile == TC_PIV_CERTIFICATE_SM_SIGNER && tc_pki_tag(&element, INTERMEDIATE_TAG)) {
    if (!element.value.length)
      return TC_TLV_INVALID;
    if (element.value.length > INTERMEDIATE_MAX)
      return TC_TLV_LIMIT;
    out->intermediate_cvc = element.encoded;
    result = TC_TLV_next(&reader, &element);
    if (result != TC_TLV_OK)
      return result;
  }
  /* SP 800-73-4 Part 1 Tables 10, 15-17 and 20-39 and SP 800-73-5 Part 1
   * Tables 21-40 allow a historic MSCUID of at most 38 bytes before FE. */
  if (profile == TC_PIV_CERTIFICATE_SLOT && tc_pki_tag(&element, MSCUID_TAG)) {
    if (!element.value.length || element.value.length > MSCUID_MAX)
      return TC_TLV_INVALID;
    out->mscuid = element.value;
    result = TC_TLV_next(&reader, &element);
    if (result != TC_TLV_OK)
      return result;
  }
  return tc_pki_tag(&element, ERROR_DETECTION_TAG) && !element.value.length && tc_pki_end(&reader)
             ? TC_TLV_OK
             : TC_TLV_INVALID;
}

TC_TLV_result TC_PIV_certificate_read(TC_bytes input, TC_PIV_certificate_profile profile,
                                      size_t max_certificate_bytes, TC_PIV_certificate* out)
{
  TC_PIV_certificate decoded = {{NULL, 0}, {NULL, 0}, {NULL, 0}, TC_PIV_CERTIFICATE_PLAIN};
  if (!out || (!input.data && input.length) || !max_certificate_bytes ||
      !tc_internal_ranges_disjoint(input.data, input.length, out, sizeof *out) ||
      (profile != TC_PIV_CERTIFICATE_SLOT && profile != TC_PIV_CERTIFICATE_TWIC &&
       profile != TC_PIV_CERTIFICATE_SM_SIGNER))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = read_container(input, profile, max_certificate_bytes, &decoded);
  if (result == TC_TLV_OK)
    *out = decoded;
  return result == TC_TLV_MORE || result == TC_TLV_END ? TC_TLV_INVALID : result;
}
#endif
