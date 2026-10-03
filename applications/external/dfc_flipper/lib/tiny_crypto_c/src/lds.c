/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/lds.h>
#if TC_ENABLE_PIV_OBJECTS
#include "credential_text_internal.h"
#include "pki_reader_internal.h"
#include "pki_tree_internal.h"
#include "pki_hash_internal.h"
#include "pki_hash_parts_internal.h"
#include "pki_octets_internal.h"

static TC_TLV_result lds_number(TC_TLV_reader* reader, const tc_pki_tree_workspace* tree,
                                uint32_t* number)
{
  TC_TLV_element field;
  TC_TLV_result result = tc_pki_tree_field(reader, 2, tree, &field);
  return result == TC_TLV_OK ? TC_DER_uint32_contents(field.value, number) : result;
}

static TC_TLV_result lds_version_string(TC_TLV_reader* reader, const tc_pki_tree_workspace* tree,
                                        size_t digits, TC_bytes* out)
{
  TC_TLV_element field;
  TC_TLV_result result = tc_pki_tree_field(reader, 0x13, tree, &field);
  if (result != TC_TLV_OK)
    return result;
  if (field.value.length != digits || !tc_credential_digits(field.value.data, digits))
    return TC_TLV_INVALID;
  *out = field.value;
  return TC_TLV_OK;
}

static TC_TLV_result lds_hashes(TC_TLV_reader* groups, const tc_pki_tree_workspace* tree,
                                size_t digest_length, unsigned wanted, uint16_t* present,
                                TC_bytes* found)
{
  TC_TLV_element field;
  TC_bytes selected = {NULL, 0};
  uint16_t seen = 0;
  unsigned count = 0;
  TC_TLV_result result;
  while ((result = tc_pki_tree_next(groups, tree, &field)) == TC_TLV_OK) {
    TC_TLV_reader entry;
    uint32_t number;
    if (++count > TC_LDS_MAX_GROUPS)
      return TC_TLV_INVALID;
    result = tc_pki_tree_open(field.encoded, 0x30, TC_TLV_DER, &groups->limits, tree, &entry);
    if (result != TC_TLV_OK)
      return result;
    result = lds_number(&entry, tree, &number);
    if (result != TC_TLV_OK)
      return result;
    if (!number || number > TC_LDS_MAX_GROUPS)
      return TC_TLV_INVALID;
    const uint16_t bit = (uint16_t)(1u << (number - 1));
    if (seen & bit)
      return TC_TLV_INVALID;
    seen |= bit;
    result = tc_pki_tree_field(&entry, 4, tree, &field);
    if (result != TC_TLV_OK)
      return result;
    if (field.value.length != digest_length || !tc_pki_end(&entry))
      return TC_TLV_INVALID;
    if (number == wanted)
      selected = field.value;
  }
  if (result != TC_TLV_END)
    return result;
  if (count < 2)
    return TC_TLV_INVALID;
  *present = seen;
  if (found)
    *found = selected;
  return TC_TLV_OK;
}

