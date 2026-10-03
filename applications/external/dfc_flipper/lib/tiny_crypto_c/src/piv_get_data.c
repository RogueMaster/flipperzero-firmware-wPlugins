/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* GET DATA and the per-application framing of its answer (SP 800-73-5
 * Part 2 3.1.2, TWIC Part 2 v5 3.3.6 and 5.2). */
#include <tiny_crypto/piv_command.h>
#if TC_ENABLE_PIV_COMMAND
#include "internal.h"
#include "piv_container_internal.h"
#include "piv_link_internal.h"
#include "tlv_internal.h"

enum {
  GET_DATA = 0xcb,
  TAG_LIST = 0x5c,
  CONTAINER = 0x53,
  MAX_TAG_BYTES = 3,
  /* 5C L and a tag of up to 3 bytes. */
  MAX_DATA_BYTES = 2 + MAX_TAG_BYTES,
  CONSTRUCTED = 0x20
};

/* 1 when tag is one complete ISO/IEC 7816-4 tag of 1 to 3 bytes. The TLV
 * header reader checks it followed by a zero length. */
static int tag_valid(TC_bytes tag)
{
  static const TC_TLV_limits limits = {MAX_TAG_BYTES + 1, 0, 1, 0};
  uint8_t header[MAX_TAG_BYTES + 1] = {0};
  TC_TLV_header parsed;
  if (!tag.data || !tag.length || tag.length > MAX_TAG_BYTES)
    return 0;
  memcpy(header, tag.data, tag.length);
  return TC_TLV_header_read((TC_bytes){header, tag.length + 1}, TC_TLV_ISO7816, &limits, &parsed) ==
             TC_TLV_OK &&
         parsed.tag_length == tag.length;
}

static int tag_equal(const TC_TLV_element* element, TC_bytes tag)
{
  return element->header.tag_length == tag.length &&
         memcmp(element->header.tag, tag.data, tag.length) == 0;
}

/* PIV answers: 7E and 7F61 return their own DO (Part 2 3.1.2), every other
 * tag a 53 container whose only empty form is 53 00 (Part 1 4.1.1). */
static TC_TLV_result piv_frame(TC_bytes data, TC_bytes tag, const TC_TLV_limits* limits,
                               TC_PIV_data_object* out)
{
  static const uint8_t discovery[] = {0x7e}, bit_group[] = {0x7f, 0x61};
  const int own_tag = (tag.length == 1 && tag.data[0] == discovery[0]) ||
                      (tag.length == 2 && !memcmp(tag.data, bit_group, 2));
  if (!own_tag) {
    TC_bytes contents;
    const TC_TLV_result result = tc_piv_container_contents(data, limits, &contents);
    if (result != TC_TLV_OK)
      return result;
    out->encoded = data;
    out->value = contents;
    out->form = TC_PIV_FORM_CONTAINER;
    return TC_TLV_OK;
  }
  TC_TLV_element element;
  const TC_TLV_result result = TC_TLV_read(data, TC_TLV_ISO7816, limits, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tag_equal(&element, tag) || element.encoded.length != data.length || !element.value.length)
    return TC_TLV_INVALID;
  out->encoded = element.encoded;
  out->value = element.value;
  out->form = TC_PIV_FORM_TEMPLATE;
  return TC_TLV_OK;
}

/* TWIC answers: 53 or the requested tag. 53 00, TAG 00 and, for a
 * constructed tag, TAG 02 80 00 are empty (TWIC Part 2 v5 3.3.6). */
static TC_TLV_result twic_frame(TC_bytes data, TC_bytes tag, const TC_TLV_limits* limits,
                                TC_PIV_data_object* out)
{
  static const uint8_t empty_template[] = {0x80, 0x00};
  TC_TLV_element element;
  const TC_TLV_result result = TC_TLV_read(data, TC_TLV_ISO7816, limits, &element);
  if (result != TC_TLV_OK)
    return result;
  const int container = element.header.tag_length == 1 && element.header.tag[0] == CONTAINER;
  if ((!container && !tag_equal(&element, tag)) || element.encoded.length != data.length)
    return TC_TLV_INVALID;
  out->encoded = element.encoded;
  out->value = element.value;
  out->form = container ? TC_PIV_FORM_CONTAINER : TC_PIV_FORM_TEMPLATE;
  if (!container && (tag.data[0] & CONSTRUCTED) && element.value.length == sizeof empty_template &&
      !memcmp(element.value.data, empty_template, sizeof empty_template))
    out->value.length = 0;
  return TC_TLV_OK;
}

/* Frame the answer data of a 9000 or 6282. */
static TC_TLV_result object_frame(const TC_PIV_link* link, TC_bytes data, uint16_t sw, TC_bytes tag,
                                  TC_PIV_data_object* out)
{
  const TC_TLV_limits limits = {data.length, data.length, 1, 1};
  if (!data.length) {
    /* TWIC Part 2 v5 4.5: an uninitialized optional object may answer 9000
     * without data. PIV requires the 53 container. */
    if (link->application != TC_PIV_APPLICATION_TWIC || sw != TC_PIV_SW_SUCCESS_VALUE)
      return TC_TLV_INVALID;
    out->encoded = (TC_bytes){NULL, 0};
    out->value = (TC_bytes){NULL, 0};
    out->form = TC_PIV_FORM_NONE;
    return TC_TLV_OK;
  }
  const TC_TLV_result result = link->application == TC_PIV_APPLICATION_TWIC
                                   ? twic_frame(data, tag, &limits, out)
                                   : piv_frame(data, tag, &limits, out);
  /* The answer is complete, so a value past its end is malformed. */
  return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
}

TC_PIV_result TC_PIV_get_data(TC_PIV_link* link, TC_bytes tag, TC_buffer response,
                              TC_PIV_data_object* out)
{
  if (!tc_piv_link_ready(link) || !out || !tag_valid(tag) ||
      !tc_piv_response_valid(link, response, out, sizeof *out) ||
      !tc_internal_ranges_disjoint(response.data, response.capacity, tag.data, tag.length))
    return TC_PIV_ARGUMENT;
  if (link->application == TC_PIV_APPLICATION_NONE) {
    TC_secure_zero(response.data, response.capacity);
    return TC_PIV_REFUSED;
  }
  /* Data field: 5C L tag (Part 2 Table 6). */
  static const uint8_t tag_list = TAG_LIST;
  uint8_t data[MAX_DATA_BYTES];
  uint8_t* end = tc_tlv_header_write(data, &tag_list, 1, tag.length);
  memcpy(end, tag.data, tag.length);
  const TC_APDU_command command = {{data, (size_t)(end - data) + tag.length},
                                   link->response_ne,
                                   TC_PIV_PLAIN_CLA,
                                   GET_DATA,
                                   0x3f,
                                   0xff};
  TC_APDU_response answer;
  const TC_PIV_result result =
      tc_piv_link_transceive(link, TC_PIV_COMMAND_GET_DATA, &command, response, &answer);
  if (result != TC_PIV_OK)
    return result;
  if (answer.sw != TC_PIV_SW_SUCCESS_VALUE && answer.sw != TC_PIV_SW_END_OF_OBJECT_VALUE)
    return tc_piv_link_fail(link, response, answer.sw, TC_PIV_CARD_STATUS);
  TC_PIV_data_object object;
  if (object_frame(link, answer.data, answer.sw, tag, &object) != TC_TLV_OK)
    return tc_piv_link_fail(link, response, 0, TC_PIV_INVALID);
  object.status = answer.sw;
  *out = object;
  return TC_PIV_OK;
}
#endif
