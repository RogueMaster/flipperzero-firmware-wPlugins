/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509
#include <tiny_crypto/x509.h>
#include "internal.h"
#include "pki_bits_internal.h"
#include "pki_internal.h"
#include <string.h>

/* Every reader in this file keeps the element and depth budget of the value
 * passed to init. Nested fields open through tc_pki_child_open and charge
 * their elements to the parent reader when accepted. */

TC_TLV_result TC_X509_subject_key_identifier_read(TC_bytes value, const TC_TLV_limits* limits,
                                                  TC_bytes* out)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out || !limits)
    return TC_TLV_ARGUMENT;
  result = TC_TLV_read(value, TC_TLV_DER, limits, &element);
  if (result == TC_TLV_MORE)
    return TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result;
  if (!limits->max_elements)
    return TC_TLV_LIMIT;
  if (!tc_pki_tag(&element, 4) || element.encoded.length != value.length)
    return TC_TLV_INVALID;
  *out = element.value;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_authority_key_identifier_read(TC_bytes value, const TC_TLV_limits* limits,
                                                    TC_X509_authority_key_identifier* out)
{
  TC_X509_authority_key_identifier parsed = {{NULL, 0}, {NULL, 0}, {NULL, 0}, 0, 0};
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  /* RFC 5280 section 4.2.1.1: keyIdentifier [0], authorityCertIssuer [1],
   * authorityCertSerialNumber [2]. */
  static const uint8_t key_id_tags[] = {0x80, 0xa1, 0x82};
  size_t previous = 0;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = tc_pki_value_open(&reader, value, 0x30, limits, 1);
  if (result != TC_TLV_OK)
    return result;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    size_t field;
    if (tc_pki_context_order(&element, key_id_tags, sizeof key_id_tags, &previous, &field) !=
        TC_TLV_OK)
      return TC_TLV_INVALID;
    if (field == 0) {
      parsed.has_key_identifier = 1;
      parsed.key_identifier = element.value;
    } else if (field == 1) {
      if (!element.value.length)
        return TC_TLV_INVALID;
      parsed.issuer = element.value;
    } else {
      result = TC_DER_integer_contents(element.value);
      if (result != TC_TLV_OK)
        return result;
      parsed.serial = element.value;
      parsed.serial_negative = (element.value.data[0] & 128) != 0;
    }
  }
  if (result != TC_TLV_END)
    return result;
  if (!!parsed.issuer.length != !!parsed.serial.length)
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_extensions_init(TC_TLV_reader* reader, TC_bytes encoded,
                                      const TC_TLV_limits* limits)
{
  if (!encoded.data && !encoded.length)
    return TC_TLV_reader_init(reader, (TC_bytes){NULL, 0}, TC_TLV_DER, limits);
  return tc_pki_value_open(reader, encoded, 0x30, limits, 0);
}

TC_TLV_result TC_X509_policy_mappings_init(TC_TLV_reader* reader, TC_bytes value,
                                           const TC_TLV_limits* limits)
{
  return tc_pki_value_open(reader, value, 0x30, limits, 0);
}

TC_TLV_result TC_X509_name_constraints_read(TC_bytes value, const TC_TLV_limits* limits,
                                            TC_X509_name_constraints* out)
{
  TC_X509_name_constraints parsed = {{NULL, 0}, {NULL, 0}};
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  static const uint8_t subtree_tags[] = {0xa0, 0xa1}; /* permitted, excluded */
  size_t previous = 0;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = tc_pki_value_open(&reader, value, 0x30, limits, 0);
  if (result != TC_TLV_OK)
    return result;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    size_t index;
    if (tc_pki_context_order(&element, subtree_tags, sizeof subtree_tags, &previous, &index) !=
            TC_TLV_OK ||
        !element.value.length)
      return TC_TLV_INVALID;
    if (index == 0)
      parsed.permitted = element.value;
    else
      parsed.excluded = element.value;
  }
  if (result != TC_TLV_END)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}

/* Read the next constructed SEQUENCE from reader and open its fields as a
 * child. next receives the advanced parent. */