TC_TLV_result TC_LDS_read(TC_bytes encoded, const TC_TLV_limits* limits, TC_TLV_frames frames,
                          size_t* work, TC_LDS_security_object* out)
{
  TC_LDS_security_object parsed = {0};
  TC_TLV_reader fields, groups;
  TC_TLV_element field;
  TC_DER_algorithm algorithm;
  tc_hash_info info;
  uint32_t version;
  TC_TLV_result result = tc_pki_reader_storage(encoded, limits, frames, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
  result = tc_pki_tree_open(encoded, 0x30, TC_TLV_DER, limits, &tree, &fields);
  if (result != TC_TLV_OK)
    return result;
  result = lds_number(&fields, &tree, &version);
  if (result != TC_TLV_OK)
    return result;
  if (version > 1)
    return TC_TLV_UNSUPPORTED;
  parsed.version = version;
  parsed.encoded = encoded;
  result = tc_pki_tree_field(&fields, 0x30, &tree, &field);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_tree_algorithm(field.encoded, TC_TLV_DER, limits, &tree, &algorithm);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_hash_algorithm(&algorithm, &parsed.hash);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_hash_info_get(parsed.hash, &info))
    return TC_TLV_UNSUPPORTED;
  result = tc_pki_tree_field(&fields, 0x30, &tree, &field);
  if (result != TC_TLV_OK)
    return result;
  parsed.hashes = field.encoded;
  result = TC_TLV_reader_init(&groups, field.value, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  result = lds_hashes(&groups, &tree, info.digest_length, 0, &parsed.groups, NULL);
  if (result != TC_TLV_OK)
    return result;
  if (version == 1) {
    TC_TLV_reader versions;
    result = tc_pki_tree_field(&fields, 0x30, &tree, &field);
    if (result != TC_TLV_OK)
      return result;
    result = TC_TLV_reader_init(&versions, field.value, TC_TLV_DER, limits);
    if (result != TC_TLV_OK)
      return result;
    result = lds_version_string(&versions, &tree, 4, &parsed.lds_version);
    if (result != TC_TLV_OK)
      return result;
    if ((parsed.lds_version.data[0] == '0' && parsed.lds_version.data[1] == '0') ||
        (parsed.lds_version.data[2] == '0' && parsed.lds_version.data[3] == '0'))
      return TC_TLV_INVALID;
    result = lds_version_string(&versions, &tree, 6, &parsed.unicode_version);
    if (result != TC_TLV_OK)
      return result;
    if (!tc_pki_end(&versions))
      return TC_TLV_INVALID;
  }
  if (!tc_pki_end(&fields))
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_LDS_read_content(TC_bytes octets, const TC_TLV_limits* limits,
                                  TC_TLV_frames frames, size_t* work, TC_buffer buffer,
                                  TC_LDS_security_object* out)
{
  TC_bytes writes[4], content;
  tc_pki_storage_plan plan;
  if (!limits || !work || !out)
    return TC_TLV_ARGUMENT;
  tc_pki_storage_plan_begin(&plan, writes, 4, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, frames.data, frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  TC_PKI_PLAN_WRITE(&plan, buffer.data, buffer.capacity);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, limits, 1);
  tc_pki_storage_plan_input_span(&plan, octets);
  TC_TLV_result result = tc_pki_storage_plan_finish(&plan, NULL);
  if (result == TC_TLV_OK)
    result = tc_pki_work_charge(work, tc_pki_storage_plan_used(&plan));
  if (result != TC_TLV_OK)
    return result;
  /* CMS permits nested BER chunks. The reconstructed LDS remains DER. */
  result = tc_pki_octets_contiguous(octets, 4, TC_TLV_BER, limits,
                                    &(tc_pki_tree_workspace){frames.data, frames.capacity, work},
                                    buffer, &content);
  if (result != TC_TLV_OK)
    return result;
  return TC_LDS_read(content, limits, frames, work, out);
}

TC_TLV_result TC_LDS_hash_find(const TC_LDS_security_object* object, unsigned number,
                               const TC_TLV_limits* limits, TC_TLV_frames frames, size_t* work,
                               TC_bytes* out)
{
  TC_TLV_reader groups;
  TC_bytes selected;
  tc_hash_info info;
  uint16_t present;
  if (!object || !number || number > TC_LDS_MAX_GROUPS)
    return TC_TLV_ARGUMENT;
  /* Check both borrowed spans and their metadata before charging work. */
  TC_TLV_result result = tc_pki_reader_storage_check(object->encoded, object, sizeof *object,
                                                     frames, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_reader_storage_check(object->hashes, limits, sizeof *limits, frames, work, out,
                                       sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_hash_info_get(object->hash, &info))
    return TC_TLV_UNSUPPORTED;
  result = tc_pki_work_charge(work, 2 * TC_PKI_READER_STORAGE_WORK);
  if (result != TC_TLV_OK)
    return result;
  const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
  result = tc_pki_tree_open(object->hashes, 0x30, TC_TLV_DER, limits, &tree, &groups);
  if (result != TC_TLV_OK)
    return result;
  result = lds_hashes(&groups, &tree, info.digest_length, number, &present, &selected);
  if (result != TC_TLV_OK)
    return result;
  if (present != object->groups)
    return TC_TLV_INVALID;
  if (!selected.data)
    return TC_TLV_END;
  *out = selected;
  return TC_TLV_OK;
}

TC_TLV_result TC_LDS_hash_check(const TC_LDS_security_object* object, unsigned number,
                                const TC_bytes* parts, size_t count, const TC_TLV_limits* limits,
                                TC_TLV_frames frames, size_t* work, int* matched)
{
  enum { WRITES = 3, MAX_DIGEST_BYTES = 64 };
  TC_bytes writes[WRITES], expected;
  tc_pki_storage_plan plan;
  if (!object || !limits || !work || !matched || !number || number > TC_LDS_MAX_GROUPS)
    return TC_TLV_ARGUMENT;
  if (count > limits->max_elements)
    return TC_TLV_LIMIT;
  tc_pki_storage_plan_begin(&plan, writes, WRITES, *work);
  TC_PKI_PLAN_WRITE(&plan, frames.data, frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  TC_PKI_PLAN_WRITE(&plan, matched, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, object, 1);
  TC_PKI_PLAN_INPUT(&plan, limits, 1);
  TC_PKI_PLAN_INPUT(&plan, parts, count);
  tc_pki_storage_plan_input_span(&plan, object->encoded);
  tc_pki_storage_plan_input_span(&plan, object->hashes);
  /* Reject an oversized part list before charging any of its checks. */
  if (plan.status == TC_TLV_OK && count > plan.budget / WRITES)
    return TC_TLV_LIMIT;
  tc_pki_storage_plan_input_spans(&plan, parts, count);
  TC_TLV_result stored = tc_pki_storage_plan_finish(&plan, work);
  if (stored != TC_TLV_OK)
    return stored;
  TC_TLV_result result = TC_LDS_hash_find(object, number, limits, frames, work, &expected);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_hash_available(object->hash))
    return TC_TLV_UNSUPPORTED;
  result = tc_pki_work_charge(work, expected.length);
  if (result != TC_TLV_OK)
    return result;
  TC_hash_context scratch;
  uint8_t digest[MAX_DIGEST_BYTES];
  const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
  result = tc_pki_hash_parts(parts, count, object->hash, limits, &tree, &scratch, digest);
  if (result == TC_TLV_OK) {
    const TC_status comparison = TC_ct_equal(expected, (TC_bytes){digest, expected.length});
    if (comparison == TC_ERROR)
      result = TC_TLV_ARGUMENT;
    else
      *matched = comparison == TC_OK;
  }
  TC_secure_zero(digest, sizeof digest);
  TC_secure_zero(&scratch, sizeof scratch);
  return result;
}
#endif
