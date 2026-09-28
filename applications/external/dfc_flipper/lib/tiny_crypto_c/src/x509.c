/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509
#include <tiny_crypto/x509.h>
#include "pki_internal.h"
#include "pki_spans_internal.h"
#include "string_internal.h"
#include "x509_time_internal.h"
#include "internal.h"
#include "pki_storage_internal.h"
#include "hash_info_internal.h"
#include <string.h>

static const TC_TLV_limits unlimited = {SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX};

static TC_TLV_result open(TC_bytes contents, TC_TLV_reader* reader)
{ return TC_TLV_reader_init(reader, contents.data, contents.length, TC_TLV_DER, &unlimited); }

static TC_TLV_result field(TC_TLV_reader* reader, unsigned tag, TC_TLV_element* element)
{ return tc_pki_next(reader, tag, element) == TC_TLV_OK ? TC_TLV_OK : TC_TLV_INVALID; }

static int string_value(const TC_TLV_element* element)
{
  size_t offset = 0;
  uint32_t point;
  TC_TLV_result result;
  unsigned tag = element->header.tag[0];
  if (element->header.tag_length != 1) return 1;
  if (tag != 0x0c && tag != 0x13 && tag != 0x16 && tag != 0x1c && tag != 0x1e) return 1;
  do { result = tc_asn1_string_next(tag, element->value, &offset, &point); }
  while (result == TC_TLV_OK);
  return result == TC_TLV_END;
}

tc_x509_attribute_kind tc_x509_attribute_syntax(TC_bytes oid)
{
  static const uint8_t email[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,9,1};
  static const uint8_t pilot[] = {9,0x92,0x26,0x89,0x93,0xf2,0x2c,100,1};
  if (oid.length == 3 && oid.data[0] == 0x55 && oid.data[1] == 4) {
    switch (oid.data[2]) {
      case 3: case 4: case 7: case 8: case 9: case 10: case 11: case 12:
      case 41: case 42: case 43: case 44: case 65: return TC_X509_ATTRIBUTE_DIRECTORY;
      case 6: return TC_X509_ATTRIBUTE_COUNTRY;
      case 5: case 46: return TC_X509_ATTRIBUTE_PRINTABLE;
      default: break;
    }
  }
  if (oid.length == sizeof email && !memcmp(oid.data, email, sizeof email))
    return TC_X509_ATTRIBUTE_EMAIL;
  if (oid.length == sizeof pilot + 1 && !memcmp(oid.data, pilot, sizeof pilot)) {
    if (oid.data[sizeof pilot] == 1) return TC_X509_ATTRIBUTE_DIRECTORY;
    if (oid.data[sizeof pilot] == 25) return TC_X509_ATTRIBUTE_DOMAIN;
  }
  return TC_X509_ATTRIBUTE_UNKNOWN;
}

int tc_x509_attribute_type(TC_bytes oid, unsigned tag, size_t length)
{
  const tc_x509_attribute_kind syntax = tc_x509_attribute_syntax(oid);
  const int directory = syntax == TC_X509_ATTRIBUTE_DIRECTORY;
  const int printable = syntax == TC_X509_ATTRIBUTE_PRINTABLE || syntax == TC_X509_ATTRIBUTE_COUNTRY;
  const int ia5 = syntax == TC_X509_ATTRIBUTE_EMAIL || syntax == TC_X509_ATTRIBUTE_DOMAIN;
  if (syntax == TC_X509_ATTRIBUTE_COUNTRY && length != 2) return 0;
  if (directory || printable || ia5) {
    if (!length) return 0;
    if (directory && tag != 0x0c && tag != 0x13 && tag != 0x14 && tag != 0x1c && tag != 0x1e) return 0;
    if ((printable && tag != 0x13) || (ia5 && tag != 0x16)) return 0;
  }
  return 1;
}

int tc_x509_attribute_value(TC_bytes oid, const TC_TLV_element* element)
{
  const unsigned tag = element->header.tag_length == 1 ? element->header.tag[0] : 256;
  return tc_x509_attribute_type(oid,tag,element->value.length) && string_value(element);
}

