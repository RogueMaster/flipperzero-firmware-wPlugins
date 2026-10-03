/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * X.509 CRL parsing (RFC 5280 section 5): TBSCertList fields, revoked-entry
 * iteration and the CRL index. */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_crl_internal.h"
#include "x509_time_internal.h"
#include "pki_extensions_internal.h"
#include "pki_reader_internal.h"
#include "x509_crl_source_internal.h"
#include "hash_dispatch_internal.h"

TC_TLV_result tc_x509_crl_content_equal(const TC_X509_crl* left, const TC_X509_crl* right,
                                        size_t* work, int* equal)
{
  if (!left || !right || !work || !equal)
    return TC_TLV_ARGUMENT;
  int order;
  TC_TLV_result result;
  if (!left->prepared && !right->prepared) {
    result = tc_pki_span_compare(left->tbs, right->tbs, work, &order);
  } else {
    if (!left->prepared) {
      const TC_X509_crl* swap = left;
      left = right;
      right = swap;
    }
    const TC_X509_crl_prepared* prepared = left->prepared;
    tc_hash_info info;
    if (!tc_hash_info_get(prepared->hash, &info))
      return TC_TLV_UNSUPPORTED;
    if (!prepared->digest.data || prepared->digest.length != info.digest_length)
      return TC_TLV_ARGUMENT;
    TC_bytes digest;
    enum { MAX_DIGEST_BYTES = 64 };
    uint8_t computed[MAX_DIGEST_BYTES];
    if (right->prepared) {
      /* Different retained hashes cannot establish content equality. */
      if (right->prepared->hash != prepared->hash)
        return TC_TLV_UNSUPPORTED;
      digest = right->prepared->digest;
      if (!digest.data || digest.length != info.digest_length)
        return TC_TLV_ARGUMENT;
    } else {
      if (!tc_hash_available(prepared->hash))
        return TC_TLV_UNSUPPORTED;
      result = tc_pki_work_charge(work, right->tbs.length);
      if (result != TC_TLV_OK)
        return result;
      TC_hash_context hash;
      if (tc_hash_digest_parts(prepared->hash, &right->tbs, 1, computed, &hash) != TC_OK)
        return TC_TLV_ARGUMENT;
      digest = (TC_bytes){computed, info.digest_length};
    }
    result = tc_pki_span_compare(prepared->digest, digest, work, &order);
  }
  if (result == TC_TLV_OK)
    *equal = order == 0;
  return result;
}

TC_TLV_result TC_X509_crl_read(TC_bytes encoded, const TC_TLV_limits* limits, TC_TLV_frames frames,
                               size_t* work, TC_X509_crl* out)
{
  TC_TLV_result result = tc_pki_reader_storage(encoded, limits, frames, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
  return tc_x509_crl_read(encoded, limits, &tree, out);
}

TC_TLV_result TC_X509_crl_extensions_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                          const TC_X509_workspace* workspace, size_t* work,
                                          TC_X509_crl_extensions* out)
{
  TC_TLV_result result =
      tc_pki_reader_workspace_storage(encoded, limits, workspace, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  const tc_pki_tree_workspace tree = {workspace->frames.data, workspace->frames.capacity, work};
  return tc_x509_crl_extension_info_read(encoded, limits, &tree, workspace->extension_oids,
                                         workspace->extension_capacity, out);
}

TC_TLV_result tc_x509_crl_record_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                      const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                      size_t oid_capacity, TC_X509_crl_record* record)
{
  if (!record)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_crl_read(encoded, limits, tree, &record->crl);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_extension_info_read(record->crl.extensions, limits, tree, oids, oid_capacity,
                                           &record->extensions);
  if (result != TC_TLV_OK)
    return result;
  record->policy = tc_x509_crl_extension_policy(&record->extensions);
  return record->policy == TC_TLV_OK || record->policy == TC_TLV_INVALID ||
                 record->policy == TC_TLV_UNSUPPORTED
             ? TC_TLV_OK
             : record->policy;
}

