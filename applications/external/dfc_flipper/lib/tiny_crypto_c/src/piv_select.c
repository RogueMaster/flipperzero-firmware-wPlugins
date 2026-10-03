/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* SELECT and the application property template reader. */
#include <tiny_crypto/piv_command.h>
#if TC_ENABLE_PIV_COMMAND
#include "internal.h"
#include "piv_link_internal.h"

enum {
  SELECT = 0xa4,
  SELECT_BY_NAME = 0x04,
  TEMPLATE = 0x61,          /* application property template */
  AID = 0x4f,               /* application identifier */
  AUTHORITY = 0x79,         /* coexistent tag allocation authority */
  LABEL = 0x50,             /* application label */
  ALGORITHMS = 0xac,        /* cryptographic algorithm identifier template */
  ALGORITHM = 0x80,         /* one algorithm identifier */
  OBJECT_IDENTIFIER = 0x06, /* object identifier, value 00 */
  INTEGER = 0x02,
  URL = 0x5f50,    /* uniform resource locator */
  LIMITS = 0x7f66, /* extended length information (ISO/IEC 7816-4 12.8.1) */
  SUITE_CS2 = 0x27,
  SUITE_CS7 = 0x2e,
  /* ISO/IEC 7816-4 12.8.1 limits the channel can apply: a header, and SW1
   * SW2 with one byte. */
  MIN_COMMAND_BYTES = TC_APDU_HEADER_BYTES,
  MIN_RESPONSE_BYTES = TC_APDU_STATUS_BYTES + 1
};

const uint8_t* tc_piv_aid_prefix(TC_PIV_application_id application)
{
  if (application == TC_PIV_APPLICATION_PIV)
    return tc_piv_aid_prefixes[0];
  if (application == TC_PIV_APPLICATION_TWIC)
    return tc_piv_aid_prefixes[1];
  return NULL;
}

/* Fixed framing bounds of the template (documented in piv_command.h). */
static const TC_TLV_limits template_limits = {4096, 4096, 64, 4};

static int tag_is(const TC_TLV_element* element, uint16_t tag)
{
  const TC_TLV_header* header = &element->header;
  if (tag > 0xff)
    return header->tag_length == 2 && header->tag[0] == (uint8_t)(tag >> 8) &&
           header->tag[1] == (uint8_t)tag;
  return header->tag_length == 1 && header->tag[0] == tag;
}

/* Keep the value of a DO that may appear once. seen collects one bit per DO,
 * and a second occurrence is INVALID. */
static TC_TLV_result keep_once(TC_bytes* slot, unsigned* seen, unsigned bit,
                               const TC_TLV_element* element)
{
  if (*seen & bit)
    return TC_TLV_INVALID;
  *seen |= bit;
  *slot = element->value;
  return TC_TLV_OK;
}

/* Read a 7F66 size limit: a nonzero unsigned big-endian count. ISO/IEC
 * 7816-4 12.8.1 asks for a positive INTEGER, and TWIC NEXGEN cards send 32769
 * as 80 01 without the leading 00 of two's complement, so a set top bit is a
 * size. A leading 00 is accepted only before such a byte. Values above
 * SIZE_MAX read as SIZE_MAX. */
static TC_TLV_result size_limit(TC_bytes value, size_t* out)
{
  if (!value.length || (value.length > 1 && !value.data[0] && !(value.data[1] & 0x80)))
    return TC_TLV_INVALID;
  size_t result = 0;
  for (size_t i = 0; i < value.length; ++i)
    result = result > (SIZE_MAX >> 8) ? SIZE_MAX : (result << 8) | value.data[i];
  if (!result)
    return TC_TLV_INVALID;
  *out = result;
  return TC_TLV_OK;
}

/* DO 7F66: exactly two 02 size limits, command and response APDU bytes. */
static TC_TLV_result limits_read(const TC_TLV_reader* parent, const TC_TLV_element* element,
                                 TC_PIV_application* out)
{
  TC_TLV_reader reader;
  TC_TLV_element integer;
  size_t values[2] = {0, 0};
  TC_TLV_result result = TC_TLV_reader_child(&reader, parent, element);
  for (size_t i = 0; result == TC_TLV_OK && i < 2; ++i) {
    result = TC_TLV_next(&reader, &integer);
    if (result == TC_TLV_OK)
      result = tag_is(&integer, INTEGER) ? size_limit(integer.value, &values[i]) : TC_TLV_INVALID;
  }
  if (result == TC_TLV_OK && TC_TLV_next(&reader, &integer) != TC_TLV_END)
    result = TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (values[0] < MIN_COMMAND_BYTES || values[1] < MIN_RESPONSE_BYTES)
    return TC_TLV_INVALID;
  out->max_command_bytes = values[0];
  out->max_response_bytes = values[1];
  return TC_TLV_OK;
}