TC_TLV_result TC_X509_name_init(TC_TLV_reader* reader, TC_bytes encoded,
    const TC_TLV_limits* bounds)
{
  TC_TLV_reader parsed;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!reader) return TC_TLV_ARGUMENT;
  result = TC_TLV_read(encoded.data, encoded.length, TC_TLV_DER, bounds, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x30) || element.encoded.length != encoded.length) return TC_TLV_INVALID;
  result = TC_TLV_reader_init(&parsed, element.value.data, element.value.length, TC_TLV_DER, bounds);
  if (result != TC_TLV_OK) return result;
  if (!parsed.limits.max_elements || !parsed.limits.max_depth) return TC_TLV_LIMIT;
  --parsed.limits.max_depth;
  parsed.elements = 1;
  *reader = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_attribute_next(TC_TLV_reader* reader, TC_X509_name_attribute* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_limits bounds;
  TC_TLV_element element;
  TC_X509_name_attribute attribute;
  TC_TLV_result result;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  if (reader->profile != TC_TLV_DER) return TC_TLV_ARGUMENT;
  next = *reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x30)) return TC_TLV_INVALID;
  attribute.encoded = element.encoded;
  bounds = next.limits;
  bounds.max_elements -= next.elements;
  if (!bounds.max_depth) return TC_TLV_LIMIT;
  --bounds.max_depth;
  result = TC_TLV_reader_init(&fields, element.value.data, element.value.length, TC_TLV_DER, &bounds);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_next(&fields, 6, &element);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  result = TC_DER_oid(element.encoded.data, element.encoded.length, &attribute.oid);
  if (result != TC_TLV_OK) return result;
  result = TC_TLV_next(&fields, &element);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (!tc_pki_end(&fields) || !tc_x509_attribute_value(attribute.oid, &element)) return TC_TLV_INVALID;
  attribute.value = element.encoded;
  next.elements += fields.elements;
  *reader = next;
  *out = attribute;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_rdn_contents(TC_TLV_reader* attributes)
{
  TC_bytes previous = {NULL,0};
  TC_X509_name_attribute attribute;
  TC_TLV_result result;
  if (!attributes || attributes->profile != TC_TLV_DER) return TC_TLV_ARGUMENT;
  if (tc_pki_end(attributes)) return TC_TLV_INVALID;
  while ((result = TC_X509_attribute_next(attributes,&attribute)) == TC_TLV_OK) {
    if (previous.data && tc_pki_compare(previous,attribute.encoded) > 0) return TC_TLV_INVALID;
    previous = attribute.encoded;
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

TC_TLV_result TC_X509_rdn_next(TC_TLV_reader* reader, TC_bytes* out)
{
  TC_TLV_reader next, attributes;
  TC_TLV_limits bounds;
  TC_TLV_element element;
  TC_bytes contents;
  TC_TLV_result result;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  if (reader->profile != TC_TLV_DER) return TC_TLV_ARGUMENT;
  next = *reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x31) || !element.value.length) return TC_TLV_INVALID;
  contents = element.value;
  bounds = next.limits;
  bounds.max_elements -= next.elements;
  if (!bounds.max_depth) return TC_TLV_LIMIT;
  --bounds.max_depth;
  result = TC_TLV_reader_init(&attributes, contents.data, contents.length, TC_TLV_DER, &bounds);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_rdn_contents(&attributes);
  if (result != TC_TLV_OK) return result;
  next.elements += attributes.elements;
  *reader = next;
  *out = contents;
  return TC_TLV_OK;
}

static TC_TLV_result name(TC_bytes contents, int allow_empty)
{
  TC_TLV_reader sequence;
  TC_bytes rdn;
  TC_TLV_result result;
  if ((!allow_empty && !contents.length) || open(contents, &sequence) != TC_TLV_OK) return TC_TLV_INVALID;
  while ((result = TC_X509_rdn_next(&sequence, &rdn)) == TC_TLV_OK) {}
  if (result != TC_TLV_END) return result;
  return TC_TLV_OK;
}