static TC_TLV_result next_sequence(const TC_TLV_reader* reader, TC_TLV_reader* next,
                                   TC_TLV_reader* fields, TC_TLV_element* element)
{
  TC_TLV_result result;
  *next = *reader;
  result = TC_TLV_next(next, element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(element, 0x30) || !element->header.constructed)
    return TC_TLV_INVALID;
  return tc_pki_child_open(fields, next, element->value);
}

/* Read a required field inside a complete SEQUENCE. */
static TC_TLV_result required_field(TC_TLV_reader* fields, TC_TLV_element* element)
{
  TC_TLV_result result = TC_TLV_next(fields, element);
  return result == TC_TLV_END ? TC_TLV_INVALID : result;
}

TC_TLV_result TC_X509_policy_mapping_next(TC_TLV_reader* reader, TC_X509_policy_mapping* out)
{
  static const uint8_t any_policy[] = {0x55, 0x1d, TC_PKI_EXT_CERTIFICATE_POLICIES, 0};
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_bytes oids[2];
  TC_TLV_result result;
  unsigned i;
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  result = next_sequence(reader, &next, &fields, &element);
  if (result != TC_TLV_OK)
    return result;
  /* RFC 5280 section 4.2.1.5: issuerDomainPolicy, subjectDomainPolicy.
   * Neither may be anyPolicy. */
  for (i = 0; i < 2; ++i) {
    result = required_field(&fields, &element);
    if (result != TC_TLV_OK)
      return result;
    result = TC_DER_oid(element.encoded, &oids[i]);
    if (result != TC_TLV_OK)
      return result;
    if (oids[i].length == sizeof any_policy &&
        memcmp(oids[i].data, any_policy, sizeof any_policy) == 0)
      return TC_TLV_INVALID;
  }
  if (!tc_pki_end(&fields))
    return TC_TLV_INVALID;
  tc_pki_child_close(&next, &fields);
  out->issuer_policy = oids[0];
  out->subject_policy = oids[1];
  *reader = next;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_extension_next(TC_TLV_reader* reader, TC_X509_extension* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_X509_extension extension;
  TC_TLV_result result;
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  result = next_sequence(reader, &next, &fields, &element);
  if (result != TC_TLV_OK)
    return result;
  /* RFC 5280 section 4.1: extnID, critical BOOLEAN DEFAULT FALSE, extnValue. */
  result = required_field(&fields, &element);
  if (result != TC_TLV_OK)
    return result;
  if (TC_DER_oid(element.encoded, &extension.oid) != TC_TLV_OK)
    return TC_TLV_INVALID;
  extension.critical = 0;
  result = required_field(&fields, &element);
  if (result != TC_TLV_OK)
    return result;
  if (tc_pki_tag(&element, 1)) {
    /* DEFAULT FALSE is omitted in DER. */
    if (TC_DER_boolean(element.encoded, &extension.critical) != TC_TLV_OK || !extension.critical)
      return TC_TLV_INVALID;
    result = required_field(&fields, &element);
    if (result != TC_TLV_OK)
      return result;
  }
  if (!tc_pki_tag(&element, 4) || !tc_pki_end(&fields))
    return TC_TLV_INVALID;
  extension.value = element.value;
  tc_pki_child_close(&next, &fields);
  *reader = next;
  *out = extension;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_policies_init(TC_X509_policy_reader* reader, TC_bytes value,
                                    const TC_TLV_limits* limits, TC_bytes* seen, size_t capacity)
{
  TC_X509_policy_reader parsed;
  TC_TLV_result result;
  if (!reader || (!seen && capacity) || capacity > SIZE_MAX / sizeof *seen ||
      !tc_internal_ranges_disjoint(value.data, value.length, seen, capacity * sizeof *seen) ||
      !tc_internal_ranges_disjoint(reader, sizeof *reader, seen, capacity * sizeof *seen) ||
      !tc_internal_ranges_disjoint(value.data, value.length, reader, sizeof *reader))
    return TC_TLV_ARGUMENT;
  result = tc_pki_value_open(&parsed.reader, value, 0x30, limits, 0);
  if (result != TC_TLV_OK)
    return result;
  parsed.seen = seen;
  parsed.capacity = capacity;
  parsed.count = 0;
  *reader = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_policy_information_next(TC_TLV_reader* reader, TC_X509_policy* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_X509_policy policy = {{NULL, 0}, {NULL, 0}};
  TC_TLV_result result = next_sequence(reader, &next, &fields, &element);
  if (result != TC_TLV_OK)
    return result;
  /* RFC 5280 section 4.2.1.4: policyIdentifier, optional policyQualifiers. */
  result = required_field(&fields, &element);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_oid(element.encoded, &policy.oid);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_next(&fields, &element);
  if (result == TC_TLV_OK) {
    /* policyQualifiers SEQUENCE SIZE (1..MAX) OF PolicyQualifierInfo. */
    if (!tc_pki_tag(&element, 0x30) || !element.header.constructed || !element.value.length ||
        !tc_pki_end(&fields))
      return TC_TLV_INVALID;
    policy.qualifiers = element.encoded;
  } else if (result != TC_TLV_END)
    return result;
  tc_pki_child_close(&next, &fields);
  *reader = next;
  *out = policy;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_policy_next(TC_X509_policy_reader* reader, TC_X509_policy* out)
{
  TC_TLV_reader next;
  TC_X509_policy policy;
  TC_TLV_result result;
  size_t i;
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  if (reader->capacity > SIZE_MAX / sizeof *reader->seen ||
      !tc_internal_ranges_disjoint(out, sizeof *out, reader, sizeof *reader) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, reader->seen,
                                   reader->capacity * sizeof *reader->seen) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, reader->reader.input.data,
                                   reader->reader.input.length))
    return TC_TLV_ARGUMENT;
  next = reader->reader;
  if (tc_pki_end(&next))
    return TC_TLV_END;
  if (reader->count >= reader->capacity)
    return TC_TLV_LIMIT;
  result = tc_x509_policy_information_next(&next, &policy);
  if (result != TC_TLV_OK)
    return result;
  for (i = 0; i < reader->count; ++i)
    if (reader->seen[i].length == policy.oid.length &&
        memcmp(reader->seen[i].data, policy.oid.data, policy.oid.length) == 0)
      return TC_TLV_INVALID;
  reader->seen[reader->count++] = policy.oid;
  reader->reader = next;
  *out = policy;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_policy_qualifiers_init(TC_TLV_reader* reader, TC_bytes qualifiers,
                                             const TC_TLV_limits* limits)
{
  return TC_X509_extensions_init(reader, qualifiers, limits);
}

TC_TLV_result TC_X509_policy_qualifier_next(TC_TLV_reader* reader, TC_X509_policy_qualifier* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_X509_policy_qualifier qualifier;
  TC_TLV_result result;
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  result = next_sequence(reader, &next, &fields, &element);
  if (result != TC_TLV_OK)
    return result;
  /* RFC 5280 section 4.2.1.4 PolicyQualifierInfo: policyQualifierId and one
   * open-type qualifier. */
  result = required_field(&fields, &element);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_oid(element.encoded, &qualifier.oid);
  if (result != TC_TLV_OK)
    return result;
  result = required_field(&fields, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_end(&fields))
    return TC_TLV_INVALID;
  qualifier.value = element.encoded;
  tc_pki_child_close(&next, &fields);
  *reader = next;
  *out = qualifier;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_basic_constraints_read(TC_bytes value, const TC_TLV_limits* limits,
                                             TC_X509_basic_constraints* out)
{
  TC_X509_basic_constraints constraints = {0, 0, 0};
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out)
    return TC_TLV_ARGUMENT;
  /* RFC 5280 section 4.2.1.9: cA BOOLEAN DEFAULT FALSE, pathLenConstraint.
   * pathLenConstraint requires cA. */
  result = tc_pki_value_open(&reader, value, 0x30, limits, 1);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_next(&reader, &element);
  if (result == TC_TLV_OK && tc_pki_tag(&element, 1)) {
    if (TC_DER_boolean(element.encoded, &constraints.ca) != TC_TLV_OK || !constraints.ca)
      return TC_TLV_INVALID;
    result = TC_TLV_next(&reader, &element);
  }
  if (result == TC_TLV_OK) {
    if (!constraints.ca)
      return TC_TLV_INVALID;
    result = TC_DER_uint32(element.encoded, &constraints.path_length);
    if (result != TC_TLV_OK)
      return result;
    constraints.has_path_length = 1;
    result = TC_TLV_next(&reader, &element);
  }
  if (result != TC_TLV_END)
    return result == TC_TLV_OK ? TC_TLV_INVALID : result;
  *out = constraints;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_key_usage_read(TC_bytes value, const TC_TLV_limits* limits, uint16_t* out)
{
  enum { KEY_USAGE_BITS = 9 };
  TC_TLV_element element;
  TC_bytes bits;
  TC_TLV_result result;
  unsigned unused;
  if (!out || !limits)
    return TC_TLV_ARGUMENT;
  result = TC_TLV_read(value, TC_TLV_DER, limits, &element);
  if (result == TC_TLV_MORE)
    return TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result;
  if (!limits->max_elements)
    return TC_TLV_LIMIT;
  if (!tc_pki_tag(&element, 3) || element.encoded.length != value.length)
    return TC_TLV_INVALID;
  result = tc_der_bit_string_contents(element.value, &bits, &unused);
  if (result != TC_TLV_OK)
    return result;
  /* RFC 5280 section 4.2.1.3: at least one bit is set. */
  if (!bits.length)
    return TC_TLV_INVALID;
  return tc_pki_named_bits(bits, unused, KEY_USAGE_BITS, out);
}

TC_TLV_result TC_X509_extended_key_usage_read(TC_bytes value, const TC_TLV_limits* limits,
                                              TC_bytes* oids, size_t capacity, size_t* count)
{
  TC_bytes oid;
  TC_TLV_reader reader, start;
  TC_TLV_element element;
  TC_TLV_result result;
  size_t found = 0, i;
  if (!count || (!oids && capacity) || capacity > SIZE_MAX / sizeof *oids ||
      !tc_internal_ranges_disjoint(value.data, value.length, oids, capacity * sizeof *oids) ||
      !tc_internal_ranges_disjoint(value.data, value.length, count, sizeof *count) ||
      !tc_internal_ranges_disjoint(oids, capacity * sizeof *oids, count, sizeof *count))
    return TC_TLV_ARGUMENT;
  /* RFC 5280 section 4.2.1.12: SEQUENCE SIZE (1..MAX) OF KeyPurposeId. */
  result = tc_pki_value_open(&reader, value, 0x30, limits, 0);
  if (result != TC_TLV_OK)
    return result;
  start = reader;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    result = TC_DER_oid(element.encoded, &oid);
    if (result != TC_TLV_OK)
      return result;
    if (found == capacity)
      return TC_TLV_LIMIT;
    ++found;
  }
  if (result != TC_TLV_END)
    return result;
  /* Validate first so a malformed final OID leaves the caller's array intact. */
  for (i = 0; i < found; ++i) {
    (void)TC_TLV_next(&start, &element); /* The first pass accepted each element. */
    oids[i] = element.value;
  }
  *count = found;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_policy_constraints_read(TC_bytes value, const TC_TLV_limits* limits,
                                              TC_X509_policy_constraints* out)
{
  TC_X509_policy_constraints parsed = {0, 0, 0, 0};
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  /* RFC 5280 section 4.2.1.11: requireExplicitPolicy [0], inhibitPolicyMapping
   * [1], at least one present. */
  static const uint8_t skip_tags[] = {0x80, 0x81};
  size_t previous = 0;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = tc_pki_value_open(&reader, value, 0x30, limits, 0);
  if (result != TC_TLV_OK)
    return result;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    size_t index;
    uint32_t count;
    if (tc_pki_context_order(&element, skip_tags, sizeof skip_tags, &previous, &index) != TC_TLV_OK)
      return TC_TLV_INVALID;
    result = TC_DER_uint32_contents(element.value, &count);
    if (result != TC_TLV_OK)
      return result;
    if (index == 0) {
      parsed.has_require_explicit_policy = 1;
      parsed.require_explicit_policy = count;
    } else {
      parsed.has_inhibit_policy_mapping = 1;
      parsed.inhibit_policy_mapping = count;
    }
  }
  if (result != TC_TLV_END)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}
#endif
