/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_EXTENSIONS_INTERNAL_H_
#define TC_PKI_EXTENSIONS_INTERNAL_H_
#include "pki_internal.h"
#include "x509_path_internal.h"
#include "pki_tree_internal.h"
#include "pki_spans_internal.h"

static inline TC_TLV_result tc_pki_extensions_init(TC_TLV_reader* reader,
    const TC_X509_certificate* certificate, const TC_TLV_limits* limits, size_t* work)
{
  if (tc_x509_path_charge(work,certificate->extensions.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  return TC_X509_extensions_init(reader,certificate->extensions.data,certificate->extensions.length,limits);
}

static inline TC_TLV_result tc_pki_extension_next(TC_TLV_reader* reader,
    size_t* work, TC_X509_extension* extension)
{
  if (tc_pki_end(reader)) return TC_TLV_END;
  if (tc_x509_path_charge(work,1) != TC_TLV_OK) return TC_TLV_LIMIT;
  return TC_X509_extension_next(reader,extension);
}

/* Validate extension wrappers, embedded DER framing, and unique OIDs.
 * Per-extension semantics belong to the certificate/CRL policy layer.
 * OID scratch and all writable ranges must be disjoint from encoded data. */
typedef TC_TLV_result (*tc_pki_extension_check)(void* context, const TC_X509_extension* extension);

static inline TC_TLV_result tc_pki_extensions_visit(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_pki_extension_check check, void* context)
{
  TC_TLV_reader reader;
  TC_TLV_result result;
  size_t count = 0;
  if (!tree || !tree->work || (!oids && capacity)) return TC_TLV_ARGUMENT;
  if (!encoded.data && !encoded.length) return TC_TLV_OK;
  result = tc_pki_tree_open(encoded,0x30,TC_TLV_DER,limits,tree,&reader);
  if (result != TC_TLV_OK) return result;
  if (tc_pki_end(&reader)) return TC_TLV_INVALID;
  while (!tc_pki_end(&reader)) {
    TC_X509_extension extension;
    TC_TLV_element value;
    if (count == capacity) return TC_TLV_LIMIT;
    result = tc_pki_extension_next(&reader,tree->work,&extension);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_tree_read(extension.value,TC_TLV_DER,limits,tree,&value);
    if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
    if (value.encoded.length != extension.value.length) return TC_TLV_INVALID;
    if (check) {
      result = check(context,&extension);
      if (result != TC_TLV_OK) return result;
    }
    oids[count++] = extension.oid;
  }
  return tc_pki_spans_unique(oids,count,tree->work);
}

static inline TC_TLV_result tc_pki_extensions_check(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity)
{
  return tc_pki_extensions_visit(encoded,limits,tree,oids,capacity,NULL,NULL);
}

/* The standard certificate extensions use the 2.5.29 arc. */
static inline unsigned tc_pki_extension_id(const TC_X509_extension* extension)
{
  if (extension->oid.length != 3 || extension->oid.data[0] != 0x55 || extension->oid.data[1] != 0x1d)
    return 0;
  return extension->oid.data[2];
}
/* Borrow the certificate's SKI. A NULL span means absent; an empty OCTET
 * STRING remains distinguishable. Output changes only on OK; work is
 * provisional. Parsed inputs and output/work are disjoint. */
static inline TC_TLV_result tc_pki_subject_key_identifier(const TC_X509_certificate* certificate,
    const TC_TLV_limits* limits, size_t* work, TC_bytes* out)
{
  enum { SUBJECT_KEY_IDENTIFIER = 14 };
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_bytes identifier = {NULL,0};
  TC_TLV_result result;
  if (!certificate || !limits || !work || !out) return TC_TLV_ARGUMENT;
  result = tc_pki_extensions_init(&reader,certificate,limits,work);
  if (result != TC_TLV_OK) return result;
  while ((result = tc_pki_extension_next(&reader,work,&extension)) == TC_TLV_OK) {
    if (tc_pki_extension_id(&extension) != SUBJECT_KEY_IDENTIFIER) continue;
    if (identifier.data) return TC_TLV_INVALID;
    if (tc_x509_path_charge(work,extension.value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = TC_X509_subject_key_identifier_read(extension.value.data,extension.value.length,limits,&identifier);
    if (result != TC_TLV_OK) return result;
  }
  if (result != TC_TLV_END) return result;
  *out = identifier;
  return TC_TLV_OK;
}

/* Shared single-occurrence KeyUsage decoder for certificate policy passes. */
static inline TC_TLV_result tc_pki_key_usage_value(TC_bytes value,
    int* present, uint16_t* usage)
{
  uint16_t parsed;
  TC_TLV_result result;
  if (*present) return TC_TLV_INVALID;
  result = TC_X509_key_usage_read(value.data,value.length,&parsed);
  if (result != TC_TLV_OK) return result;
  *present = 1; *usage = parsed;
  return TC_TLV_OK;
}
#endif