static TC_TLV_result general_name(const TC_TLV_element* element, int constraint)
{
  TC_bytes contents;
  TC_TLV_reader fields;
  TC_TLV_element value;
  unsigned tag;
  size_t i;
  if (element->header.tag_length != 1) return TC_TLV_INVALID;
  tag = element->header.tag[0];
  switch (tag) {
    case 0x81: case 0x82: case 0x86:
      if (!element->value.length) return TC_TLV_INVALID;
      for (i = 0; i < element->value.length; ++i)
        if (!element->value.data[i] || element->value.data[i] > 127 ||
            ((tag == 0x82 || tag == 0x86) && element->value.data[i] <= 32)) return TC_TLV_INVALID;
      break;
    case 0x87:
      if (element->value.length != (constraint ? 8u : 4u) &&
          element->value.length != (constraint ? 32u : 16u)) return TC_TLV_INVALID;
      break;
    case 0x88:
      if (TC_DER_oid_contents(element->value.data, element->value.length) != TC_TLV_OK) return TC_TLV_INVALID;
      break;
    case 0xa4:
      if (TC_DER_sequence(element->value.data, element->value.length, &contents) != TC_TLV_OK ||
          name(contents, constraint) != TC_TLV_OK) return TC_TLV_INVALID;
      break;
    case 0xa0:
      if (open(element->value, &fields) != TC_TLV_OK || field(&fields, 6, &value) != TC_TLV_OK ||
          TC_DER_oid(value.encoded.data, value.encoded.length, &contents) != TC_TLV_OK ||
          field(&fields, 0xa0, &value) != TC_TLV_OK || !tc_pki_end(&fields)) return TC_TLV_INVALID;
      contents = value.value;
      if (TC_TLV_read(contents.data, contents.length, TC_TLV_DER, &unlimited, &value) != TC_TLV_OK ||
          value.encoded.length != contents.length) return TC_TLV_INVALID;
      break;
    case 0xa3: case 0xa5:
      if (!element->value.length) return TC_TLV_INVALID;
      break;
    default: return TC_TLV_INVALID;
  }
  return TC_TLV_OK;
}

static TC_TLV_result general_names(TC_bytes encoded)
{
  TC_bytes contents;
  TC_TLV_reader reader;
  TC_TLV_element element;
  if (TC_DER_sequence(encoded.data, encoded.length, &contents) != TC_TLV_OK || !contents.length ||
      open(contents, &reader) != TC_TLV_OK) return TC_TLV_INVALID;
  while (!tc_pki_end(&reader))
    if (TC_TLV_next(&reader, &element) != TC_TLV_OK || general_name(&element, 0) != TC_TLV_OK)
      return TC_TLV_INVALID;
  return TC_TLV_OK;
}

static void count_node(void* user, const TC_TLV_event* event)
{
  size_t* remaining = (size_t*)user;
  if (event->kind == TC_TLV_BEGIN) --*remaining;
}

