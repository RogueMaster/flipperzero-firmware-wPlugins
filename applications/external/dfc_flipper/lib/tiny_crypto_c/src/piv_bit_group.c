/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* BIT group template reader. */
#include <tiny_crypto/piv_card_objects.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "piv_card_objects_internal.h"
#include "pki_internal.h"

enum {
  GROUP_TAG = 0x7f61,
  FINGERS_TAG = 0x02,
  BIT_TAG = 0x7f60,
  MAX_FINGERS = 2 /* Part 1 Table 42: first and optional second finger */
};

/* The contents of the 7F61 template: 02 01 n, then n 7F60 values. */
static TC_TLV_result group_read(TC_bytes contents, TC_PIV_bit_group* out)
{
  TC_TLV_element element = {0};
  TC_TLV_reader reader;
  TC_TLV_result result =
      TC_TLV_reader_init(&reader, contents, TC_TLV_ISO7816, &tc_piv_object_limits);
  if (result == TC_TLV_OK)
    result = tc_pki_field(&reader, FINGERS_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (element.value.length != 1 || element.value.data[0] > MAX_FINGERS)
    return TC_TLV_INVALID;
  out->fingers = element.value.data[0];
  for (unsigned i = 0; i < out->fingers; ++i) {
    result = tc_pki_field(&reader, BIT_TAG, &element);
    if (result != TC_TLV_OK)
      return result;
    if (!element.value.length || element.value.length > TC_PIV_BIT_MAX_BYTES)
      return TC_TLV_INVALID;
    out->templates[i] = element.value;
  }
  return tc_pki_end(&reader) ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result bit_group_read(TC_bytes encoded, TC_PIV_bit_group* out)
{
  TC_PIV_bit_group parsed = {0, {{NULL, 0}, {NULL, 0}}};
  TC_TLV_element element;
  if (!out || !tc_internal_span_valid(encoded.data, encoded.length) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.length, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  if (!encoded.length)
    return TC_TLV_INVALID;
  /* GET DATA returns the template with its own tag (Part 2 section 3.1.2). */
  TC_TLV_result result = TC_TLV_read(encoded, TC_TLV_ISO7816, &tc_piv_object_limits, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, GROUP_TAG) || element.encoded.length != encoded.length)
    return TC_TLV_INVALID;
  result = group_read(element.value, &parsed);
  if (result != TC_TLV_OK)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_bit_group_read(TC_bytes encoded, TC_PIV_bit_group* out)
{
  return tc_piv_object_result(bit_group_read(encoded, out));
}
#endif
