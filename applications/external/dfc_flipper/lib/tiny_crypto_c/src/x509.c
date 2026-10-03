/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509
#include <tiny_crypto/x509.h>
#include "pki_internal.h"
#include "pki_spans_internal.h"
#include "pki_extensions_internal.h"
#include "pki_names_internal.h"
#include "string_internal.h"
#include "x509_time_internal.h"
#include "internal.h"
#include "pki_storage_internal.h"
#include "pki_reader_internal.h"
#include "hash_info_internal.h"
#include "pki_signature_oid_internal.h"
#include "der_bits_internal.h"
#include <string.h>

static const TC_TLV_limits unlimited = {SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX};

static TC_TLV_result open(TC_bytes contents, TC_TLV_reader* reader)
{
  return TC_TLV_reader_init(reader, contents, TC_TLV_DER, &unlimited);
}

static int string_value(const TC_TLV_element* element)
{
  size_t offset = 0;
  uint32_t point;
  TC_TLV_result result;
  unsigned tag = element->header.tag[0];
  if (element->header.tag_length != 1)
    return 1;
  if (tag != 0x0c && tag != 0x13 && tag != 0x16 && tag != 0x1c && tag != 0x1e)
    return 1;
  do {
    result = tc_asn1_string_next(tag, element->value, &offset, &point);
  } while (result == TC_TLV_OK);
  return result == TC_TLV_END;
}

tc_x509_attribute_kind tc_x509_attribute_syntax(TC_bytes oid)
{
  static const uint8_t email[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 9, 1};
  static const uint8_t pilot[] = {9, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 100, 1};
  if (oid.length == 3 && oid.data[0] == 0x55 && oid.data[1] == 4) {
    switch (oid.data[2]) {
    case 3:
    case 4:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 41:
    case 42:
    case 43:
    case 44:
    case 65:
      return TC_X509_ATTRIBUTE_DIRECTORY;
    case 6:
      return TC_X509_ATTRIBUTE_COUNTRY;
    case 5:
    case 46:
      return TC_X509_ATTRIBUTE_PRINTABLE;
    default:
      break;
    }
  }
  if (oid.length == sizeof email && !memcmp(oid.data, email, sizeof email))
    return TC_X509_ATTRIBUTE_EMAIL;
  if (oid.length == sizeof pilot + 1 && !memcmp(oid.data, pilot, sizeof pilot)) {
    if (oid.data[sizeof pilot] == 1)
      return TC_X509_ATTRIBUTE_DIRECTORY;
    if (oid.data[sizeof pilot] == 25)
      return TC_X509_ATTRIBUTE_DOMAIN;
  }
  return TC_X509_ATTRIBUTE_UNKNOWN;
}

int tc_x509_attribute_type(TC_bytes oid, unsigned tag, size_t length)
{
  const tc_x509_attribute_kind syntax = tc_x509_attribute_syntax(oid);
  const int directory = syntax == TC_X509_ATTRIBUTE_DIRECTORY;
  const int printable =
      syntax == TC_X509_ATTRIBUTE_PRINTABLE || syntax == TC_X509_ATTRIBUTE_COUNTRY;
  const int ia5 = syntax == TC_X509_ATTRIBUTE_EMAIL || syntax == TC_X509_ATTRIBUTE_DOMAIN;
  if (syntax == TC_X509_ATTRIBUTE_COUNTRY && length != 2)
    return 0;
  if (directory || printable || ia5) {
    if (!length)
      return 0;
    if (directory && tag != 0x0c && tag != 0x13 && tag != 0x14 && tag != 0x1c && tag != 0x1e)
      return 0;
    if ((printable && tag != 0x13) || (ia5 && tag != 0x16))
      return 0;
  }
  return 1;
}

int tc_x509_attribute_value(TC_bytes oid, const TC_TLV_element* element)
{
  const unsigned tag = element->header.tag_length == 1 ? element->header.tag[0] : 256;
  return tc_x509_attribute_type(oid, tag, element->value.length) && string_value(element);
}

