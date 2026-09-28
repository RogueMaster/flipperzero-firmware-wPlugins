/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509
#include <tiny_crypto/x509.h>
#include "internal.h"
#include "pki_bits_internal.h"
#include <string.h>

static const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 4, 1};

TC_TLV_result TC_X509_subject_key_identifier_read(const uint8_t* data, size_t length,
    const TC_TLV_limits* bounds, TC_bytes* out)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_TLV_read(data, length, TC_TLV_DER, bounds, &element);
  if (result != TC_TLV_OK) return result;
  if (element.header.tag_length != 1 || element.header.tag[0] != 4 ||
      element.encoded.length != length) return TC_TLV_INVALID;
  *out = element.value;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_authority_key_identifier_read(const uint8_t* data, size_t length,
    const TC_TLV_limits* bounds, TC_X509_authority_key_identifier* out)
{
  TC_X509_authority_key_identifier parsed = {{NULL,0},{NULL,0},{NULL,0},0,0};
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  unsigned previous = 0;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_TLV_read(data, length, TC_TLV_DER, bounds, &element);
  if (result != TC_TLV_OK) return result;
  if (element.header.tag_length != 1 || element.header.tag[0] != 0x30 ||
      element.encoded.length != length) return TC_TLV_INVALID;
  result = TC_TLV_reader_init(&reader, element.value.data, element.value.length, TC_TLV_DER, bounds);
  if (result != TC_TLV_OK) return result;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    unsigned field;
    if (element.header.tag_length != 1) return TC_TLV_INVALID;
    switch (element.header.tag[0]) {
      case 0x80: field = 1; break;
      case 0xa1: field = 2; break;
      case 0x82: field = 3; break;
      default: return TC_TLV_INVALID;
    }
    if (field <= previous) return TC_TLV_INVALID;
    if (field == 1) {
      parsed.has_key_identifier = 1; parsed.key_identifier = element.value;
    } else if (field == 2) {
      if (!element.value.length) return TC_TLV_INVALID;
      parsed.issuer = element.value;
    } else {
      result = TC_DER_integer_contents(element.value.data, element.value.length);
      if (result != TC_TLV_OK) return result;
      parsed.serial = element.value;
      parsed.serial_negative = (element.value.data[0] & 128) != 0;
    }
    previous = field;
  }
  if (result != TC_TLV_END) return result;
  if (!!parsed.issuer.length != !!parsed.serial.length) return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

static TC_TLV_result sequence_reader(TC_TLV_reader* reader, const uint8_t* data,
                                      size_t length, const TC_TLV_limits* bounds)
{
  TC_TLV_element sequence;
  TC_TLV_result result;
  result = TC_TLV_read(data, length, TC_TLV_DER, bounds, &sequence);
  if (result != TC_TLV_OK) return result;
  if (sequence.header.tag_length != 1 || sequence.header.tag[0] != 0x30 ||
      sequence.encoded.length != length || !sequence.value.length) return TC_TLV_INVALID;
  return TC_TLV_reader_init(reader, sequence.value.data, sequence.value.length, TC_TLV_DER, bounds);
}

TC_TLV_result TC_X509_extensions_init(TC_TLV_reader* reader, const uint8_t* data,
                                      size_t length, const TC_TLV_limits* bounds)
{
  if (!data && !length) return TC_TLV_reader_init(reader, NULL, 0, TC_TLV_DER, bounds);
  return sequence_reader(reader, data, length, bounds);
}

TC_TLV_result TC_X509_policy_mappings_init(TC_TLV_reader* reader, const uint8_t* data,
                                          size_t length, const TC_TLV_limits* bounds)
{
  return sequence_reader(reader, data, length, bounds);
}

TC_TLV_result TC_X509_general_names_init(TC_TLV_reader* reader, const uint8_t* data,
    size_t length, const TC_TLV_limits* bounds)
{
  return sequence_reader(reader, data, length, bounds);
}

