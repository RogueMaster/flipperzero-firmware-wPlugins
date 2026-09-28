/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_TREE_INTERNAL_H_
#define TC_PKI_TREE_INTERNAL_H_
#include "pki_internal.h"
#include <tiny_crypto/x509.h>
#include "pki_string_internal.h"

typedef struct tc_pki_tree_workspace {
  TC_TLV_frame* frames;
  size_t capacity;
  size_t* work;
} tc_pki_tree_workspace;

/* Bound the scan before parsing. Successful reads charge only their object;
 * failed reads consume the reserved allowance. Inputs and scratch are disjoint. */
static inline TC_TLV_result tc_pki_tree_read(TC_bytes input, TC_TLV_profile profile,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* workspace, TC_TLV_element* out)
{
  TC_TLV_element parsed;
  TC_TLV_result result;
  size_t allowance;
  if (!workspace || !workspace->work || !out) return TC_TLV_ARGUMENT;
  allowance = input.length < *workspace->work ? input.length : *workspace->work;
  *workspace->work -= allowance;
  result = TC_TLV_read_tree(input.data,allowance,profile,limits,
      workspace->frames,workspace->capacity,&parsed);
  if (result == TC_TLV_MORE && allowance < input.length) return TC_TLV_LIMIT;
  if (result != TC_TLV_OK) return result;
  *workspace->work += allowance - parsed.encoded.length;
  *out = parsed;
  return TC_TLV_OK;
}

static inline TC_TLV_result tc_pki_tree_next(TC_TLV_reader* reader,
    const tc_pki_tree_workspace* workspace, TC_TLV_element* out)
{
  TC_TLV_element parsed;
  TC_TLV_result result;
  if (!reader || !out || reader->offset > reader->input.length ||
      (reader->input.length && !reader->input.data)) return TC_TLV_ARGUMENT;
  if (tc_pki_end(reader)) return TC_TLV_END;
  if (reader->elements >= reader->limits.max_elements) return TC_TLV_LIMIT;
  result = tc_pki_tree_read((TC_bytes){reader->input.data + reader->offset,
      reader->input.length - reader->offset},reader->profile,&reader->limits,workspace,&parsed);
  if (result != TC_TLV_OK) return result;
  reader->offset += parsed.encoded.length;
  ++reader->elements;
  *out = parsed;
  return TC_TLV_OK;
}

static inline TC_TLV_result tc_pki_tree_field(TC_TLV_reader* reader, unsigned tag,
    const tc_pki_tree_workspace* workspace, TC_TLV_element* out)
{
  TC_TLV_result result = tc_pki_tree_next(reader,workspace,out);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  return tc_pki_tag(out,tag) ? TC_TLV_OK : TC_TLV_INVALID;
}

static inline TC_TLV_result tc_pki_tree_open(TC_bytes input, unsigned tag,
    TC_TLV_profile profile, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* workspace, TC_TLV_reader* out)
{
  TC_TLV_element outer;
  TC_TLV_result result = tc_pki_tree_read(input,profile,limits,workspace,&outer);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&outer,tag) || !outer.header.constructed || outer.encoded.length != input.length)
    return TC_TLV_INVALID;
  return TC_TLV_reader_init(out,outer.value.data,outer.value.length,profile,limits);
}

typedef struct {
  TC_bytes oid, value;
} tc_pki_oid_value;

/* OID followed by at most one encoded value. The schema decides whether the
 * value is required and how to interpret it. */
static inline TC_TLV_result tc_pki_tree_oid_value(TC_bytes input, unsigned tag,
    TC_TLV_profile profile, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* workspace, tc_pki_oid_value* out)
{
  TC_TLV_reader fields;
  TC_TLV_element element;
  tc_pki_oid_value parsed = {{NULL,0},{NULL,0}};
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_tree_open(input,tag,profile,limits,workspace,&fields);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_field(&fields,6,workspace,&element);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_oid_contents(element.value.data,element.value.length);
  if (result != TC_TLV_OK) return result;
  parsed.oid = element.value;
  if (!tc_pki_end(&fields)) {
    result = tc_pki_tree_next(&fields,workspace,&element);
    if (result != TC_TLV_OK) return result;
    if (!tc_pki_end(&fields)) return TC_TLV_INVALID;
    parsed.value = element.encoded;
  }
  *out = parsed;
  return TC_TLV_OK;
}