TC_TLV_result TC_X509_name_init(TC_TLV_reader* reader, TC_bytes encoded,
                                const TC_TLV_limits* bounds)
{
  TC_TLV_reader parsed;
  TC_TLV_element element;
  TC_TLV_result result;
  /* The returned reader is rewritten while it reads encoded. */
  if (!reader || !tc_internal_ranges_disjoint(reader, sizeof *reader, encoded.data, encoded.length))
    return TC_TLV_ARGUMENT;
  result = TC_TLV_read(encoded, TC_TLV_DER, bounds, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, 0x30) || element.encoded.length != encoded.length)
    return TC_TLV_INVALID;
  result = TC_TLV_reader_init(&parsed, element.value, TC_TLV_DER, bounds);
  if (result != TC_TLV_OK)
    return result;
  if (!parsed.limits.max_elements || !parsed.limits.max_depth)
    return TC_TLV_LIMIT;
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
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  if (reader->profile != TC_TLV_DER)
    return TC_TLV_ARGUMENT;
  next = *reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, 0x30))
    return TC_TLV_INVALID;
  attribute.encoded = element.encoded;
  bounds = next.limits;
  bounds.max_elements -= next.elements;
  if (!bounds.max_depth)
    return TC_TLV_LIMIT;
  --bounds.max_depth;
  result = TC_TLV_reader_init(&fields, element.value, TC_TLV_DER, &bounds);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_next(&fields, 6, &element);
  if (result != TC_TLV_OK)
    return result == TC_TLV_END ? TC_TLV_INVALID : result;
  result = TC_DER_oid(element.encoded, &attribute.oid);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_next(&fields, &element);
  if (result != TC_TLV_OK)
    return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (!tc_pki_end(&fields) || !tc_x509_attribute_value(attribute.oid, &element))
    return TC_TLV_INVALID;
  attribute.value = element.encoded;
  next.elements += fields.elements;
  *reader = next;
  *out = attribute;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_rdn_contents(TC_TLV_reader* attributes)
{
  TC_bytes previous = {NULL, 0};
  TC_X509_name_attribute attribute;
  TC_TLV_result result;
  if (!attributes || attributes->profile != TC_TLV_DER)
    return TC_TLV_ARGUMENT;
  if (tc_pki_end(attributes))
    return TC_TLV_INVALID;
  while ((result = TC_X509_attribute_next(attributes, &attribute)) == TC_TLV_OK) {
    if (previous.data && tc_pki_compare(previous, attribute.encoded) > 0)
      return TC_TLV_INVALID;
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
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  if (reader->profile != TC_TLV_DER)
    return TC_TLV_ARGUMENT;
  next = *reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, 0x31) || !element.value.length)
    return TC_TLV_INVALID;
  contents = element.value;
  bounds = next.limits;
  bounds.max_elements -= next.elements;
  if (!bounds.max_depth)
    return TC_TLV_LIMIT;
  --bounds.max_depth;
  result = TC_TLV_reader_init(&attributes, contents, TC_TLV_DER, &bounds);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_rdn_contents(&attributes);
  if (result != TC_TLV_OK)
    return result;
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
  if ((!allow_empty && !contents.length) || open(contents, &sequence) != TC_TLV_OK)
    return TC_TLV_INVALID;
  while ((result = TC_X509_rdn_next(&sequence, &rdn)) == TC_TLV_OK) {
  }
  if (result != TC_TLV_END)
    return result;
  return TC_TLV_OK;
}

static TC_TLV_result general_name(const TC_TLV_element* element, int constraint)
{
  TC_bytes contents;
  TC_TLV_reader fields;
  TC_TLV_element value;
  unsigned tag;
  size_t i;
  if (element->header.tag_length != 1)
    return TC_TLV_INVALID;
  tag = element->header.tag[0];
  switch (tag) {
  case 0x81:
  case 0x82:
  case 0x86:
    if (!element->value.length)
      return TC_TLV_INVALID;
    for (i = 0; i < element->value.length; ++i)
      if (!element->value.data[i] || element->value.data[i] > 127 ||
          ((tag == 0x82 || tag == 0x86) && element->value.data[i] <= 32))
        return TC_TLV_INVALID;
    break;
  case 0x87:
    if (element->value.length != (constraint ? 8u : 4u) &&
        element->value.length != (constraint ? 32u : 16u))
      return TC_TLV_INVALID;
    break;
  case 0x88:
    if (TC_DER_oid_contents(element->value) != TC_TLV_OK)
      return TC_TLV_INVALID;
    break;
  case 0xa4:
    if (TC_DER_sequence(element->value, &contents) != TC_TLV_OK ||
        name(contents, constraint) != TC_TLV_OK)
      return TC_TLV_INVALID;
    break;
  case 0xa0:
    if (open(element->value, &fields) != TC_TLV_OK ||
        tc_pki_field(&fields, 6, &value) != TC_TLV_OK ||
        TC_DER_oid(value.encoded, &contents) != TC_TLV_OK ||
        tc_pki_field(&fields, 0xa0, &value) != TC_TLV_OK || !tc_pki_end(&fields))
      return TC_TLV_INVALID;
    contents = value.value;
    if (TC_TLV_read(contents, TC_TLV_DER, &unlimited, &value) != TC_TLV_OK ||
        value.encoded.length != contents.length)
      return TC_TLV_INVALID;
    break;
  case 0xa3:
  case 0xa5:
    if (!element->value.length)
      return TC_TLV_INVALID;
    break;
  default:
    return TC_TLV_INVALID;
  }
  return TC_TLV_OK;
}