/* AC (SP 800-73-5 Part 2 Table 5): one-byte 80 algorithm identifiers, at
 * least one, exactly one 06 01 00, and at most one of the SM suites 27 and 2E
 * (Part 2 3.1.1). */
static TC_TLV_result algorithms_read(const TC_TLV_reader* parent, const TC_TLV_element* element,
                                     uint8_t* suite)
{
  TC_TLV_reader reader;
  TC_TLV_element child;
  size_t algorithms = 0, identifiers = 0;
  uint8_t found = 0;
  TC_TLV_result result = TC_TLV_reader_child(&reader, parent, element);
  while (result == TC_TLV_OK && (result = TC_TLV_next(&reader, &child)) == TC_TLV_OK) {
    if (tag_is(&child, ALGORITHM)) {
      if (child.value.length != 1)
        return TC_TLV_INVALID;
      ++algorithms;
      const uint8_t algorithm = child.value.data[0];
      if (algorithm == SUITE_CS2 || algorithm == SUITE_CS7) {
        if (found)
          return TC_TLV_INVALID;
        found = algorithm;
      }
    } else if (tag_is(&child, OBJECT_IDENTIFIER)) {
      if (child.value.length != 1 || child.value.data[0] || ++identifiers > 1)
        return TC_TLV_INVALID;
    }
  }
  if (result != TC_TLV_END)
    return result;
  if (!algorithms || identifiers != 1)
    return TC_TLV_INVALID;
  *suite = found;
  return TC_TLV_OK;
}

/* 79 (Part 2 Table 4) holds one nonempty 4F. */
static TC_TLV_result authority_read(const TC_TLV_reader* parent, const TC_TLV_element* element)
{
  TC_TLV_reader reader;
  TC_TLV_element child;
  TC_bytes rid = {NULL, 0};
  unsigned seen = 0;
  TC_TLV_result result = TC_TLV_reader_child(&reader, parent, element);
  while (result == TC_TLV_OK && (result = TC_TLV_next(&reader, &child)) == TC_TLV_OK)
    if (tag_is(&child, AID) && (result = keep_once(&rid, &seen, 1u, &child)) != TC_TLV_OK)
      return result;
  if (result != TC_TLV_END)
    return result;
  return rid.length ? TC_TLV_OK : TC_TLV_INVALID;
}

/* The DOs of one 61 template. seen holds one bit per FIELD_* DO. */
enum {
  FIELD_AID = 1u << 0,
  FIELD_AUTHORITY = 1u << 1,
  FIELD_LABEL = 1u << 2,
  FIELD_URL = 1u << 3,
  FIELD_ALGORITHMS = 1u << 4,
  FIELD_LIMITS = 1u << 5
};
typedef struct {
  TC_bytes aid, authority, label, url, algorithms;
  unsigned seen;
  uint8_t suite;
} template_fields;

static TC_TLV_result template_read(const TC_TLV_reader* parent, const TC_TLV_element* element,
                                   template_fields* fields)
{
  TC_TLV_reader reader;
  TC_TLV_element child;
  TC_TLV_result result = TC_TLV_reader_child(&reader, parent, element);
  while (result == TC_TLV_OK && (result = TC_TLV_next(&reader, &child)) == TC_TLV_OK) {
    if (tag_is(&child, AID))
      result = keep_once(&fields->aid, &fields->seen, FIELD_AID, &child);
    else if (tag_is(&child, AUTHORITY)) {
      result = keep_once(&fields->authority, &fields->seen, FIELD_AUTHORITY, &child);
      if (result == TC_TLV_OK)
        result = authority_read(&reader, &child);
    } else if (tag_is(&child, LABEL))
      result = keep_once(&fields->label, &fields->seen, FIELD_LABEL, &child);
    else if (tag_is(&child, URL))
      result = keep_once(&fields->url, &fields->seen, FIELD_URL, &child);
    else if (tag_is(&child, ALGORITHMS)) {
      result = keep_once(&fields->algorithms, &fields->seen, FIELD_ALGORITHMS, &child);
      if (result == TC_TLV_OK)
        result = algorithms_read(&reader, &child, &fields->suite);
    }
  }
  if (result != TC_TLV_END)
    return result;
  return (fields->seen & (FIELD_AID | FIELD_AUTHORITY)) == (FIELD_AID | FIELD_AUTHORITY)
             ? TC_TLV_OK
             : TC_TLV_INVALID;
}

