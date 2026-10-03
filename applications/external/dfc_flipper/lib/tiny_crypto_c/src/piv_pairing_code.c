/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Pairing Code Reference Data Container reader. */
#include <tiny_crypto/piv_card_objects.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "piv_card_objects_internal.h"
#include "pki_internal.h"

enum { PAIRING_CODE_TAG = 0x99, ERROR_DETECTION_TAG = 0xfe };

static TC_TLV_result pairing_code_read(TC_bytes encoded, TC_PIV_container_encoding encoding,
                                       TC_bytes* code)
{
  TC_TLV_element element = {0}, error_detection = {0};
  TC_TLV_reader reader = {0};
  TC_bytes contents;
  TC_TLV_result result = tc_piv_object_contents(encoded, encoding, code, sizeof *code, &contents);
  if (result == TC_TLV_OK)
    result = TC_TLV_reader_init(&reader, contents, TC_TLV_ISO7816, &tc_piv_object_limits);
  if (result == TC_TLV_OK)
    result = tc_pki_field(&reader, PAIRING_CODE_TAG, &element);
  if (result == TC_TLV_OK)
    result = tc_pki_field(&reader, ERROR_DETECTION_TAG, &error_detection);
  if (result != TC_TLV_OK)
    return result;
  if (error_detection.value.length || !tc_pki_end(&reader) ||
      element.value.length != TC_PIV_PAIRING_CODE_BYTES)
    return TC_TLV_INVALID;
  /* Part 2 section 2.4.3: the pairing code is eight ASCII digits. */
  for (size_t i = 0; i < TC_PIV_PAIRING_CODE_BYTES; ++i)
    if (element.value.data[i] < '0' || element.value.data[i] > '9')
      return TC_TLV_INVALID;
  *code = element.value;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_pairing_code_read(TC_bytes encoded, TC_PIV_container_encoding encoding,
                                       TC_bytes* code)
{
  return tc_piv_object_result(pairing_code_read(encoded, encoding, code));
}
#endif
