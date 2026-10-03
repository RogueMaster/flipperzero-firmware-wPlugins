/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Dynamic authentication template 7C (SP 800-73-5 Part 2 3.2.4 Table 7). */
#include <tiny_crypto/tlv.h>
#if TC_ENABLE_PIV_COMMAND
#include "piv_template_internal.h"
#include "internal.h"
#include "tlv_internal.h"

static const uint8_t template_tag = TC_PIV_TEMPLATE_TAG;

/* Value length of one DO, or SIZE_MAX when its parts overflow size_t. */
static size_t value_length(const tc_piv_template_item* item)
{
  size_t length = 0;
  for (size_t i = 0; i < TC_PIV_TEMPLATE_MAX_PARTS; ++i) {
    if (item->parts[i].length > SIZE_MAX - 1 - length)
      return SIZE_MAX;
    length += item->parts[i].length;
  }
  return length;
}

/* Size of the DOs inside 7C, or 0 when one exceeds the writer bound. */
static size_t contents_size(const tc_piv_template_item* items, size_t count)
{
  size_t total = 0;
  for (size_t i = 0; i < count; ++i) {
    const size_t length = value_length(&items[i]);
    const size_t header = length == SIZE_MAX ? 0 : tc_tlv_header_size(1, length);
    if (!header || length > SIZE_MAX - header - total)
      return 0;
    total += header + length;
  }
  return total;
}

size_t tc_piv_template_size(const tc_piv_template_item* items, size_t count)
{
  const size_t contents = contents_size(items, count);
  const size_t header = tc_tlv_header_size(1, contents);
  if ((count && !contents) || !header || contents > SIZE_MAX - header)
    return 0;
  return header + contents;
}

uint8_t* tc_piv_template_write(uint8_t* out, const tc_piv_template_item* items, size_t count)
{
  out = tc_tlv_header_write(out, &template_tag, 1, contents_size(items, count));
  for (size_t i = 0; i < count; ++i) {
    out = tc_tlv_header_write(out, &items[i].tag, 1, value_length(&items[i]));
    for (size_t j = 0; j < TC_PIV_TEMPLATE_MAX_PARTS; ++j) {
      const TC_bytes part = items[i].parts[j];
      if (part.length)
        memcpy(out, part.data, part.length);
      out += part.length;
    }
  }
  return out;
}

/* Framing errors of a complete template are INVALID. LIMIT stays LIMIT. */
static TC_TLV_result framing_result(TC_TLV_result result)
{
  return result == TC_TLV_LIMIT || result == TC_TLV_ARGUMENT ? result : TC_TLV_INVALID;
}

TC_TLV_result tc_piv_template_read(TC_bytes encoded, const uint8_t* tags, size_t count,
                                   TC_bytes* values)
{
  TC_bytes found[TC_PIV_TEMPLATE_MAX_ITEMS];
  if (count > TC_PIV_TEMPLATE_MAX_ITEMS)
    return TC_TLV_ARGUMENT;
  /* The template and its DOs, one level deep. */
  const TC_TLV_limits limits = {encoded.length, encoded.length, count + 1, 1};
  TC_TLV_reader root, reader;
  TC_TLV_element element = {0}, child;
  TC_TLV_result result = TC_TLV_reader_init(&root, encoded, TC_TLV_ISO7816, &limits);
  if (result == TC_TLV_OK)
    result = TC_TLV_next(&root, &element);
  if (result != TC_TLV_OK)
    return framing_result(result);
  if (element.header.tag_length != 1 || element.header.tag[0] != TC_PIV_TEMPLATE_TAG ||
      element.encoded.length != encoded.length)
    return TC_TLV_INVALID;
  result = TC_TLV_reader_child(&reader, &root, &element);
  for (size_t i = 0; result == TC_TLV_OK && i < count; ++i) {
    result = TC_TLV_next(&reader, &child);
    if (result != TC_TLV_OK)
      break;
    if (child.header.tag_length != 1 || child.header.tag[0] != tags[i])
      result = TC_TLV_INVALID;
    found[i] = child.value;
  }
  if (result == TC_TLV_OK && TC_TLV_next(&reader, &child) != TC_TLV_END)
    result = TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return framing_result(result);
  for (size_t i = 0; i < count; ++i)
    values[i] = found[i];
  return TC_TLV_OK;
}
#endif