/* AlgorithmIdentifier parameters are optional. */
static inline TC_TLV_result tc_pki_tree_algorithm(TC_bytes input,
    TC_TLV_profile profile, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* workspace, TC_DER_algorithm* out)
{
  tc_pki_oid_value parsed;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = tc_pki_tree_oid_value(input,0x30,profile,limits,workspace,&parsed);
  if (result != TC_TLV_OK) return result;
  *out = (TC_DER_algorithm){parsed.oid,parsed.value};
  return TC_TLV_OK;
}
/* Read one attribute, retaining its encoded value for matching. */
static inline TC_TLV_result tc_pki_tree_attribute(TC_TLV_reader* reader,
    const tc_pki_tree_workspace* workspace, TC_X509_name_attribute* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element attribute, oid, value;
  TC_X509_name_attribute parsed;
  TC_TLV_result result;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  if (reader->profile != TC_TLV_DER && reader->profile != TC_TLV_BER) return TC_TLV_ARGUMENT;
  next = *reader;
  result = tc_pki_tree_next(&next,workspace,&attribute);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&attribute,0x30)) return TC_TLV_INVALID;
  result = TC_TLV_reader_init(&fields,attribute.value.data,attribute.value.length,
      reader->profile,&reader->limits);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_field(&fields,6,workspace,&oid);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_oid_contents(oid.value.data,oid.value.length);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_next(&fields,workspace,&value);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (!tc_pki_end(&fields)) return TC_TLV_INVALID;
  if (value.header.constructed && tc_x509_attribute_syntax(oid.value) != TC_X509_ATTRIBUTE_UNKNOWN) {
    size_t length;
    if (value.header.tag_length != 1) return TC_TLV_INVALID;
    const unsigned tag = value.header.tag[0] & ~0x20u;
    result = tc_pki_string_walk(value.encoded,tag,reader->profile,&reader->limits,
        workspace->frames,workspace->capacity,workspace->work,NULL,NULL,&length);
    if (result != TC_TLV_OK) return result;
    if (!tc_x509_attribute_type(oid.value,tag,length)) return TC_TLV_INVALID;
  } else if (!tc_x509_attribute_value(oid.value,&value)) return TC_TLV_INVALID;
  parsed.encoded = attribute.encoded;
  parsed.oid = oid.value;
  parsed.value = value.encoded;
  *reader = next;
  *out = parsed;
  return TC_TLV_OK;
}

/* Name ::= SEQUENCE OF SET OF AttributeTypeAndValue. BER permits unsorted sets. */
static inline TC_TLV_result tc_pki_tree_name(TC_bytes input,
    TC_TLV_profile profile, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* workspace)
{
  TC_TLV_reader rdns, attributes;
  TC_TLV_element rdn;
  TC_X509_name_attribute attribute;
  TC_TLV_result result;
  if (profile != TC_TLV_DER && profile != TC_TLV_BER) return TC_TLV_ARGUMENT;
  result = tc_pki_tree_open(input,0x30,profile,limits,workspace,&rdns);
  if (result != TC_TLV_OK) return result;
  while (!tc_pki_end(&rdns)) {
    TC_bytes previous = {NULL,0};
    result = tc_pki_tree_field(&rdns,0x31,workspace,&rdn);
    if (result != TC_TLV_OK) return result;
    if (!rdn.value.length) return TC_TLV_INVALID;
    result = TC_TLV_reader_init(&attributes,rdn.value.data,rdn.value.length,profile,limits);
    if (result != TC_TLV_OK) return result;
    while (!tc_pki_end(&attributes)) {
      result = tc_pki_tree_attribute(&attributes,workspace,&attribute);
      if (result != TC_TLV_OK) return result;
      if (profile == TC_TLV_DER && previous.data &&
          tc_pki_compare(previous,attribute.encoded) > 0) return TC_TLV_INVALID;
      previous = attribute.encoded;
    }
  }
  return TC_TLV_OK;
}

/* Compare complete Names with independent framing profiles. Scratch is shared
 * across the two inputs; all writable storage must be disjoint.
 * matched changes only on OK. */
TC_TLV_result tc_pki_name_equal(TC_bytes left, TC_TLV_profile left_profile,
    TC_bytes right, TC_TLV_profile right_profile, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* workspace, const tc_pki_tree_workspace* tree,
    int* matched);
/* Compare DER Names with optional appended RDN contents, without concatenation.
 * Empty suffix spans mean no appended RDN. Storage/output rules match above. */
TC_TLV_result tc_pki_name_appended_equal(TC_bytes left, TC_bytes left_rdn,
    TC_bytes right, TC_bytes right_rdn, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* workspace, const tc_pki_tree_workspace* tree,
    int* matched);
#endif