/* Profile from the version bytes (SP 800-73-5 Part 1 2.2, TWIC Part 2 v5
 * 4.1, TWIC Part 3 v4 D.3). */
static TC_TLV_result profile_get(const uint8_t* version, TC_PIV_application_id expected,
                                 unsigned flags, TC_PIV_card_profile* out)
{
  if (version[0] != TC_PIV_AID_VERSION)
    return TC_TLV_UNSUPPORTED;
  if (expected == TC_PIV_APPLICATION_PIV) {
    if (version[1])
      return TC_TLV_UNSUPPORTED;
    *out = TC_PIV_CARD;
  } else if (version[1] == TC_TWIC_AID_SUBVERSION_NEXGEN)
    *out = TC_TWIC_NEXGEN_CARD;
  else if (version[1] == TC_TWIC_AID_SUBVERSION_LEGACY ||
           (flags & TC_PIV_SELECT_TWIC_SUBVERSION_COMPATIBLE))
    *out = TC_TWIC_LEGACY_CARD;
  else
    return TC_TLV_UNSUPPORTED;
  return TC_TLV_OK;
}

static TC_TLV_result application_parse(TC_bytes response, TC_PIV_application_id expected,
                                       unsigned flags, TC_PIV_application* out)
{
  TC_TLV_frame frames[4];
  TC_TLV_result result =
      TC_TLV_walk(response, TC_TLV_ISO7816, &template_limits,
                  (TC_TLV_frames){frames, sizeof frames / sizeof *frames}, NULL, NULL);
  TC_TLV_reader reader = {0};
  TC_TLV_element element;
  if (result == TC_TLV_OK)
    result = TC_TLV_reader_init(&reader, response, TC_TLV_ISO7816, &template_limits);
  if (result != TC_TLV_OK)
    return result;
  template_fields fields;
  memset(&fields, 0, sizeof fields);
  TC_PIV_application parsed;
  memset(&parsed, 0, sizeof parsed);
  size_t index = 0;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    if (tag_is(&element, TEMPLATE)) {
      result = index ? TC_TLV_INVALID : template_read(&reader, &element, &fields);
    } else if (!index) {
      result = TC_TLV_INVALID;
    } else if (tag_is(&element, LIMITS)) {
      if (fields.seen & FIELD_LIMITS)
        result = TC_TLV_INVALID;
      fields.seen |= FIELD_LIMITS;
      if (result == TC_TLV_OK)
        result = limits_read(&reader, &element, &parsed);
    }
    if (result != TC_TLV_OK)
      return result;
    ++index;
  }
  if (result != TC_TLV_END)
    return result;
  if (!index)
    return TC_TLV_INVALID;
  const uint8_t* prefix = tc_piv_aid_prefix(expected);
  if (fields.aid.length != TC_PIV_AID_BYTES ||
      memcmp(fields.aid.data, prefix, TC_PIV_AID_PREFIX_BYTES) != 0)
    return TC_TLV_INVALID;
  const uint8_t* version = fields.aid.data + TC_PIV_AID_PREFIX_BYTES;
  result = profile_get(version, expected, flags, &parsed.profile);
  if (result != TC_TLV_OK)
    return result;
  parsed.aid = fields.aid;
  parsed.label = fields.label;
  parsed.url = fields.url;
  parsed.algorithms = fields.algorithms;
  parsed.version[0] = version[0];
  parsed.version[1] = version[1];
  parsed.sm_suite = fields.suite;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_application_read(TC_bytes response, TC_PIV_application_id expected,
                                      unsigned flags, TC_PIV_application* out)
{
  if (!out || !tc_internal_span_valid(response.data, response.length) ||
      !tc_piv_aid_prefix(expected) ||
      (flags & ~(unsigned)TC_PIV_SELECT_TWIC_SUBVERSION_COMPATIBLE) ||
      !tc_internal_ranges_disjoint(response.data, response.length, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  const TC_TLV_result result = application_parse(response, expected, flags, out);
  /* A template cut short is malformed: the response is complete. */
  return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
}

static TC_PIV_result template_result(TC_TLV_result result)
{
  switch (result) {
  case TC_TLV_LIMIT:
    return TC_PIV_LIMIT;
  case TC_TLV_UNSUPPORTED:
    return TC_PIV_UNSUPPORTED;
  default:
    return TC_PIV_INVALID;
  }
}

/* A new selection resets the PIV security statuses (Part 2 3.1.1). Secure
 * messaging belongs to the PIV application, so selecting another application
 * ends a bound session. Reselecting PIV keeps it, since Part 2 4.3 names no
 * SELECT among the events that destroy the session keys. */
static void selection_reset(TC_PIV_link* link, TC_PIV_application_id application)
{
  if (link->application != (uint8_t)application)
    tc_piv_link_unbind(link);
  link->application = TC_PIV_APPLICATION_NONE;
  link->profile = TC_PIV_CARD;
  link->sm_suite = 0;
  link->flags &= (uint8_t)~(TC_PIV_LINK_VCI | TC_PIV_LINK_PIN_VERIFIED);
}

/* Record the selected application and apply its card limits and GET
 * RESPONSE flags. */
static TC_PIV_result selection_apply(TC_PIV_link* link, TC_PIV_application_id application,
                                     const TC_PIV_application* selected)
{
  const TC_APDU_result result =
      TC_APDU_channel_restrict(&link->channel, selected->max_command_bytes,
                               selected->max_response_bytes, TC_APDU_GET_RESPONSE_PLAIN_CLA);
  if (result != TC_APDU_OK)
    return result == TC_APDU_ERROR ? TC_PIV_ERROR : TC_PIV_INVALID;
  link->application = (uint8_t)application;
  link->profile = (uint8_t)selected->profile;
  link->sm_suite = selected->sm_suite;
  return TC_PIV_OK;
}

TC_PIV_result TC_PIV_select(TC_PIV_link* link, TC_PIV_application_id application, unsigned flags,
                            TC_buffer response, TC_PIV_application* out)
{
  const uint8_t* prefix = tc_piv_aid_prefix(application);
  if (!tc_piv_link_ready(link) || !out || !prefix ||
      (flags & ~(unsigned)TC_PIV_SELECT_TWIC_SUBVERSION_COMPATIBLE) ||
      !tc_piv_response_valid(link, response, out, sizeof *out))
    return TC_PIV_ARGUMENT;
  /* PIV selects by the complete AID, TWIC by the 9-byte prefix (TWIC Part 2
   * v5 5.1, TWIC Part 3 v4 D.3). SELECT is never protected (Part 2 4.2). */
  const TC_bytes aid = application == TC_PIV_APPLICATION_PIV
                           ? (TC_bytes){tc_piv_aid, TC_PIV_AID_BYTES}
                           : (TC_bytes){prefix, TC_PIV_AID_PREFIX_BYTES};
  const TC_APDU_command command = {aid,    TC_APDU_SHORT_MAX_NE, TC_PIV_PLAIN_CLA,
                                   SELECT, SELECT_BY_NAME,       0x00};
  selection_reset(link, application);
  TC_APDU_response answer;
  TC_PIV_result result =
      tc_piv_link_transceive(link, TC_PIV_COMMAND_SELECT, &command, response, &answer);
  if (result != TC_PIV_OK)
    return result;
  if (answer.sw != TC_PIV_SW_SUCCESS_VALUE)
    return tc_piv_link_fail(link, response, answer.sw, TC_PIV_CARD_STATUS);
  TC_PIV_application selected;
  const TC_TLV_result read = TC_PIV_application_read(answer.data, application, flags, &selected);
  if (read != TC_TLV_OK)
    return tc_piv_link_fail(link, response, 0, template_result(read));
  result = selection_apply(link, application, &selected);
  if (result != TC_PIV_OK)
    return tc_piv_link_fail(link, response, 0, result);
  *out = selected;
  return TC_PIV_OK;
}
#endif