static TC_TLV_result next_tree(TC_TLV_reader* reader, TC_TLV_frame* frames,
    size_t capacity, TC_TLV_element* element)
{
  TC_TLV_reader next;
  TC_TLV_limits budget;
  TC_TLV_result result;
  size_t remaining;
  next = *reader;
  result = TC_TLV_next(&next, element);
  if (result != TC_TLV_OK) return result;
  budget = reader->limits;
  budget.max_elements -= reader->elements;
  remaining = budget.max_elements;
  result = TC_TLV_walk(element->encoded.data, element->encoded.length, TC_TLV_DER,
      &budget, frames, capacity, count_node, &remaining);
  if (result != TC_TLV_OK) return result;
  next.elements = reader->limits.max_elements - remaining;
  *reader = next;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_name_next(TC_TLV_reader* reader,
    TC_TLV_frame* frames, size_t capacity, TC_X509_general_name* out)
{
  TC_TLV_reader next;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  next = *reader;
  result = next_tree(&next, frames, capacity, &element);
  if (result != TC_TLV_OK) return result;
  result = general_name(&element, 0);
  if (result != TC_TLV_OK) return result;
  out->type = element.header.tag[0] & 31;
  out->encoded = element.encoded; out->value = element.value;
  *reader = next;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_subtree_next(TC_TLV_reader* reader,
    TC_TLV_frame* frames, size_t capacity, TC_X509_general_subtree* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_X509_general_subtree parsed = {{0,{NULL,0},{NULL,0}},0,0,0};
  TC_TLV_result result;
  unsigned previous = 0;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  next = *reader;
  result = next_tree(&next, frames, capacity, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x30) || open(element.value, &fields) != TC_TLV_OK ||
      TC_TLV_next(&fields, &element) != TC_TLV_OK || general_name(&element, 1) != TC_TLV_OK)
    return TC_TLV_INVALID;
  parsed.base.type = element.header.tag[0] & 31;
  parsed.base.encoded = element.encoded; parsed.base.value = element.value;
  while ((result = TC_TLV_next(&fields, &element)) == TC_TLV_OK) {
    unsigned tag = element.header.tag[0];
    uint32_t distance;
    if (element.header.tag_length != 1 || (tag != 0x80 && tag != 0x81) ||
        tag <= previous) return TC_TLV_INVALID;
    result = TC_DER_uint32_contents(element.value.data, element.value.length, &distance);
    if (result != TC_TLV_OK) return result;
    if (tag == 0x80) {
      if (!distance) return TC_TLV_INVALID; /* DER omits DEFAULT zero. */
      parsed.minimum = distance;
    } else {
      parsed.has_maximum = 1; parsed.maximum = distance;
    }
    previous = tag;
  }
  if (result != TC_TLV_END) return result;
  *reader = next; *out = parsed;
  return TC_TLV_OK;
}

static TC_TLV_result extensions(TC_bytes encoded, TC_X509_workspace* workspace,
                                TC_TLV_limits* budget, int* critical_san)
{
  TC_bytes contents;
  TC_TLV_reader reader;
  TC_X509_extension extension;
  size_t count = 0;
  TC_TLV_result result = TC_DER_sequence(encoded.data, encoded.length, &contents);
  if (result != TC_TLV_OK || !contents.length) return TC_TLV_INVALID;
  if (open(contents, &reader) != TC_TLV_OK) return TC_TLV_INVALID;
  while (!tc_pki_end(&reader)) {
    TC_TLV_element embedded;
    if (count == workspace->extension_capacity) return TC_TLV_LIMIT;
    result = TC_X509_extension_next(&reader, &extension);
    if (result != TC_TLV_OK) return result;
    result = TC_TLV_read(extension.value.data, extension.value.length, TC_TLV_DER, budget, &embedded);
    if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
    if (embedded.encoded.length != extension.value.length) return TC_TLV_INVALID;
    result = TC_TLV_walk(extension.value.data, extension.value.length, TC_TLV_DER, budget,
                         workspace->frames, workspace->frame_capacity, count_node, &budget->max_elements);
    if (result != TC_TLV_OK) return result;
    if (extension.oid.length == 3 && extension.oid.data[0] == 0x55 && extension.oid.data[1] == 0x1d) {
      if (extension.oid.data[2] == 19) {
        TC_X509_basic_constraints constraints;
        result = TC_X509_basic_constraints_read(extension.value.data, extension.value.length, &constraints);
        if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
      } else if (extension.oid.data[2] == 15) {
        uint16_t usage;
        result = TC_X509_key_usage_read(extension.value.data, extension.value.length, &usage);
        if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
      } else if (extension.oid.data[2] == 17 || extension.oid.data[2] == 18) {
        if (general_names(extension.value) != TC_TLV_OK) return TC_TLV_INVALID;
        if (extension.oid.data[2] == 17 && extension.critical) *critical_san = 1;
      }
    }
    workspace->extension_oids[count++] = extension.oid;
  }
  return tc_pki_spans_unique(workspace->extension_oids,count,NULL);
}

static TC_TLV_result validity(TC_bytes contents, TC_X509_certificate* certificate)
{
  TC_TLV_reader reader;
  TC_TLV_element element;
  if (open(contents, &reader) != TC_TLV_OK || TC_TLV_next(&reader, &element) != TC_TLV_OK ||
      tc_x509_time_value(&element, &certificate->not_before) != TC_TLV_OK ||
      TC_TLV_next(&reader, &element) != TC_TLV_OK ||
      tc_x509_time_value(&element, &certificate->not_after) != TC_TLV_OK || !tc_pki_end(&reader)) return TC_TLV_INVALID;
  return TC_TLV_OK;
}

static TC_TLV_result signature_format(const TC_X509_certificate* certificate)
{
  static const uint8_t ecdsa[] = {0x2a,0x86,0x48,0xce,0x3d,4};
  static const uint8_t dsa[] = {0x2a,0x86,0x48,0xce,0x38,4,3};
  static const uint8_t rsa[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1};
  TC_bytes oid = certificate->signature_algorithm.oid;
  TC_bytes parameters = certificate->signature_algorithm.parameters;
  if (oid.length == 9 && !memcmp(oid.data, rsa, sizeof rsa) && oid.data[8] == 10)
    return tc_x509_pss_parameters(parameters);
  int pair = (oid.length == 7 && !memcmp(oid.data, dsa, sizeof dsa)) ||
    ((oid.length == 7 || oid.length == 8) && !memcmp(oid.data, ecdsa, sizeof ecdsa) &&
     ((oid.length == 7 && oid.data[6] == 1) ||
      (oid.length == 8 && oid.data[6] == 3 && oid.data[7] >= 1 && oid.data[7] <= 4)));
  if (pair) {
    TC_DER_signature_pair value;
    if (parameters.length) return TC_TLV_INVALID;
    return TC_DER_ecdsa_signature(certificate->signature.data, certificate->signature.length, &value);
  }
  if (oid.length == 3 && oid.data[0] == 0x2b && oid.data[1] == 0x65 &&
      (oid.data[2] == 112 || oid.data[2] == 113)) {
    if (parameters.length || certificate->signature.length != (oid.data[2] == 112 ? 64u : 114u)) return TC_TLV_INVALID;
  } else if (oid.length == 9 && !memcmp(oid.data, rsa, sizeof rsa) &&
             (oid.data[8] == 2 || oid.data[8] == 4 || oid.data[8] == 5 ||
              (oid.data[8] >= 11 && oid.data[8] <= 16))) {
    /* RFC 4055 requires readers to accept absent RSA signature parameters. */
    if (parameters.length && TC_DER_null(parameters.data, parameters.length) != TC_TLV_OK) return TC_TLV_INVALID;
  }
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_read(const uint8_t* data, size_t length,
                           const TC_TLV_limits* limits, TC_X509_workspace* workspace,
                           TC_X509_certificate* out)
{
  TC_X509_certificate certificate;
  TC_TLV_element element;
  TC_TLV_reader outer, tbs;
  TC_bytes inner_algorithm;
  TC_TLV_result result;
  TC_TLV_limits budget;
  uint32_t version;
  unsigned unused, last = 0;
  int empty_subject, critical_san = 0;
  if (!out || !workspace || (!workspace->extension_oids && workspace->extension_capacity)) return TC_TLV_ARGUMENT;
  result = TC_TLV_read(data, length, TC_TLV_DER, limits, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x30) || element.encoded.length != length) return TC_TLV_INVALID;
  budget = *limits;
  result = TC_TLV_walk(data, length, TC_TLV_DER, limits, workspace->frames, workspace->frame_capacity,
                       count_node, &budget.max_elements);
  if (result != TC_TLV_OK) return result;
  memset(&certificate, 0, sizeof certificate);
  certificate.encoded = element.encoded;
  if (open(element.value, &outer) != TC_TLV_OK || field(&outer, 0x30, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  certificate.tbs = element.encoded;
  if (open(element.value, &tbs) != TC_TLV_OK || TC_TLV_next(&tbs, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  certificate.version = 1;
  if (tc_pki_tag(&element, 0xa0)) {
    if (TC_DER_uint32(element.value.data, element.value.length, &version) != TC_TLV_OK ||
        version == 0 || version > 2) return TC_TLV_INVALID;
    certificate.version = version + 1;
    if (TC_TLV_next(&tbs, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  }
  if (TC_DER_integer(element.encoded.data, element.encoded.length, &certificate.serial,
                     &certificate.serial_negative) != TC_TLV_OK) return TC_TLV_INVALID;
  if (field(&tbs, 0x30, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  inner_algorithm = element.encoded;
  if (field(&tbs, 0x30, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  certificate.issuer = element.encoded;
  if (name(element.value, 0) != TC_TLV_OK) return TC_TLV_INVALID;
  if (field(&tbs, 0x30, &element) != TC_TLV_OK || validity(element.value, &certificate) != TC_TLV_OK) return TC_TLV_INVALID;
  if (field(&tbs, 0x30, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  certificate.subject = element.encoded;
  empty_subject = !element.value.length;
  if (name(element.value, 1) != TC_TLV_OK) return TC_TLV_INVALID;
  if (field(&tbs, 0x30, &element) != TC_TLV_OK) return TC_TLV_INVALID;
  certificate.spki = element.encoded;
  result = TC_X509_subject_public_key(element.encoded.data, element.encoded.length, &certificate.public_key);
  if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  while (!tc_pki_end(&tbs)) {
    unsigned tag;
    if (TC_TLV_next(&tbs, &element) != TC_TLV_OK || element.header.tag_length != 1) return TC_TLV_INVALID;
    tag = element.header.tag[0];
    if (tag == 0x81 || tag == 0x82) {
      if (certificate.version < 2 || last >= tag || !element.value.length) return TC_TLV_INVALID;
      unused = element.value.data[0];
      if (unused > 7 || (element.value.length == 1 && unused) ||
          (unused && (element.value.data[element.value.length - 1] & ((1u << unused) - 1)))) return TC_TLV_INVALID;
    } else if (tag == 0xa3) {
      if (certificate.version != 3 || last >= tag) return TC_TLV_INVALID;
      certificate.extensions = element.value;
      result = extensions(element.value, workspace, &budget, &critical_san);
      if (result != TC_TLV_OK) return result;
    } else return TC_TLV_INVALID;
    last = tag;
  }
  if (empty_subject && !critical_san) return TC_TLV_INVALID;
  if (field(&outer, 0x30, &element) != TC_TLV_OK || tc_pki_compare(inner_algorithm, element.encoded) ||
      TC_DER_algorithm_identifier(element.encoded.data, element.encoded.length, &certificate.signature_algorithm) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (field(&outer, 3, &element) != TC_TLV_OK || !tc_pki_end(&outer) ||
      TC_DER_bit_string(element.encoded.data, element.encoded.length, &certificate.signature, &unused) != TC_TLV_OK ||
      unused || !certificate.signature.length) return TC_TLV_INVALID;
  result = signature_format(&certificate);
  if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  *out = certificate;
  return TC_TLV_OK;
}
static TC_X509_signature_result signature_callback_result(TC_X509_signature_result result,
    size_t available, size_t* work)
{
  if (*work > available) { *work = 0; return TC_X509_SIGNATURE_ERROR; }
  switch (result) {
    case TC_X509_SIGNATURE_VALID: case TC_X509_SIGNATURE_INVALID:
    case TC_X509_SIGNATURE_UNSUPPORTED: case TC_X509_SIGNATURE_ERROR:
    case TC_X509_SIGNATURE_LIMIT: return result;
    default: return TC_X509_SIGNATURE_ERROR;
  }
}

TC_X509_signature_result TC_X509_signature_verify_digest(TC_bytes digest,
    const TC_signature_algorithm* algorithm, TC_bytes signature,
    const TC_X509_public_key* issuer_key, const TC_X509_signature_provider* provider,
    size_t* work)
{
  enum { INPUT_COUNT = 11 };
  TC_bytes inputs[INPUT_COUNT], output;
  tc_hash_info hash;
  size_t checks = INPUT_COUNT, available;
  TC_X509_signature_result result;
  if (!algorithm || !issuer_key || !work) return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify_digest) return TC_X509_SIGNATURE_UNSUPPORTED;
  if (tc_pki_storage_span(work,1,sizeof *work,&output) != TC_TLV_OK ||
      tc_pki_storage_span(algorithm,1,sizeof *algorithm,&inputs[0]) != TC_TLV_OK ||
      tc_pki_storage_span(issuer_key,1,sizeof *issuer_key,&inputs[1]) != TC_TLV_OK ||
      tc_pki_storage_span(provider,1,sizeof *provider,&inputs[2]) != TC_TLV_OK)
    return TC_X509_SIGNATURE_ERROR;
  inputs[3] = digest; inputs[4] = signature;
  inputs[5] = issuer_key->algorithm.oid; inputs[6] = issuer_key->algorithm.parameters;
  inputs[7] = issuer_key->key; inputs[8] = issuer_key->modulus;
  inputs[9] = issuer_key->exponent; inputs[10] = issuer_key->curve_oid;
  for (size_t i = 0; i < INPUT_COUNT; ++i)
    if (tc_pki_storage_input(&output,1,inputs[i],&checks) != TC_TLV_OK)
      return TC_X509_SIGNATURE_ERROR;
  if (!signature.length || !issuer_key->key.length || !issuer_key->algorithm.oid.length)
    return TC_X509_SIGNATURE_ERROR;
  if (!tc_hash_info_get(algorithm->hash,&hash)) return TC_X509_SIGNATURE_UNSUPPORTED;
  if (digest.length != hash.digest_length) return TC_X509_SIGNATURE_ERROR;
  if (*work < INPUT_COUNT) { *work = 0; return TC_X509_SIGNATURE_LIMIT; }
  *work -= INPUT_COUNT;
  for (size_t i = 3; i < INPUT_COUNT; ++i) {
    if (*work < inputs[i].length) { *work = 0; return TC_X509_SIGNATURE_LIMIT; }
    *work -= inputs[i].length;
  }
  available = *work;
  result = provider->verify_digest(provider->context,digest,algorithm,signature,issuer_key,work);
  return signature_callback_result(result,available,work);
}

TC_X509_signature_result TC_X509_signature_verify_message(const TC_bytes* message, size_t count,
    const TC_DER_algorithm* algorithm, TC_bytes signature, const TC_X509_public_key* issuer_key,
    const TC_X509_signature_provider* provider, size_t* work)
{
  TC_bytes inputs[13];
  size_t i, available;
  TC_X509_signature_result result;
  if (!algorithm || !issuer_key || !work || (count && !message) ||
      count > SIZE_MAX / sizeof(*message)) return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify) return TC_X509_SIGNATURE_UNSUPPORTED;
  inputs[0] = signature; inputs[1] = algorithm->oid; inputs[2] = algorithm->parameters;
  inputs[3] = issuer_key->algorithm.oid; inputs[4] = issuer_key->algorithm.parameters;
  inputs[5] = issuer_key->key; inputs[6] = issuer_key->modulus; inputs[7] = issuer_key->exponent;
  inputs[8] = issuer_key->curve_oid;
  inputs[9] = (TC_bytes){(const uint8_t*)algorithm,sizeof(*algorithm)};
  inputs[10] = (TC_bytes){(const uint8_t*)issuer_key,sizeof(*issuer_key)};
  inputs[11] = (TC_bytes){(const uint8_t*)provider,sizeof(*provider)};
  inputs[12] = (TC_bytes){(const uint8_t*)message,count * sizeof(*message)};
  if (!signature.length || !algorithm->oid.length || !issuer_key->algorithm.oid.length ||
      !issuer_key->key.length) return TC_X509_SIGNATURE_ERROR;
  for (i = 0; i < 13; ++i)
    if ((!inputs[i].data && inputs[i].length) ||
        !tc_internal_ranges_disjoint(work,sizeof(*work),inputs[i].data,inputs[i].length))
      return TC_X509_SIGNATURE_ERROR;
  /* Check every segment before writing work; a later segment may alias it. */
  if (count > *work) return TC_X509_SIGNATURE_LIMIT;
  for (i = 0; i < count; ++i)
    if ((!message[i].data && message[i].length) ||
        !tc_internal_ranges_disjoint(work,sizeof(*work),message[i].data,message[i].length))
      return TC_X509_SIGNATURE_ERROR;
  *work -= count;
  for (i = 0; i < count; ++i) {
    if (*work < message[i].length) { *work = 0; return TC_X509_SIGNATURE_LIMIT; }
    *work -= message[i].length;
  }
  for (i = 0; i < 6; ++i) {
    if (*work < inputs[i].length) { *work = 0; return TC_X509_SIGNATURE_LIMIT; }
    *work -= inputs[i].length;
  }
  available = *work;
  result = provider->verify(provider->context,message,count,algorithm,signature,issuer_key,work);
  return signature_callback_result(result,available,work);
}

TC_X509_signature_result TC_X509_signature_verify(const TC_X509_certificate* certificate,
    const TC_X509_public_key* issuer_key, const TC_X509_signature_provider* provider, size_t* work)
{
  if (!certificate || !issuer_key || !work) return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify) return TC_X509_SIGNATURE_UNSUPPORTED;
  if (!certificate->tbs.length ||
      !tc_internal_ranges_disjoint(work,sizeof(*work),certificate,sizeof(*certificate)) ||
      !tc_internal_ranges_disjoint(work,sizeof(*work),certificate->encoded.data,certificate->encoded.length))
    return TC_X509_SIGNATURE_ERROR;
  return TC_X509_signature_verify_message(&certificate->tbs,1,&certificate->signature_algorithm,
      certificate->signature,issuer_key,provider,work);
}
#endif