TC_TLV_result TC_X509_name_constraints_read(const uint8_t* data, size_t length,
    const TC_TLV_limits* bounds, TC_X509_name_constraints* out)
{
  TC_X509_name_constraints parsed = {{NULL,0},{NULL,0}};
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  unsigned previous = 0;
  if (!out) return TC_TLV_ARGUMENT;
  result = sequence_reader(&reader, data, length, bounds);
  if (result != TC_TLV_OK) return result;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    unsigned tag = element.header.tag[0];
    if (element.header.tag_length != 1 || (tag != 0xa0 && tag != 0xa1) ||
        tag <= previous || !element.value.length) return TC_TLV_INVALID;
    if (tag == 0xa0) parsed.permitted = element.value;
    else parsed.excluded = element.value;
    previous = tag;
  }
  if (result != TC_TLV_END) return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_policy_mapping_next(TC_TLV_reader* reader,
                                         TC_X509_policy_mapping* out)
{
  static const uint8_t any_policy[] = {0x55, 0x1d, 0x20, 0};
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_bytes oids[2];
  TC_TLV_result result;
  unsigned i;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  next = *reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK) return result;
  result = sequence_reader(&fields, element.encoded.data, element.encoded.length, &limits);
  if (result != TC_TLV_OK) return result;
  for (i = 0; i < 2; ++i) {
    if (TC_TLV_next(&fields, &element) != TC_TLV_OK) return TC_TLV_INVALID;
    result = TC_DER_oid(element.encoded.data, element.encoded.length, &oids[i]);
    if (result != TC_TLV_OK) return result;
    if (oids[i].length == sizeof any_policy &&
        memcmp(oids[i].data, any_policy, sizeof any_policy) == 0) return TC_TLV_INVALID;
  }
  if (fields.offset != fields.input.length) return TC_TLV_INVALID;
  out->issuer_policy = oids[0];
  out->subject_policy = oids[1];
  *reader = next;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_extension_next(TC_TLV_reader* reader, TC_X509_extension* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_bytes contents;
  TC_X509_extension extension;
  TC_TLV_result result;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  next = *reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK) return result;
  if (TC_DER_sequence(element.encoded.data, element.encoded.length, &contents) != TC_TLV_OK ||
      TC_TLV_reader_init(&fields, contents.data, contents.length, TC_TLV_DER, &limits) != TC_TLV_OK ||
      TC_TLV_next(&fields, &element) != TC_TLV_OK ||
      TC_DER_oid(element.encoded.data, element.encoded.length, &extension.oid) != TC_TLV_OK) return TC_TLV_INVALID;
  extension.critical = 0;
  if (TC_TLV_next(&fields, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  if (element.header.tag_length == 1 && element.header.tag[0] == 1) {
    /* DEFAULT FALSE is omitted in DER. */
    if (TC_DER_boolean(element.encoded.data, element.encoded.length, &extension.critical) != TC_TLV_OK ||
        !extension.critical || TC_TLV_next(&fields, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  }
  if (element.header.tag_length != 1 || element.header.tag[0] != 4 || fields.offset != contents.length)
    return TC_TLV_INVALID;
  extension.value = element.value;
  *reader = next; *out = extension;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_policies_init(TC_X509_policy_reader* reader,
    const uint8_t* data, size_t length, const TC_TLV_limits* bounds,
    TC_bytes* seen, size_t capacity)
{
  TC_X509_policy_reader parsed;
  TC_TLV_result result;
  if (!reader || (!seen && capacity) || capacity > SIZE_MAX / sizeof *seen ||
      !tc_internal_ranges_disjoint(data, length, seen, capacity * sizeof *seen) ||
      !tc_internal_ranges_disjoint(reader, sizeof *reader, seen, capacity * sizeof *seen) ||
      !tc_internal_ranges_disjoint(data, length, reader, sizeof *reader)) return TC_TLV_ARGUMENT;
  result = sequence_reader(&parsed.reader, data, length, bounds);
  if (result != TC_TLV_OK) return result;
  parsed.seen = seen; parsed.capacity = capacity; parsed.count = 0;
  *reader = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_policy_next(TC_X509_policy_reader* reader, TC_X509_policy* out)
{
  TC_TLV_reader next, fields, qualifiers;
  TC_TLV_element element;
  TC_X509_policy policy = {{NULL,0},{NULL,0}};
  TC_TLV_result result;
  size_t i;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  if (reader->capacity > SIZE_MAX / sizeof *reader->seen ||
      !tc_internal_ranges_disjoint(out, sizeof *out, reader, sizeof *reader) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, reader->seen,
                                   reader->capacity * sizeof *reader->seen) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, reader->reader.input.data,
                                   reader->reader.input.length)) return TC_TLV_ARGUMENT;
  next = reader->reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK) return result;
  if (reader->count >= reader->capacity) return TC_TLV_LIMIT;
  result = sequence_reader(&fields, element.encoded.data, element.encoded.length, &limits);
  if (result != TC_TLV_OK) return result;
  if (TC_TLV_next(&fields, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  result = TC_DER_oid(element.encoded.data, element.encoded.length, &policy.oid);
  if (result != TC_TLV_OK) return result;
  result = TC_TLV_next(&fields, &element);
  if (result == TC_TLV_OK) {
    result = sequence_reader(&qualifiers, element.encoded.data, element.encoded.length, &limits);
    if (result != TC_TLV_OK) return result;
    policy.qualifiers = element.encoded;
    if (fields.offset != fields.input.length) return TC_TLV_INVALID;
  } else if (result != TC_TLV_END) return result;
  for (i = 0; i < reader->count; ++i)
    if (reader->seen[i].length == policy.oid.length &&
        memcmp(reader->seen[i].data, policy.oid.data, policy.oid.length) == 0)
      return TC_TLV_INVALID;
  reader->seen[reader->count++] = policy.oid;
  reader->reader = next;
  *out = policy;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_policy_qualifiers_init(TC_TLV_reader* reader,
    TC_bytes qualifiers, const TC_TLV_limits* bounds)
{
  return TC_X509_extensions_init(reader, qualifiers.data, qualifiers.length, bounds);
}

TC_TLV_result TC_X509_policy_qualifier_next(TC_TLV_reader* reader,
    TC_X509_policy_qualifier* out)
{
  TC_TLV_reader next;
  TC_TLV_element element;
  TC_DER_algorithm pair;
  TC_TLV_result result;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  next = *reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK) return result;
  /* Both schemas are an OID followed by one open type. */
  result = TC_DER_algorithm_identifier(element.encoded.data, element.encoded.length, &pair);
  if (result != TC_TLV_OK) return result;
  if (!pair.parameters.length) return TC_TLV_INVALID;
  out->oid = pair.oid; out->value = pair.parameters;
  *reader = next;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_basic_constraints_read(const uint8_t* data, size_t length,
                                            TC_X509_basic_constraints* out)
{
  TC_X509_basic_constraints constraints = {0,0,0};
  TC_bytes contents;
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_DER_sequence(data, length, &contents);
  if (result != TC_TLV_OK) return result;
  result = TC_TLV_reader_init(&reader, contents.data, contents.length, TC_TLV_DER, &limits);
  if (result != TC_TLV_OK) return result;
  result = TC_TLV_next(&reader, &element);
  if (result == TC_TLV_OK && element.header.tag_length == 1 && element.header.tag[0] == 1) {
    if (TC_DER_boolean(element.encoded.data, element.encoded.length, &constraints.ca) != TC_TLV_OK || !constraints.ca)
      return TC_TLV_INVALID;
    result = TC_TLV_next(&reader, &element);
  }
  if (result == TC_TLV_OK) {
    if (!constraints.ca) return TC_TLV_INVALID;
    result = TC_DER_uint32(element.encoded.data, element.encoded.length, &constraints.path_length);
    if (result != TC_TLV_OK) return result;
    constraints.has_path_length = 1;
    result = TC_TLV_next(&reader, &element);
  }
  if (result != TC_TLV_END) return TC_TLV_INVALID;
  *out = constraints;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_key_usage_read(const uint8_t* data, size_t length, uint16_t* out)
{
  TC_bytes bits;
  TC_TLV_result result;
  unsigned unused;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_DER_bit_string(data, length, &bits, &unused);
  if (result != TC_TLV_OK) return result;
  if (!bits.length) return TC_TLV_INVALID;
  enum { KEY_USAGE_BITS = 9 };
  return tc_pki_named_bits(bits,unused,KEY_USAGE_BITS,out);
}

TC_TLV_result TC_X509_extended_key_usage_read(const uint8_t* data, size_t length,
    TC_bytes* oids, size_t capacity, size_t* count)
{
  TC_bytes contents, oid;
  TC_TLV_reader reader, start;
  TC_TLV_element element;
  TC_TLV_result result;
  TC_TLV_limits bounds = {length, length, capacity, 0};
  size_t found = 0, i;
  if (!count || (!oids && capacity) || capacity > SIZE_MAX / sizeof *oids ||
      !tc_internal_ranges_disjoint(data, length, oids, capacity * sizeof *oids) ||
      !tc_internal_ranges_disjoint(data, length, count, sizeof *count) ||
      !tc_internal_ranges_disjoint(oids, capacity * sizeof *oids, count, sizeof *count))
    return TC_TLV_ARGUMENT;
  result = TC_DER_sequence(data, length, &contents);
  if (result != TC_TLV_OK) return result;
  if (!contents.length) return TC_TLV_INVALID;
  if (!capacity) return TC_TLV_LIMIT;
  result = TC_TLV_reader_init(&reader, contents.data, contents.length, TC_TLV_DER, &bounds);
  if (result != TC_TLV_OK) return result;
  start = reader;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    result = TC_DER_oid(element.encoded.data, element.encoded.length, &oid);
    if (result != TC_TLV_OK) return result;
    ++found;
  }
  if (result != TC_TLV_END) return result;
  /* Validate first so a malformed final OID leaves the caller's array intact. */
  for (i = 0; i < found; ++i) {
    (void)TC_TLV_next(&start, &element);
    oids[i] = element.value;
  }
  *count = found;
  return TC_TLV_OK;
}
TC_TLV_result TC_X509_policy_constraints_read(const uint8_t* data, size_t length,
    TC_X509_policy_constraints* out)
{
  TC_X509_policy_constraints parsed = {0,0,0,0};
  TC_bytes contents;
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  unsigned previous = 0;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_DER_sequence(data, length, &contents);
  if (result != TC_TLV_OK) return result;
  if (!contents.length) return TC_TLV_INVALID;
  result = TC_TLV_reader_init(&reader, contents.data, contents.length, TC_TLV_DER, &limits);
  if (result != TC_TLV_OK) return result;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    unsigned tag = element.header.tag[0];
    uint32_t count;
    if (element.header.tag_length != 1 || (tag != 0x80 && tag != 0x81) || tag <= previous)
      return TC_TLV_INVALID;
    result = TC_DER_uint32_contents(element.value.data, element.value.length, &count);
    if (result != TC_TLV_OK) return result;
    if (tag == 0x80) {
      parsed.has_require_explicit_policy = 1; parsed.require_explicit_policy = count;
    } else {
      parsed.has_inhibit_policy_mapping = 1; parsed.inhibit_policy_mapping = count;
    }
    previous = tag;
  }
  if (result != TC_TLV_END) return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}
#endif