/* Certificate parsing is bounded by its element budget, so the name scan is
 * unmetered. */
static TC_TLV_result general_names(TC_bytes encoded, const TC_X509_workspace* workspace)
{
  size_t work = SIZE_MAX;
  const tc_pki_tree_workspace tree = {workspace->frames.data, workspace->frames.capacity, &work};
  TC_bytes contents;
  if (TC_DER_sequence(encoded, &contents) != TC_TLV_OK)
    return TC_TLV_INVALID;
  return tc_pki_general_names_contents_check(contents, &unlimited, &tree);
}

static void count_node(void* user, const TC_TLV_event* event)
{
  size_t* remaining = (size_t*)user;
  if (event->kind == TC_TLV_BEGIN)
    --*remaining;
}

/* Read the next element and walk its complete tree under the reader's
 * remaining element budget. The walk counts the element and everything
 * beneath it. */
static TC_TLV_result next_tree(TC_TLV_reader* reader, TC_TLV_frames frames, TC_TLV_element* element)
{
  TC_TLV_reader next;
  TC_TLV_limits budget;
  TC_TLV_result result;
  size_t remaining;
  next = *reader;
  result = TC_TLV_next(&next, element);
  if (result != TC_TLV_OK)
    return result;
  budget = reader->limits;
  budget.max_elements -= reader->elements;
  remaining = budget.max_elements;
  result = TC_TLV_walk(element->encoded, TC_TLV_DER, &budget, frames, count_node, &remaining);
  if (result != TC_TLV_OK)
    return result;
  next.elements = reader->limits.max_elements - remaining;
  *reader = next;
  return TC_TLV_OK;
}

/* A names reader binds input and frames at init. The reader struct, input and
 * frames must be pairwise disjoint because next writes the reader and frames
 * while reading input. */
static int names_reader_storage_valid(const void* reader, size_t reader_size, TC_bytes input,
                                      TC_TLV_frames frames)
{
  size_t frames_size;
  if (!reader || (!frames.data && frames.capacity) ||
      frames.capacity > SIZE_MAX / sizeof *frames.data)
    return 0;
  frames_size = frames.capacity * sizeof *frames.data;
  return tc_internal_ranges_disjoint(reader, reader_size, input.data, input.length) &&
         tc_internal_ranges_disjoint(reader, reader_size, frames.data, frames_size) &&
         tc_internal_ranges_disjoint(input.data, input.length, frames.data, frames_size);
}