TC_TLV_result TC_X509_crl_index_init(const TC_bytes* encoded, size_t count,
                                     const TC_TLV_limits* limits,
                                     const TC_X509_workspace* workspace, size_t* work,
                                     TC_X509_crl_record* records, size_t capacity,
                                     TC_X509_crl_index* out)
{
  TC_bytes writes[5];
  tc_pki_storage_plan plan;
  TC_TLV_result result;
  if (!limits || !workspace || !work || !out)
    return TC_TLV_ARGUMENT;
  tc_pki_storage_plan_begin(&plan, writes, 5, *work);
  TC_PKI_PLAN_WRITE(&plan, workspace->frames.data, workspace->frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, workspace->extension_oids, workspace->extension_capacity);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  TC_PKI_PLAN_WRITE(&plan, records, capacity);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, encoded, count);
  TC_PKI_PLAN_INPUT(&plan, limits, 1);
  TC_PKI_PLAN_INPUT(&plan, workspace, 1);
  /* Check later records before any earlier record can overwrite them. */
  tc_pki_storage_plan_input_spans(&plan, encoded, count);
  result = tc_pki_storage_plan_finish(&plan, work);
  if (result != TC_TLV_OK)
    return result;
  if (count > capacity)
    return TC_TLV_LIMIT;
  const tc_pki_tree_workspace tree = {workspace->frames.data, workspace->frames.capacity, work};
  for (size_t i = 0; i < count; ++i) {
    result = tc_x509_crl_record_read(encoded[i], limits, &tree, workspace->extension_oids,
                                     workspace->extension_capacity, &records[i]);
    if (result != TC_TLV_OK)
      return result;
  }
  *out = (TC_X509_crl_index){records, count, 0};
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_entries_init(TC_bytes encoded, const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree, TC_TLV_reader* out)
{
  TC_TLV_reader parsed;
  TC_TLV_result result;
  if (!out || !tree || !tree->work)
    return TC_TLV_ARGUMENT;
  if (!encoded.data && !encoded.length)
    return TC_TLV_reader_init(out, (TC_bytes){NULL, 0}, TC_TLV_DER, limits);
  result = tc_pki_tree_open(encoded, 0x30, TC_TLV_DER, limits, tree, &parsed);
  if (result != TC_TLV_OK)
    return result;
  /* RFC 5280 omits revokedCertificates when no certificates are revoked. */
  if (tc_pki_end(&parsed))
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_entry_next(TC_TLV_reader* reader, unsigned version,
                                     const tc_pki_tree_workspace* tree, tc_x509_crl_entry* out)
{
  TC_TLV_reader next, fields;
  TC_TLV_element element;
  tc_x509_crl_entry parsed = {0};
  TC_TLV_result result;
  if (!reader || !out || reader->profile != TC_TLV_DER || (version != 1 && version != 2))
    return TC_TLV_ARGUMENT;
  next = *reader;
  result = tc_pki_tree_next(&next, tree, &element);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_tree_open(element.encoded, 0x30, TC_TLV_DER, &reader->limits, tree, &fields);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_tree_field(&fields, 2, tree, &element);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_integer(element.encoded, &parsed.serial, &parsed.serial_negative);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_tree_next(&fields, tree, &element);
  if (result != TC_TLV_OK)
    return result == TC_TLV_END ? TC_TLV_INVALID : result;
  result = tc_x509_time_value(&element, &parsed.revoked_at);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_end(&fields)) {
    if (version != 2)
      return TC_TLV_INVALID;
    result = tc_pki_tree_field(&fields, 0x30, tree, &element);
    if (result != TC_TLV_OK)
      return result;
    if (!element.value.length || !tc_pki_end(&fields))
      return TC_TLV_INVALID;
    parsed.extensions = element.encoded;
  }
  *reader = next;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_metadata_read(const tc_x509_crl_fields* fields,
                                        const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree, TC_X509_crl* out)
{
  TC_TLV_element element;
  TC_X509_crl parsed = {0};
  unsigned unused;
  if (!fields || !out || !tree || !tree->work)
    return TC_TLV_ARGUMENT;
  const TC_bytes encoded[] = {fields->algorithm,       fields->signature, fields->version,
                              fields->inner_algorithm, fields->issuer,    fields->this_update,
                              fields->next_update,     fields->extensions};
  for (size_t i = 0; i < sizeof encoded / sizeof encoded[0]; ++i) {
    if (!encoded[i].length)
      continue;
    TC_TLV_result framed = tc_pki_tree_read(encoded[i], TC_TLV_DER, limits, tree, &element);
    if (framed != TC_TLV_OK)
      return framed;
    if (element.encoded.length != encoded[i].length)
      return TC_TLV_INVALID;
  }
  TC_TLV_result result = tc_pki_tree_algorithm(fields->algorithm, TC_TLV_DER, limits, tree,
                                               &parsed.signature_algorithm);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_bit_string(fields->signature, &parsed.signature, &unused);
  if (result != TC_TLV_OK)
    return result;
  if (unused || !parsed.signature.length)
    return TC_TLV_INVALID;
  parsed.version = 1;
  if (fields->version.length) {
    uint32_t version;
    result = TC_DER_uint32(fields->version, &version);
    if (result != TC_TLV_OK)
      return result;
    if (version != 1)
      return TC_TLV_INVALID;
    parsed.version = 2;
  }
  if (tc_pki_work_charge(tree->work, fields->inner_algorithm.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  if (!tc_pki_equal(fields->inner_algorithm, fields->algorithm))
    return TC_TLV_INVALID;
  result = TC_TLV_read(fields->issuer, TC_TLV_DER, limits, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!element.value.length)
    return TC_TLV_INVALID;
  parsed.issuer = fields->issuer;
  result = tc_pki_tree_name(parsed.issuer, TC_TLV_DER, limits, tree);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_read(fields->this_update, TC_TLV_DER, limits, &element);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_time_value(&element, &parsed.this_update);
  if (result != TC_TLV_OK)
    return result;
  if (fields->next_update.length) {
    result = TC_TLV_read(fields->next_update, TC_TLV_DER, limits, &element);
    if (result != TC_TLV_OK)
      return result;
    result = tc_x509_time_value(&element, &parsed.next_update);
    if (result != TC_TLV_OK)
      return result;
    parsed.has_next_update = 1;
  }
  if (fields->extensions.length && parsed.version != 2)
    return TC_TLV_INVALID;
  parsed.extensions = fields->extensions;
  *out = parsed;
  return TC_TLV_OK;
}

/* Layout ranges were checked against input by the source reader. */
static TC_bytes crl_borrow_field(TC_bytes input, tc_source_span span)
{
  const TC_bytes view = {span.length ? input.data + (size_t)span.offset : NULL,
                         (size_t)span.length};
  return view;
}

TC_TLV_result tc_x509_crl_read(TC_bytes input, const TC_TLV_limits* limits,
                               const tc_pki_tree_workspace* tree, TC_X509_crl* out)
{
  TC_TLV_reader outer, entries;
  TC_X509_crl parsed;
  if (!out || !tree || !tree->work)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_tree_open(input, 0x30, TC_TLV_DER, limits, tree, &outer);
  if (result != TC_TLV_OK)
    return result;
  uint8_t window[TC_TLV_HEADER_BYTES];
  TC_source source = {tc_source_memory_read, &input, input.length};
  tc_source_reader reader;
  tc_x509_crl_layout layout;
  if (tc_source_reader_init(&reader, &source, (TC_buffer){window, sizeof window}, UINT64_MAX,
                            UINT64_MAX) != TC_RESULT_OK)
    return TC_TLV_ARGUMENT;
  result = tc_x509_crl_source_layout(&reader, &layout);
  if (result != TC_TLV_OK)
    return result;
  const tc_x509_crl_fields fields = {
      crl_borrow_field(input, layout.algorithm),   crl_borrow_field(input, layout.signature),
      crl_borrow_field(input, layout.version),     crl_borrow_field(input, layout.inner_algorithm),
      crl_borrow_field(input, layout.issuer),      crl_borrow_field(input, layout.this_update),
      crl_borrow_field(input, layout.next_update), crl_borrow_field(input, layout.extensions)};
  result = tc_x509_crl_metadata_read(&fields, limits, tree, &parsed);
  if (result != TC_TLV_OK)
    return result;
  parsed.encoded = input;
  parsed.tbs = crl_borrow_field(input, layout.tbs);
  parsed.revoked = crl_borrow_field(input, layout.revoked);
  result = tc_x509_crl_entries_init(parsed.revoked, limits, tree, &entries);
  if (result != TC_TLV_OK)
    return result;
  while (!tc_pki_end(&entries)) {
    tc_x509_crl_entry entry;
    result = tc_x509_crl_entry_next(&entries, parsed.version, tree, &entry);
    if (result != TC_TLV_OK)
      return result;
  }
  *out = parsed;
  return TC_TLV_OK;
}
#endif