static TC_TLV_result names_contents_open(TC_X509_general_names_reader* parsed, TC_bytes contents,
                                         const TC_TLV_limits* limits, TC_TLV_frames frames)
{
  TC_TLV_result result = TC_TLV_reader_init(&parsed->reader, contents, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  parsed->reader.root = 0;
  parsed->frames = frames;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_names_init(TC_X509_general_names_reader* reader, TC_bytes encoded,
                                         const TC_TLV_limits* limits, TC_TLV_frames frames)
{
  TC_X509_general_names_reader parsed;
  TC_TLV_result result;
  if (!names_reader_storage_valid(reader, sizeof *reader, encoded, frames))
    return TC_TLV_ARGUMENT;
  /* RFC 5280 section 4.2.1.6: GeneralNames ::= SEQUENCE SIZE (1..MAX). */
  result = tc_pki_value_open(&parsed.reader, encoded, 0x30, limits, 0);
  if (result != TC_TLV_OK)
    return result;
  parsed.frames = frames;
  *reader = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_names_contents_init(TC_X509_general_names_reader* reader,
                                                  TC_bytes contents, const TC_TLV_limits* limits,
                                                  TC_TLV_frames frames)
{
  TC_X509_general_names_reader parsed;
  TC_TLV_result result;
  if (!names_reader_storage_valid(reader, sizeof *reader, contents, frames))
    return TC_TLV_ARGUMENT;
  result = names_contents_open(&parsed, contents, limits, frames);
  if (result != TC_TLV_OK)
    return result;
  *reader = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_name_next(TC_X509_general_names_reader* reader,
                                        TC_X509_general_name* out)
{
  TC_TLV_reader next;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  next = reader->reader;
  result = next_tree(&next, reader->frames, &element);
  if (result != TC_TLV_OK)
    return result;
  result = general_name(&element, 0);
  if (result != TC_TLV_OK)
    return result;
  out->type = element.header.tag[0] & 31;
  out->encoded = element.encoded;
  out->value = element.value;
  reader->reader = next;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_subtrees_init(TC_X509_general_subtrees_reader* reader,
                                            TC_bytes contents, const TC_TLV_limits* limits,
                                            TC_TLV_frames frames)
{
  TC_X509_general_names_reader names;
  TC_TLV_result result;
  if (!names_reader_storage_valid(reader, sizeof *reader, contents, frames))
    return TC_TLV_ARGUMENT;
  result = names_contents_open(&names, contents, limits, frames);
  if (result != TC_TLV_OK)
    return result;
  reader->reader = names.reader;
  reader->frames = names.frames;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_subtree_next(TC_X509_general_subtrees_reader* reader,
                                           TC_X509_general_subtree* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  TC_X509_general_subtree parsed = {{0, {NULL, 0}, {NULL, 0}}, 0, 0, 0};
  TC_TLV_result result;
  static const uint8_t distance_tags[] = {0x80, 0x81}; /* minimum, maximum */
  size_t previous = 0, index;
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  next = reader->reader;
  result = next_tree(&next, reader->frames, &element);
  if (result != TC_TLV_OK)
    return result;
  /* next_tree bounded the subtree, so its fields need no further budget. */
  if (!tc_pki_tag(&element, 0x30) || open(element.value, &fields) != TC_TLV_OK ||
      TC_TLV_next(&fields, &element) != TC_TLV_OK || general_name(&element, 1) != TC_TLV_OK)
    return TC_TLV_INVALID;
  parsed.base.type = element.header.tag[0] & 31;
  parsed.base.encoded = element.encoded;
  parsed.base.value = element.value;
  while ((result = TC_TLV_next(&fields, &element)) == TC_TLV_OK) {
    uint32_t distance;
    if (tc_pki_context_order(&element, distance_tags, sizeof distance_tags, &previous, &index) !=
        TC_TLV_OK)
      return TC_TLV_INVALID;
    result = TC_DER_uint32_contents(element.value, &distance);
    if (result != TC_TLV_OK)
      return result;
    if (index == 0) {
      if (!distance)
        return TC_TLV_INVALID; /* DER omits DEFAULT zero. */
      parsed.minimum = distance;
    } else {
      parsed.has_maximum = 1;
      parsed.maximum = distance;
    }
  }
  if (result != TC_TLV_END)
    return result;
  reader->reader = next;
  *out = parsed;
  return TC_TLV_OK;
}

static TC_TLV_result extensions(TC_bytes encoded, const TC_X509_workspace* workspace,
                                TC_TLV_limits* budget, int* critical_san)
{
  TC_bytes contents;
  TC_TLV_reader reader;
  TC_X509_extension extension;
  size_t count = 0;
  TC_TLV_result result = TC_DER_sequence(encoded, &contents);
  if (result != TC_TLV_OK || !contents.length)
    return TC_TLV_INVALID;
  if (open(contents, &reader) != TC_TLV_OK)
    return TC_TLV_INVALID;
  while (!tc_pki_end(&reader)) {
    TC_TLV_element embedded;
    if (count == workspace->extension_capacity)
      return TC_TLV_LIMIT;
    result = TC_X509_extension_next(&reader, &extension);
    if (result != TC_TLV_OK)
      return result;
    result = TC_TLV_read(extension.value, TC_TLV_DER, budget, &embedded);
    if (result != TC_TLV_OK)
      return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
    if (embedded.encoded.length != extension.value.length)
      return TC_TLV_INVALID;
    result = TC_TLV_walk(extension.value, TC_TLV_DER, budget, workspace->frames, count_node,
                         &budget->max_elements);
    if (result != TC_TLV_OK)
      return result;
    {
      /* The walk above charged these values to budget, so their decoders run
       * unlimited. */
      const unsigned id = tc_pki_extension_id(&extension);
      if (id == TC_PKI_EXT_BASIC_CONSTRAINTS) {
        TC_X509_basic_constraints constraints;
        result = TC_X509_basic_constraints_read(extension.value, &unlimited, &constraints);
        if (result != TC_TLV_OK)
          return result;
      } else if (id == TC_PKI_EXT_KEY_USAGE) {
        uint16_t usage;
        result = TC_X509_key_usage_read(extension.value, &unlimited, &usage);
        if (result != TC_TLV_OK)
          return result;
      } else if (id == TC_PKI_EXT_SUBJECT_ALT_NAME || id == TC_PKI_EXT_ISSUER_ALT_NAME) {
        result = general_names(extension.value, workspace);
        if (result != TC_TLV_OK)
          return result;
        if (id == TC_PKI_EXT_SUBJECT_ALT_NAME && extension.critical)
          *critical_san = 1;
      }
    }
    workspace->extension_oids[count++] = extension.oid;
  }
  return tc_pki_spans_unique(workspace->extension_oids, count, NULL);
}

static TC_TLV_result validity(TC_bytes contents, TC_X509_certificate* certificate)
{
  TC_TLV_reader reader;
  TC_TLV_element element;
  if (open(contents, &reader) != TC_TLV_OK || TC_TLV_next(&reader, &element) != TC_TLV_OK ||
      tc_x509_time_value(&element, &certificate->not_before) != TC_TLV_OK ||
      TC_TLV_next(&reader, &element) != TC_TLV_OK ||
      tc_x509_time_value(&element, &certificate->not_after) != TC_TLV_OK || !tc_pki_end(&reader))
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

static TC_TLV_result signature_format(const TC_X509_certificate* certificate)
{
  tc_pki_signature_oid_info info =
      tc_pki_signature_oid_classify(certificate->signature_algorithm.oid);
  TC_bytes parameters = certificate->signature_algorithm.parameters;
  TC_DER_signature_pair value;
  if (info.kind == TC_PKI_SIGNATURE_RSA_PSS)
    return tc_x509_pss_parameters(parameters);
  if (tc_pki_signature_parameters_check(info.kind, parameters, TC_TLV_DER) != TC_TLV_OK)
    return TC_TLV_INVALID;
  switch (info.kind) {
  case TC_PKI_SIGNATURE_ECDSA:
  case TC_PKI_SIGNATURE_DSA:
    return TC_DER_ecdsa_signature(certificate->signature, &value);
  case TC_PKI_SIGNATURE_ED25519:
    return certificate->signature.length == 64 ? TC_TLV_OK : TC_TLV_INVALID;
  case TC_PKI_SIGNATURE_ED448:
    return certificate->signature.length == 114 ? TC_TLV_OK : TC_TLV_INVALID;
  default:
    return TC_TLV_OK;
  }
}

/* TBSCertificate fields after the SEQUENCE header (RFC 5280 section 4.1).
 * budget holds the elements left after the caller's walk. Sub-readers are
 * unlimited because that walk bounded depth and element count. */
static TC_TLV_result tbs_fields(TC_bytes contents, const TC_X509_workspace* workspace,
                                TC_TLV_limits* budget, TC_X509_certificate* certificate)
{
  /* issuerUniqueID [1], subjectUniqueID [2], extensions [3]. */
  static const uint8_t optional_tags[] = {0x81, 0x82, 0xa3};
  size_t previous = 0, index;
  TC_TLV_element element;
  TC_TLV_reader tbs;
  TC_TLV_result result;
  uint32_t version;
  unsigned unused;
  int critical_san = 0;
  if (open(contents, &tbs) != TC_TLV_OK || TC_TLV_next(&tbs, &element) != TC_TLV_OK)
    return TC_TLV_INVALID;
  certificate->version = 1;
  if (tc_pki_tag(&element, 0xa0)) {
    /* DER omits the DEFAULT v1 value. */
    if (TC_DER_uint32(element.value, &version) != TC_TLV_OK || version == 0 || version > 2)
      return TC_TLV_INVALID;
    certificate->version = version + 1;
    if (TC_TLV_next(&tbs, &element) != TC_TLV_OK)
      return TC_TLV_INVALID;
  }
  if (TC_DER_integer(element.encoded, &certificate->serial, &certificate->serial_negative) !=
      TC_TLV_OK)
    return TC_TLV_INVALID;
  if (tc_pki_field(&tbs, 0x30, &element) != TC_TLV_OK ||
      TC_DER_algorithm_identifier(element.encoded, &certificate->signature_algorithm) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (tc_pki_field(&tbs, 0x30, &element) != TC_TLV_OK || name(element.value, 0) != TC_TLV_OK)
    return TC_TLV_INVALID;
  certificate->issuer = element.encoded;
  if (tc_pki_field(&tbs, 0x30, &element) != TC_TLV_OK ||
      validity(element.value, certificate) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (tc_pki_field(&tbs, 0x30, &element) != TC_TLV_OK || name(element.value, 1) != TC_TLV_OK)
    return TC_TLV_INVALID;
  certificate->subject = element.encoded;
  if (tc_pki_field(&tbs, 0x30, &element) != TC_TLV_OK)
    return TC_TLV_INVALID;
  certificate->spki = element.encoded;
  result = TC_X509_subject_public_key(element.encoded, &certificate->public_key);
  if (result != TC_TLV_OK)
    return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  while (!tc_pki_end(&tbs)) {
    if (TC_TLV_next(&tbs, &element) != TC_TLV_OK ||
        tc_pki_context_order(&element, optional_tags, sizeof optional_tags, &previous, &index) !=
            TC_TLV_OK)
      return TC_TLV_INVALID;
    if (index < 2) {
      TC_bytes unique_id;
      /* Unique identifiers require v2 or v3. */
      if (certificate->version < 2 ||
          tc_der_bit_string_contents(element.value, &unique_id, &unused) != TC_TLV_OK)
        return TC_TLV_INVALID;
    } else {
      if (certificate->version != 3)
        return TC_TLV_INVALID;
      certificate->extensions = element.value;
      result = extensions(element.value, workspace, budget, &critical_san);
      if (result != TC_TLV_OK)
        return result;
    }
  }
  /* RFC 5280 section 4.1.2.6: an empty subject requires a critical SAN. */
  if (certificate->subject.length == 2 && !critical_san)
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

/* The certificate readers write out, the frames and the extension OID slots
 * while they read encoded, the limits and the workspace struct. Every range
 * must be disjoint from the others. */
static TC_TLV_result read_storage(TC_bytes encoded, const TC_TLV_limits* limits,
                                  const TC_X509_workspace* workspace,
                                  const TC_X509_certificate* out)
{
  size_t used;
  return tc_pki_reader_workspace_check(encoded, limits, workspace, NULL, out, sizeof *out, &used);
}

/* Read one constructed element with the given tag, walk its tree to bound
 * depth and elements, and return its contents with the remaining budget. */
static TC_TLV_result read_bounded(TC_bytes encoded, unsigned tag, const TC_TLV_limits* limits,
                                  const TC_X509_workspace* workspace, TC_TLV_limits* budget,
                                  TC_TLV_element* out)
{
  TC_TLV_result result = TC_TLV_read(encoded, TC_TLV_DER, limits, out);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(out, tag) || !out->header.constructed || out->encoded.length != encoded.length)
    return TC_TLV_INVALID;
  *budget = *limits;
  return TC_TLV_walk(encoded, TC_TLV_DER, limits, workspace->frames, count_node,
                     &budget->max_elements);
}

TC_TLV_result tc_x509_tbs_read(TC_bytes encoded, const TC_TLV_limits* limits,
                               const TC_X509_workspace* workspace, TC_X509_certificate* out)
{
  TC_X509_certificate certificate = {0};
  TC_TLV_element element;
  TC_TLV_limits budget;
  TC_TLV_result result = read_storage(encoded, limits, workspace, out);
  if (result != TC_TLV_OK)
    return result;
  result = read_bounded(encoded, 0x30, limits, workspace, &budget, &element);
  if (result != TC_TLV_OK)
    return result;
  certificate.tbs = element.encoded;
  result = tbs_fields(element.value, workspace, &budget, &certificate);
  if (result != TC_TLV_OK)
    return result;
  *out = certificate;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_certificate_read(TC_bytes encoded, unsigned tag, const TC_TLV_limits* limits,
                                       const TC_X509_workspace* workspace, TC_X509_certificate* out)
{
  TC_X509_certificate certificate = {0};
  TC_DER_algorithm outer_algorithm;
  TC_TLV_element element;
  TC_TLV_reader outer;
  TC_TLV_limits budget;
  TC_TLV_result result;
  unsigned unused;
  result = read_storage(encoded, limits, workspace, out);
  if (result != TC_TLV_OK)
    return result;
  result = read_bounded(encoded, tag, limits, workspace, &budget, &element);
  if (result != TC_TLV_OK)
    return result;
  certificate.encoded = element.encoded;
  if (open(element.value, &outer) != TC_TLV_OK || tc_pki_field(&outer, 0x30, &element) != TC_TLV_OK)
    return TC_TLV_INVALID;
  certificate.tbs = element.encoded;
  result = tbs_fields(element.value, workspace, &budget, &certificate);
  if (result != TC_TLV_OK)
    return result;
  /* RFC 5280 section 4.1.1.2: signatureAlgorithm matches the TBS signature
   * field. Equal DER fields have equal encodings. */
  if (tc_pki_field(&outer, 0x30, &element) != TC_TLV_OK ||
      TC_DER_algorithm_identifier(element.encoded, &outer_algorithm) != TC_TLV_OK ||
      !tc_pki_equal(outer_algorithm.oid, certificate.signature_algorithm.oid) ||
      !tc_pki_equal(outer_algorithm.parameters, certificate.signature_algorithm.parameters))
    return TC_TLV_INVALID;
  if (tc_pki_field(&outer, 3, &element) != TC_TLV_OK || !tc_pki_end(&outer) ||
      TC_DER_bit_string(element.encoded, &certificate.signature, &unused) != TC_TLV_OK || unused ||
      !certificate.signature.length)
    return TC_TLV_INVALID;
  result = signature_format(&certificate);
  if (result != TC_TLV_OK)
    return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  *out = certificate;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_read(TC_bytes encoded, const TC_TLV_limits* limits,
                           const TC_X509_workspace* workspace, TC_X509_certificate* out)
{
  return tc_x509_certificate_read(encoded, 0x30, limits, workspace, out);
}

static TC_X509_signature_result signature_callback_result(TC_X509_signature_result result,
                                                          size_t available, size_t* work)
{
  if (*work > available) {
    *work = 0;
    return TC_X509_SIGNATURE_ERROR;
  }
  switch (result) {
  case TC_X509_SIGNATURE_VALID:
  case TC_X509_SIGNATURE_INVALID:
  case TC_X509_SIGNATURE_UNSUPPORTED:
  case TC_X509_SIGNATURE_ERROR:
  case TC_X509_SIGNATURE_LIMIT:
    return result;
  default:
    return TC_X509_SIGNATURE_ERROR;
  }
}

/* Shared preflight for both verification forms. Every input range, the
 * signed data included, must be disjoint from work. Work is charged one unit
 * plus the length of each signed-data span and each charged field. The key's
 * modulus, exponent and curve_oid are sub-spans of key and are only checked
 * for overlap. LIMIT sets *work to 0. */
static TC_X509_signature_result signature_prepare(const TC_bytes* data, size_t data_count,
                                                  const TC_bytes* objects, size_t object_count,
                                                  const TC_bytes* charged, size_t charged_count,
                                                  const TC_X509_public_key* issuer_key,
                                                  size_t* work)
{
  const TC_bytes key_parts[] = {issuer_key->modulus, issuer_key->exponent, issuer_key->curve_oid};
  const TC_bytes* groups[] = {data, charged};
  const size_t counts[] = {data_count, charged_count};
  tc_pki_storage_plan plan;
  TC_bytes output;
  if (data_count > SIZE_MAX / sizeof *data)
    return TC_X509_SIGNATURE_ERROR;
  tc_pki_storage_plan_begin(&plan, &output, 1, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  tc_pki_storage_plan_seal(&plan);
  tc_pki_storage_plan_input_spans(&plan, objects, object_count);
  TC_PKI_PLAN_INPUT(&plan, data, data_count);
  tc_pki_storage_plan_input_spans(&plan, data, data_count);
  tc_pki_storage_plan_input_spans(&plan, charged, charged_count);
  tc_pki_storage_plan_input_spans(&plan, key_parts, sizeof key_parts / sizeof *key_parts);
  if (tc_pki_storage_plan_finish(&plan, NULL) != TC_TLV_OK)
    return TC_X509_SIGNATURE_ERROR;
  for (size_t group = 0; group < 2; ++group)
    for (size_t i = 0; i < counts[group]; ++i) {
      const size_t length = groups[group][i].length;
      if (*work < 1 || *work - 1 < length) {
        *work = 0;
        return TC_X509_SIGNATURE_LIMIT;
      }
      *work -= 1 + length;
    }
  return TC_X509_SIGNATURE_VALID;
}

TC_X509_signature_result
TC_X509_signature_verify_digest(TC_bytes digest, const TC_signature_algorithm* algorithm,
                                TC_bytes signature, const TC_X509_public_key* issuer_key,
                                const TC_X509_signature_provider* provider, size_t* work)
{
  tc_hash_info hash;
  size_t available;
  TC_X509_signature_result result;
  if (!algorithm || !issuer_key || !work)
    return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify_digest)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  const TC_bytes objects[] = {{(const uint8_t*)algorithm, sizeof *algorithm},
                              {(const uint8_t*)issuer_key, sizeof *issuer_key},
                              {(const uint8_t*)provider, sizeof *provider}};
  const TC_bytes charged[] = {signature, issuer_key->algorithm.oid,
                              issuer_key->algorithm.parameters, issuer_key->key};
  if (!signature.length || !issuer_key->key.length || !issuer_key->algorithm.oid.length)
    return TC_X509_SIGNATURE_ERROR;
  if (!tc_hash_info_get(algorithm->hash, &hash))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (digest.length != hash.digest_length)
    return TC_X509_SIGNATURE_ERROR;
  result = signature_prepare(&digest, 1, objects, sizeof objects / sizeof *objects, charged,
                             sizeof charged / sizeof *charged, issuer_key, work);
  if (result != TC_X509_SIGNATURE_VALID)
    return result;
  available = *work;
  result =
      provider->verify_digest(provider->context, digest, algorithm, signature, issuer_key, work);
  return signature_callback_result(result, available, work);
}

TC_X509_signature_result TC_X509_signature_verify_message(
    const TC_bytes* message, size_t count, const TC_DER_algorithm* algorithm, TC_bytes signature,
    const TC_X509_public_key* issuer_key, const TC_X509_signature_provider* provider, size_t* work)
{
  size_t available;
  TC_X509_signature_result result;
  if (!algorithm || !issuer_key || !work || (count && !message))
    return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  const TC_bytes objects[] = {{(const uint8_t*)algorithm, sizeof *algorithm},
                              {(const uint8_t*)issuer_key, sizeof *issuer_key},
                              {(const uint8_t*)provider, sizeof *provider}};
  const TC_bytes charged[] = {signature,
                              algorithm->oid,
                              algorithm->parameters,
                              issuer_key->algorithm.oid,
                              issuer_key->algorithm.parameters,
                              issuer_key->key};
  if (!signature.length || !algorithm->oid.length || !issuer_key->algorithm.oid.length ||
      !issuer_key->key.length)
    return TC_X509_SIGNATURE_ERROR;
  result = signature_prepare(message, count, objects, sizeof objects / sizeof *objects, charged,
                             sizeof charged / sizeof *charged, issuer_key, work);
  if (result != TC_X509_SIGNATURE_VALID)
    return result;
  available = *work;
  result =
      provider->verify(provider->context, message, count, algorithm, signature, issuer_key, work);
  return signature_callback_result(result, available, work);
}

TC_X509_signature_result TC_X509_signature_verify(const TC_X509_certificate* certificate,
                                                  const TC_X509_public_key* issuer_key,
                                                  const TC_X509_signature_provider* provider,
                                                  size_t* work)
{
  if (!certificate || !issuer_key || !work)
    return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (!certificate->tbs.length ||
      !tc_internal_ranges_disjoint(work, sizeof(*work), certificate, sizeof(*certificate)) ||
      !tc_internal_ranges_disjoint(work, sizeof(*work), certificate->encoded.data,
                                   certificate->encoded.length))
    return TC_X509_SIGNATURE_ERROR;
  return TC_X509_signature_verify_message(&certificate->tbs, 1, &certificate->signature_algorithm,
                                          certificate->signature, issuer_key, provider, work);
}
#endif
