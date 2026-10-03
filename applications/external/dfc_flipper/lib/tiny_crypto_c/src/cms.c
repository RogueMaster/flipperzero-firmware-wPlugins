/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/cms.h>
#include <tiny_crypto/x509.h>
#include <tiny_crypto/piv_oid.h>
#if TC_ENABLE_CMS
#include "cms_base_internal.h"
#include "pki_internal.h"
#include "x509_time_internal.h"
#include "pki_budget_internal.h"
#include "pki_octets_internal.h"
#include "pki_tree_internal.h"
#include "pki_storage_internal.h"
#include "pki_reader_internal.h"
#include "cms_digest_internal.h"
#include "hash_dispatch_internal.h"
#include "cms_signature_internal.h"
#include "pki_status_internal.h"
#include "pki_hash_parts_internal.h"

static const uint8_t cms_data_oid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 7, 1};

typedef struct {
  TC_bytes input;
  TC_TLV_element* fields;
  size_t capacity, count, roots;
  int overflow;
} cms_fields_state;

/* Record each depth-1 field. BEGIN fixes the start, CLOSE fixes the end, so
 * a BER end-of-contents marker belongs to encoded and stays out of value. */
static void cms_fields_visit(void* user, const TC_TLV_event* event)
{
  cms_fields_state* state = user;
  TC_TLV_element* field;
  if (event->kind == TC_TLV_BEGIN && event->depth == 0)
    ++state->roots;
  if (event->depth != 1 || state->overflow)
    return;
  if (event->kind == TC_TLV_BEGIN) {
    if (state->count == state->capacity) {
      state->overflow = 1;
      return;
    }
    field = &state->fields[state->count++];
    field->header = event->header;
    field->encoded = (TC_bytes){state->input.data + event->offset, 0};
    field->value = (TC_bytes){field->encoded.data + event->header.header_length, 0};
  } else if (event->kind == TC_TLV_CLOSE && state->count) {
    field = &state->fields[state->count - 1];
    field->value.length = (size_t)(state->input.data + event->offset - field->value.data);
    field->encoded.length =
        (size_t)(state->input.data + event->offset - field->encoded.data) + event->bytes.length;
  }
}

/* Collect the immediate BER fields of one constructed value with the given
 * tag while validating the whole value. capacity covers the largest schema
 * read with it, so more fields are malformed input and return INVALID. fields may
 * change on failure. count changes only on OK. Spans borrow encoded. */
static TC_TLV_result cms_fields(TC_bytes encoded, unsigned tag, TC_TLV_profile profile,
                                const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
                                TC_TLV_element* fields, size_t capacity, size_t* count)
{
  TC_TLV_element root;
  cms_fields_state state = {encoded, fields, capacity, 0, 0, 0};
  TC_TLV_result result = TC_TLV_header_read(encoded, profile, limits, &root.header);
  if (result != TC_TLV_OK)
    return result;
  if (!root.header.constructed || !tc_pki_tag(&root, tag))
    return TC_TLV_INVALID;
  if (tc_pki_work_charge(tree->work, encoded.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_TLV_walk(encoded, profile, limits, (TC_TLV_frames){tree->frames, tree->capacity},
                       cms_fields_visit, &state);
  if (result != TC_TLV_OK)
    return result;
  if (state.roots != 1 || state.overflow)
    return TC_TLV_INVALID;
  *count = state.count;
  return TC_TLV_OK;
}

typedef struct {
  TC_hash_algorithm algorithm;
  TC_hash_context* workspace;
  size_t* work;
} cms_octets_hash_state;

static TC_TLV_result cms_octets_hash_update(void* context, TC_bytes bytes)
{
  cms_octets_hash_state* state = context;
  if (tc_pki_work_charge(state->work, bytes.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  return tc_hash_update(state->algorithm, state->workspace, bytes) == TC_OK ? TC_TLV_OK
                                                                            : TC_TLV_INVALID;
}

/* Hash BER OCTET STRING value bytes in place, without flattening constructed
 * content. digest receives the selected hash's output size and is written only
 * on OK. workspace is wiped on return. Callers check that algorithm is
 * available. All writable storage is disjoint from encoded and each other. */
static TC_TLV_result cms_octets_hash(TC_bytes encoded, const TC_TLV_limits* limits,
                                     const tc_pki_tree_workspace* tree, TC_hash_algorithm algorithm,
                                     TC_hash_context* workspace, uint8_t* digest)
{
  cms_octets_hash_state state = {algorithm, workspace, tree->work};
  TC_TLV_result result = TC_TLV_INVALID;
  if (tc_hash_init(algorithm, workspace) == TC_OK)
    result = tc_pki_octets(encoded, TC_TLV_BER, limits, tree, cms_octets_hash_update, &state);
  if (result == TC_TLV_OK && tc_hash_final(algorithm, workspace, digest) != TC_OK)
    result = TC_TLV_INVALID;
  TC_secure_zero(workspace, sizeof *workspace);
  return result;
}

/* Unknown hashes may belong to unused signers. When selecting a hash, compare
 * its OID and validate parameters. Equal parameters may have distinct BER encodings. */
TC_TLV_result tc_cms_digest_algorithms(TC_bytes encoded, const TC_DER_algorithm* required,
                                       const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree, TC_hash_algorithm* out)
{
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  TC_hash_algorithm hash = TC_HASH_UNKNOWN;
  int listed = 0;
  if (required) {
    if (tc_pki_work_charge(tree->work, required->oid.length) != TC_TLV_OK ||
        tc_pki_work_charge(tree->work, required->parameters.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    result = tc_pki_hash_algorithm_profile(required, TC_TLV_BER, &hash);
    if (result != TC_TLV_OK)
      return result;
  }
  result = tc_pki_tree_open(encoded, 0x31, TC_TLV_BER, limits, tree, &reader);
  if (result != TC_TLV_OK)
    return result;
  while (!tc_pki_end(&reader)) {
    TC_DER_algorithm algorithm;
    result = tc_pki_tree_next(&reader, tree, &element);
    if (result != TC_TLV_OK)
      return result;
    result = tc_pki_tree_algorithm(element.encoded, TC_TLV_BER, limits, tree, &algorithm);
    if (result != TC_TLV_OK)
      return result;
    if (!required)
      continue;
    if (tc_pki_work_charge(tree->work, algorithm.oid.length) != TC_TLV_OK ||
        tc_pki_work_charge(tree->work, required->oid.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    if (!tc_pki_equal(algorithm.oid, required->oid))
      continue;
    if (tc_pki_work_charge(tree->work, algorithm.parameters.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    result = tc_pki_hash_parameters_profile(&algorithm, TC_TLV_BER);
    if (result != TC_TLV_OK)
      return result;
    listed = 1;
  }
  if (required && !listed)
    return TC_TLV_INVALID;
  if (out)
    *out = hash;
  return TC_TLV_OK;
}

void tc_cms_signer_spans(const TC_CMS_signer_info* signer, TC_bytes* spans)
{
  spans[0] = signer->encoded;
  spans[1] = signer->issuer;
  spans[2] = signer->serial;
  spans[3] = signer->subject_key_id;
  spans[4] = signer->digest_algorithm.oid;
  spans[5] = signer->digest_algorithm.parameters;
  spans[6] = signer->signature_algorithm.oid;
  spans[7] = signer->signature_algorithm.parameters;
  spans[8] = signer->signed_attributes;
  spans[9] = signer->unsigned_attributes;
  spans[10] = signer->signature;
}

static TC_TLV_result cms_signature_storage(const TC_CMS_signer_verify_request* request,
                                           TC_bytes input,
                                           const TC_CMS_signature_workspace* workspace,
                                           size_t* work)
{
  TC_bytes writes[3];
  tc_pki_storage_plan plan;
  if (!request || !request->signer || !request->key || !request->provider || !request->limits ||
      !workspace || !work)
    return TC_TLV_ARGUMENT;
  const TC_X509_public_key* key = request->key;
  TC_bytes signer_fields[TC_CMS_SIGNER_SPAN_COUNT];
  tc_cms_signer_spans(request->signer, signer_fields);
  const TC_bytes fields[] = {
      key->algorithm.oid, key->algorithm.parameters, key->key, key->modulus, key->exponent,
      key->curve_oid,     request->content_type,     input};
  tc_pki_storage_plan_begin(&plan, writes, 3, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, workspace->frames.data, workspace->frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, workspace->signature, workspace->signature_capacity);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, request, 1);
  TC_PKI_PLAN_INPUT(&plan, request->signer, 1);
  TC_PKI_PLAN_INPUT(&plan, key, 1);
  TC_PKI_PLAN_INPUT(&plan, request->provider, 1);
  TC_PKI_PLAN_INPUT(&plan, request->limits, 1);
  TC_PKI_PLAN_INPUT(&plan, workspace, 1);
  tc_pki_storage_plan_input_spans(&plan, signer_fields, TC_CMS_SIGNER_SPAN_COUNT);
  tc_pki_storage_plan_input_spans(&plan, fields, sizeof fields / sizeof *fields);
  if (tc_pki_storage_plan_finish(&plan, NULL) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  return tc_pki_work_charge(work, tc_pki_storage_plan_used(&plan));
}

/* Hash scratch shared by content hashing and signed-attribute hashing. digest
 * holds one digest of the largest enabled hash. */
typedef struct {
  TC_hash_context* hash;
  uint8_t* digest;
} cms_hash_scratch;

/* Check the signed-attribute binding of digest and verify the signature with
 * the resolved algorithm. signer_name and cache follow tc_cms_signer_verify. */
static TC_X509_signature_result cms_verify_digest(const TC_CMS_signer_verify_request* request,
                                                  TC_bytes digest,
                                                  const tc_cms_signature_algorithm* algorithm,
                                                  const TC_CMS_signature_workspace* workspace,
                                                  size_t* work, const cms_hash_scratch* scratch,
                                                  TC_bytes* signer_name,
                                                  tc_cms_signed_attrs_cache* cache)
{
  const TC_CMS_signer_info* signer = request->signer;
  const TC_TLV_limits* limits = request->limits;
  TC_hash_context* hash_workspace = scratch->hash;
  uint8_t* digest_scratch = scratch->digest;
  TC_CMS_signed_attributes attributes;
  tc_hash_info hash;
  TC_bytes signature, signed_digest = digest;
  TC_TLV_result checked;
  int matched;
  if (signer_name)
    *signer_name = (TC_bytes){NULL, 0};
  if (signer->signed_attributes.length) {
    if (cache && cache->valid && cache->hash == algorithm->signature.hash) {
      if (signer_name)
        *signer_name = cache->signer_name;
      signed_digest = (TC_bytes){cache->digest, cache->digest_length};
    } else {
      checked = TC_CMS_signed_attributes_read(signer->signed_attributes, &request->policy, limits,
                                              workspace->frames, work, &attributes);
      if (checked != TC_TLV_OK)
        return tc_pki_signature_error(checked);
      if (signer_name)
        *signer_name = attributes.signer_name;
      /* The entry checked the digest length against the content hash. */
      checked =
          tc_cms_content_digest_compare(&attributes, request->content_type, digest, work, &matched);
      if (checked != TC_TLV_OK)
        return tc_pki_signature_error(checked);
      if (!matched)
        return TC_X509_SIGNATURE_INVALID;
      if (!tc_hash_available(algorithm->signature.hash) ||
          !tc_hash_info_get(algorithm->signature.hash, &hash))
        return TC_X509_SIGNATURE_UNSUPPORTED;
      for (size_t i = 0; i < sizeof attributes.signature_input / sizeof *attributes.signature_input;
           ++i)
        if (tc_pki_work_charge(work, attributes.signature_input[i].length) != TC_TLV_OK)
          return TC_X509_SIGNATURE_LIMIT;
      /* Binding is complete, so the content digest buffer can be reused. */
      if (tc_hash_digest_parts(algorithm->signature.hash, attributes.signature_input,
                               sizeof attributes.signature_input /
                                   sizeof *attributes.signature_input,
                               digest_scratch, hash_workspace) != TC_OK)
        return TC_X509_SIGNATURE_ERROR;
      signed_digest = (TC_bytes){digest_scratch, hash.digest_length};
      if (cache && cache->digest && cache->capacity >= hash.digest_length) {
        memcpy(cache->digest, digest_scratch, hash.digest_length);
        cache->digest_length = hash.digest_length;
        cache->hash = algorithm->signature.hash;
        cache->signer_name = attributes.signer_name;
        cache->valid = 1;
      }
    }
  } else {
    /* RFC 5652 section 5.3 requires signed attributes for other content types. */
    if (tc_pki_work_charge(work, request->content_type.length) != TC_TLV_OK)
      return TC_X509_SIGNATURE_LIMIT;
    if (!tc_pki_equal(request->content_type, (TC_bytes){cms_data_oid, sizeof cms_data_oid}))
      return TC_X509_SIGNATURE_INVALID;
  }
  checked = tc_pki_octets_contiguous(
      signer->signature, 4, tc_cms_envelope_profile(request->policy), limits,
      &(tc_pki_tree_workspace){workspace->frames.data, workspace->frames.capacity, work},
      (TC_buffer){workspace->signature, workspace->signature_capacity}, &signature);
  return checked == TC_TLV_OK
             ? TC_X509_signature_verify_digest(signed_digest, &algorithm->signature, signature,
                                               request->key, request->provider, work)
             : tc_pki_signature_error(checked);
}

TC_TLV_result tc_cms_hash_content(TC_bytes input, TC_CMS_content_encoding encoding,
                                  TC_hash_algorithm algorithm, const TC_TLV_limits* limits,
                                  const tc_pki_tree_workspace* tree, TC_hash_context* scratch,
                                  uint8_t* digest)
{
  if (!tc_hash_available(algorithm))
    return TC_TLV_UNSUPPORTED;
  if (encoding == TC_CMS_CONTENT_BER_OCTETS)
    return cms_octets_hash(input, limits, tree, algorithm, scratch, digest);
  if (encoding != TC_CMS_CONTENT_RAW)
    return TC_TLV_ARGUMENT;
  return tc_pki_hash_parts(&input, 1, algorithm, limits, tree, scratch, digest);
}

TC_X509_signature_result tc_cms_signer_verify(const TC_CMS_signer_verify_request* request,
                                              TC_bytes input, tc_cms_verify_input kind,
                                              const TC_CMS_signature_workspace* workspace,
                                              size_t* work, TC_bytes* signer_name,
                                              tc_cms_signed_attrs_cache* cache)
{
  enum { MAX_DIGEST_BYTES = 64 };
  tc_cms_signature_algorithm algorithm;
  TC_hash_context hash_workspace;
  tc_hash_info hash;
  uint8_t digest_scratch[MAX_DIGEST_BYTES];
  TC_bytes digest = input;
  TC_TLV_result checked;
  TC_X509_signature_result result;
  if (!request || !tc_cms_verification_policy_valid(request->policy))
    return TC_X509_SIGNATURE_ERROR;
  checked = cms_signature_storage(request, input, workspace, work);
  if (checked != TC_TLV_OK)
    return tc_pki_signature_error(checked);
  if (!request->provider->verify_digest)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (!request->content_type.data || !request->content_type.length ||
      (kind == TC_CMS_VERIFY_DIGEST && !input.data))
    return TC_X509_SIGNATURE_ERROR;
  const tc_pki_tree_workspace tree = {workspace->frames.data, workspace->frames.capacity, work};
  checked = tc_cms_signature_resolve_policy(
      request->signer, request->key, tc_cms_envelope_profile(request->policy), request->limits,
      &tree, request->policy.rsa_parameters, &algorithm);
  if (checked != TC_TLV_OK)
    return tc_pki_signature_error(checked);
  if (!tc_hash_info_get(algorithm.content_hash, &hash))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (kind == TC_CMS_VERIFY_DIGEST) {
    /* A caller digest of the wrong length is an argument error. */
    if (tc_cms_content_digest_length_check(algorithm.content_hash, digest) != TC_TLV_OK)
      return TC_X509_SIGNATURE_ERROR;
  } else {
    checked = tc_cms_hash_content(
        input, kind == TC_CMS_VERIFY_BER ? TC_CMS_CONTENT_BER_OCTETS : TC_CMS_CONTENT_RAW,
        algorithm.content_hash, request->limits, &tree, &hash_workspace, digest_scratch);
    if (checked != TC_TLV_OK) {
      result = tc_pki_signature_error(checked);
      goto done;
    }
    digest = (TC_bytes){digest_scratch, hash.digest_length};
  }
  /* RFC 5652 sections 5.3 and 11.4: the signature excludes unsignedAttrs, so
   * countersignatures and other unsigned attributes stay unread here. */
  const cms_hash_scratch scratch = {&hash_workspace, digest_scratch};
  result =
      cms_verify_digest(request, digest, &algorithm, workspace, work, &scratch, signer_name, cache);
done:
  TC_secure_zero(digest_scratch, sizeof digest_scratch);
  TC_secure_zero(&hash_workspace, sizeof hash_workspace);
  return result;
}

TC_X509_signature_result TC_CMS_signer_verify_digest(const TC_CMS_signer_verify_request* request,
                                                     TC_bytes digest,
                                                     const TC_CMS_signature_workspace* workspace,
                                                     size_t* work)
{
  return tc_cms_signer_verify(request, digest, TC_CMS_VERIFY_DIGEST, workspace, work, NULL, NULL);
}

TC_X509_signature_result TC_CMS_signer_verify_content(const TC_CMS_signer_verify_request* request,
                                                      TC_bytes content,
                                                      TC_CMS_content_encoding encoding,
                                                      const TC_CMS_signature_workspace* workspace,
                                                      size_t* work)
{
  tc_cms_verify_input kind;
  if (encoding == TC_CMS_CONTENT_RAW)
    kind = TC_CMS_VERIFY_RAW;
  else if (encoding == TC_CMS_CONTENT_BER_OCTETS)
    kind = TC_CMS_VERIFY_BER;
  else
    return TC_X509_SIGNATURE_ERROR;
  return tc_cms_signer_verify(request, content, kind, workspace, work, NULL, NULL);
}

TC_TLV_result TC_CMS_content_digest_check(const TC_CMS_signed_attributes* attributes,
                                          TC_bytes expected_type, TC_hash_algorithm algorithm,
                                          TC_bytes digest, size_t* work, int* matched)
{
  TC_bytes outputs[2];
  tc_pki_storage_plan plan;
  if (!attributes || !work || !matched)
    return TC_TLV_ARGUMENT;
  if (!expected_type.data || !expected_type.length || !digest.data ||
      !attributes->content_type.data || !attributes->content_type.length ||
      !attributes->message_digest.data)
    return TC_TLV_ARGUMENT;
  const TC_bytes fields[] = {expected_type, digest, attributes->content_type,
                             attributes->message_digest};
  /* Validate every range before writing the budget or comparison result. */
  tc_pki_storage_plan_begin(&plan, outputs, 2, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  TC_PKI_PLAN_WRITE(&plan, matched, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, attributes, 1);
  tc_pki_storage_plan_input_spans(&plan, fields, sizeof fields / sizeof *fields);
  if (tc_pki_storage_plan_finish(&plan, NULL) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  /* The digest length is an argument property, so check it before any charge. */
  const TC_TLV_result length = tc_cms_content_digest_length_check(algorithm, digest);
  if (length != TC_TLV_OK)
    return length;
  if (tc_pki_work_charge(work, tc_pki_storage_plan_used(&plan)) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  return tc_cms_content_digest_compare(attributes, expected_type, digest, work, matched);
}

TC_TLV_result TC_CMS_content_digest(TC_bytes encoded, TC_hash_algorithm algorithm,
                                    const TC_TLV_limits* limits, TC_TLV_frames frames, size_t* work,
                                    TC_buffer digest)
{
  TC_hash_context scratch;
  tc_hash_info info;
  TC_TLV_result result =
      tc_pki_reader_storage(encoded, limits, frames, work, digest.data, digest.capacity);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_hash_available(algorithm) || !tc_hash_info_get(algorithm, &info))
    return TC_TLV_UNSUPPORTED;
  if (digest.capacity < info.digest_length)
    return TC_TLV_LIMIT;
  return cms_octets_hash(encoded, limits,
                         &(tc_pki_tree_workspace){frames.data, frames.capacity, work}, algorithm,
                         &scratch, digest.data);
}

/* Copy and validate a reader policy. NULL and unknown values are ARGUMENT. */
static TC_TLV_result cms_policy_read(const TC_CMS_verification_policy* policy,
                                     TC_CMS_verification_policy* out)
{
  if (!policy || !tc_cms_verification_policy_valid(*policy))
    return TC_TLV_ARGUMENT;
  *out = *policy;
  return TC_TLV_OK;
}

TC_TLV_result TC_CMS_signed_data_read(TC_bytes encoded, const TC_CMS_verification_policy* policy,
                                      const TC_TLV_limits* limits, TC_TLV_frames frames,
                                      size_t* work, TC_CMS_signed_data* out)
{
  TC_CMS_signed_data parsed;
  TC_CMS_verification_policy selected;
  const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
  TC_TLV_result result = cms_policy_read(policy, &selected);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_reader_storage(encoded, limits, frames, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  const TC_TLV_profile profile = tc_cms_envelope_profile(selected);
  result = tc_cms_signed_data_read(encoded, profile, limits, frames, work, &parsed);
  if (result != TC_TLV_OK)
    return result;
  result = tc_cms_signed_data_version_check(&parsed, profile, limits, &tree);
  if (result != TC_TLV_OK)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_CMS_signer_info_read(TC_bytes encoded, const TC_CMS_verification_policy* policy,
                                      const TC_TLV_limits* limits, TC_TLV_frames frames,
                                      size_t* work, TC_CMS_signer_info* out)
{
  TC_CMS_verification_policy selected;
  TC_TLV_result result = cms_policy_read(policy, &selected);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_reader_storage(encoded, limits, frames, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  return tc_cms_signer_info_read(encoded, tc_cms_envelope_profile(selected), limits, frames, work,
                                 out);
}

TC_TLV_result TC_CMS_signers_init(TC_bytes encoded, const TC_CMS_verification_policy* policy,
                                  const TC_TLV_limits* limits, TC_TLV_frames frames, size_t* work,
                                  TC_TLV_reader* out)
{
  TC_TLV_reader reader = {0};
  TC_CMS_verification_policy selected;
  const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
  TC_TLV_result result = cms_policy_read(policy, &selected);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_reader_storage(encoded, limits, frames, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  result =
      tc_pki_tree_open(encoded, 0x31, tc_cms_envelope_profile(selected), limits, &tree, &reader);
  if (result != TC_TLV_OK)
    return result;
  *out = reader;
  return TC_TLV_OK;
}

TC_TLV_result TC_CMS_signer_next(TC_TLV_reader* reader, TC_TLV_frames frames, size_t* work,
                                 TC_CMS_signer_info* out)
{
  TC_bytes metadata;
  TC_TLV_reader next;
  TC_TLV_element element;
  TC_CMS_signer_info parsed;
  const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
  TC_TLV_result result;
  if (!reader || tc_pki_storage_span(reader, 1, sizeof *reader, &metadata) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  result = tc_pki_reader_storage_check(reader->input, reader, sizeof *reader, frames, work, out,
                                       sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  if ((reader->profile != TC_TLV_BER && reader->profile != TC_TLV_DER) ||
      reader->offset > reader->input.length)
    return TC_TLV_ARGUMENT;
  /* Exhaustion needs no parsing budget and leaves all caller storage alone. */
  if (tc_pki_end(reader))
    return TC_TLV_END;
  result = tc_pki_work_charge(work, TC_PKI_READER_STORAGE_WORK);
  if (result != TC_TLV_OK)
    return result;
  next = *reader;
  result = tc_pki_tree_next(&next, &tree, &element);
  if (result != TC_TLV_OK)
    return result;
  result = tc_cms_signer_info_read(element.encoded, reader->profile, &next.limits, frames, work,
                                   &parsed);
  if (result != TC_TLV_OK)
    return result;
  *reader = next;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_CMS_digest_algorithms_check(const TC_CMS_signed_data* signed_data,
                                             const TC_DER_algorithm* digest_algorithm,
                                             const TC_TLV_limits* limits, TC_TLV_frames frames,
                                             size_t* work, TC_hash_algorithm* out)
{
  TC_bytes writes[3];
  tc_pki_storage_plan plan;
  TC_hash_algorithm hash;
  if (!signed_data || !digest_algorithm || !limits || !work || !out ||
      !signed_data->digest_algorithms.data || !digest_algorithm->oid.data)
    return TC_TLV_ARGUMENT;
  const TC_bytes fields[] = {signed_data->digest_algorithms, digest_algorithm->oid,
                             digest_algorithm->parameters};
  tc_pki_storage_plan_begin(&plan, writes, 3, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, frames.data, frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, signed_data, 1);
  TC_PKI_PLAN_INPUT(&plan, digest_algorithm, 1);
  TC_PKI_PLAN_INPUT(&plan, limits, 1);
  tc_pki_storage_plan_input_spans(&plan, fields, sizeof fields / sizeof *fields);
  if (tc_pki_storage_plan_finish(&plan, NULL) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_work_charge(work, tc_pki_storage_plan_used(&plan));
  if (result != TC_TLV_OK)
    return result;
  result =
      tc_cms_digest_algorithms(signed_data->digest_algorithms, digest_algorithm, limits,
                               &(tc_pki_tree_workspace){frames.data, frames.capacity, work}, &hash);
  if (result != TC_TLV_OK)
    return result;
  *out = hash;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_signed_data_read(TC_bytes encoded, TC_TLV_profile profile,
                                      const TC_TLV_limits* limits, TC_TLV_frames frames,
                                      size_t* work, TC_CMS_signed_data* out)
{
  static const uint8_t signed_data_oid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 7, 2};
  /* RFC 5652 section 5.1: SignedData has at most six fields. ContentInfo and
   * EncapsulatedContentInfo have fewer. */
  enum { SIGNED_DATA_FIELDS = 6 };
  TC_TLV_element fields[SIGNED_DATA_FIELDS];
  TC_bytes encap;
  TC_CMS_signed_data parsed = {0};
  size_t count, index;
  TC_TLV_result result;
  if (!out || (profile != TC_TLV_DER && profile != TC_TLV_BER))
    return TC_TLV_ARGUMENT;
  parsed.encoded = encoded;
  const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
#define CMS_FIELDS(input, tag)                                                                     \
  cms_fields(input, tag, profile, limits, &tree, fields, SIGNED_DATA_FIELDS, &count)
  result = CMS_FIELDS(encoded, 0x30);
  if (result != TC_TLV_OK)
    return result;
  if (count != 2 || !tc_pki_tag(&fields[0], 6) ||
      !tc_pki_equal(fields[0].value, (TC_bytes){signed_data_oid, sizeof signed_data_oid}) ||
      !tc_pki_tag(&fields[1], 0xa0))
    return TC_TLV_INVALID;
  encoded = fields[1].encoded;
  result = CMS_FIELDS(encoded, 0xa0);
  if (result != TC_TLV_OK)
    return result;
  if (count != 1 || !tc_pki_tag(&fields[0], 0x30))
    return TC_TLV_INVALID;
  encoded = fields[0].encoded;
  result = CMS_FIELDS(encoded, 0x30);
  if (result != TC_TLV_OK)
    return result;
  if (count < 4 || !tc_pki_tag(&fields[0], 2) || !tc_pki_tag(&fields[1], 0x31) ||
      !tc_pki_tag(&fields[2], 0x30))
    return TC_TLV_INVALID;
  result = TC_DER_uint32_contents(fields[0].value, &parsed.version);
  if (result != TC_TLV_OK)
    return result;
  parsed.digest_algorithms = fields[1].encoded;
  encap = fields[2].encoded;
  index = 3;
  if (index < count && tc_pki_tag(&fields[index], 0xa0))
    parsed.certificates = fields[index++].encoded;
  if (index < count && tc_pki_tag(&fields[index], 0xa1))
    parsed.revocations = fields[index++].encoded;
  if (index + 1 != count || !tc_pki_tag(&fields[index], 0x31))
    return TC_TLV_INVALID;
  parsed.signers = fields[index].encoded;
  result = CMS_FIELDS(encap, 0x30);
  if (result != TC_TLV_OK)
    return result;
  if ((count != 1 && count != 2) || !tc_pki_tag(&fields[0], 6) ||
      TC_DER_oid_contents(fields[0].value) != TC_TLV_OK)
    return TC_TLV_INVALID;
  parsed.content_type = fields[0].value;
  if (count == 2) {
    if (!tc_pki_tag(&fields[1], 0xa0))
      return TC_TLV_INVALID;
    encoded = fields[1].encoded;
    result = CMS_FIELDS(encoded, 0xa0);
    if (result != TC_TLV_OK)
      return result;
    if (count != 1 || (!tc_pki_tag(&fields[0], 4) && !tc_pki_tag(&fields[0], 0x24)))
      return TC_TLV_INVALID;
    parsed.has_content = 1;
    parsed.content = fields[0].encoded;
    result = tc_pki_octets(parsed.content, profile, limits, &tree, NULL, NULL);
    if (result != TC_TLV_OK)
      return result;
  }
#undef CMS_FIELDS
  result = tc_cms_digest_algorithms(parsed.digest_algorithms, NULL, limits, &tree, NULL);
  if (result != TC_TLV_OK)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_classify_certificate(const TC_TLV_element* element,
                                          tc_cms_certificate_kind* out)
{
  if (tc_pki_tag(element, 0x30))
    *out = TC_CMS_CERT_X509;
  else if (tc_pki_tag(element, 0xa0))
    *out = TC_CMS_CERT_EXTENDED;
  else if (tc_pki_tag(element, 0xa1))
    *out = TC_CMS_CERT_ATTRIBUTE_V1;
  else if (tc_pki_tag(element, 0xa2))
    *out = TC_CMS_CERT_ATTRIBUTE_V2;
  else if (tc_pki_tag(element, 0xa3))
    *out = TC_CMS_CERT_OTHER;
  else
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_signed_data_check(const TC_CMS_signed_data* input, TC_TLV_profile profile,
                                       const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree, size_t signer_index,
                                       TC_CMS_signer_info* selected)
{
  enum { BASE_VERSION = 1, EXTENDED_VERSION = 3, ATTRIBUTE_V2_VERSION = 4, OTHER_VERSION = 5 };
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  uint32_t required;
  int found = 0;
  if (!input || !limits || !tree || !tree->work || !input->content_type.data ||
      !input->content_type.length)
    return TC_TLV_ARGUMENT;
  if (tc_pki_work_charge(tree->work, input->content_type.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  required = tc_pki_equal(input->content_type, (TC_bytes){cms_data_oid, sizeof cms_data_oid})
                 ? BASE_VERSION
                 : EXTENDED_VERSION;
  if (input->certificates.length) {
    result = tc_pki_tree_open(input->certificates, 0xa0, profile, limits, tree, &reader);
    if (result != TC_TLV_OK)
      return result;
    while (!tc_pki_end(&reader)) {
      tc_cms_certificate_kind kind;
      uint32_t version = BASE_VERSION;
      result = tc_pki_tree_next(&reader, tree, &element);
      if (result != TC_TLV_OK)
        return result;
      result = tc_cms_classify_certificate(&element, &kind);
      if (result != TC_TLV_OK)
        return result;
      if (kind == TC_CMS_CERT_OTHER)
        version = OTHER_VERSION;
      else if (kind == TC_CMS_CERT_ATTRIBUTE_V2)
        version = ATTRIBUTE_V2_VERSION;
      else if (kind == TC_CMS_CERT_ATTRIBUTE_V1)
        version = EXTENDED_VERSION;
      if (version > required)
        required = version;
    }
  }
  if (input->revocations.length) {
    result = tc_pki_tree_open(input->revocations, 0xa1, profile, limits, tree, &reader);
    if (result != TC_TLV_OK)
      return result;
    while (!tc_pki_end(&reader)) {
      result = tc_pki_tree_next(&reader, tree, &element);
      if (result != TC_TLV_OK)
        return result;
      if (tc_pki_tag(&element, 0xa1))
        required = OTHER_VERSION;
      else if (!tc_pki_tag(&element, 0x30))
        return TC_TLV_INVALID;
    }
  }
  result = tc_pki_tree_open(input->signers, 0x31, profile, limits, tree, &reader);
  if (result != TC_TLV_OK)
    return result;
  while (!tc_pki_end(&reader)) {
    TC_CMS_signer_info signer;
    result = tc_pki_tree_next(&reader, tree, &element);
    if (result != TC_TLV_OK)
      return result;
    result =
        tc_cms_signer_info_read(element.encoded, profile, limits,
                                (TC_TLV_frames){tree->frames, tree->capacity}, tree->work, &signer);
    if (result != TC_TLV_OK)
      return result;
    if (signer.version == EXTENDED_VERSION && required < EXTENDED_VERSION)
      required = EXTENDED_VERSION;
    if (selected && !found) {
      if (signer_index)
        --signer_index;
      else {
        *selected = signer;
        found = 1;
      }
    }
  }
  return input->version == required && (!selected || found) ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result tc_cms_signed_data_version_check(const TC_CMS_signed_data* input,
                                               TC_TLV_profile profile, const TC_TLV_limits* limits,
                                               const tc_pki_tree_workspace* tree)
{
  return tc_cms_signed_data_check(input, profile, limits, tree, 0, NULL);
}

static void cms_definite(void* user, const TC_TLV_event* event)
{
  if (event->kind == TC_TLV_BEGIN && event->header.indefinite)
    *(int*)user = 0;
}

static TC_TLV_result cms_open(TC_bytes encoded, unsigned tag, TC_TLV_profile profile,
                              const TC_TLV_limits* limits, TC_TLV_frames frames, size_t* work,
                              TC_TLV_element* outer)
{
  int definite = 1;
  TC_TLV_result result = TC_TLV_read(encoded, profile, limits, outer);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(outer, tag) || outer->encoded.length != encoded.length || !outer->value.length)
    return TC_TLV_INVALID;
  if (tc_pki_work_charge(work, encoded.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_TLV_walk(encoded, profile, limits, frames, cms_definite, &definite);
  return result != TC_TLV_OK ? result : definite ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result cms_octet_count(void* context, TC_bytes bytes)
{
  size_t* count = context;
  if (bytes.length > SIZE_MAX - *count)
    return TC_TLV_LIMIT;
  *count += bytes.length;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_signer_info_read(TC_bytes encoded, TC_TLV_profile profile,
                                      const TC_TLV_limits* limits, TC_TLV_frames frames,
                                      size_t* work, TC_CMS_signer_info* out)
{
  TC_CMS_signer_info parsed = {0};
  const tc_pki_tree_workspace workspace = {frames.data, frames.capacity, work};
  TC_TLV_element element;
  TC_TLV_reader fields, identifier;
  size_t octets;
  TC_TLV_result result;
  if (!limits || !work || !out || (profile != TC_TLV_DER && profile != TC_TLV_BER))
    return TC_TLV_ARGUMENT;
  parsed.encoded = encoded;
#define CMS_TRY(call)                                                                              \
  do {                                                                                             \
    result = (call);                                                                               \
    if (result != TC_TLV_OK)                                                                       \
      return result == TC_TLV_END ? TC_TLV_INVALID : result;                                       \
  } while (0)
  CMS_TRY(tc_pki_tree_open(encoded, 0x30, profile, limits, &workspace, &fields));
  CMS_TRY(tc_pki_tree_field(&fields, 2, &workspace, &element));
  CMS_TRY(TC_DER_uint32_contents(element.value, &parsed.version));
  CMS_TRY(tc_pki_tree_next(&fields, &workspace, &element));
  if (tc_pki_tag(&element, 0x80) || tc_pki_tag(&element, 0xa0)) {
    if (parsed.version != 3)
      return TC_TLV_INVALID;
    octets = 0;
    CMS_TRY(tc_pki_octets_implicit(element.encoded, 0x80, profile, limits,
                                   &(tc_pki_tree_workspace){frames.data, frames.capacity, work},
                                   cms_octet_count, &octets));
    if (!octets)
      return TC_TLV_INVALID;
    parsed.subject_key_id = element.encoded;
  } else if (tc_pki_tag(&element, 0x30)) {
    if (parsed.version != 1)
      return TC_TLV_INVALID;
    CMS_TRY(TC_TLV_reader_init(&identifier, element.value, profile, limits));
    CMS_TRY(tc_pki_tree_field(&identifier, 0x30, &workspace, &element));
    parsed.issuer = element.encoded;
    CMS_TRY(tc_pki_tree_name(parsed.issuer, profile, limits, &workspace));
    CMS_TRY(tc_pki_tree_field(&identifier, 2, &workspace, &element));
    if (!tc_pki_end(&identifier))
      return TC_TLV_INVALID;
    CMS_TRY(TC_DER_integer_contents(element.value));
    parsed.serial = element.value;
    parsed.serial_negative = (element.value.data[0] & 0x80) != 0;
  } else
    return TC_TLV_INVALID;
  CMS_TRY(tc_pki_tree_field(&fields, 0x30, &workspace, &element));
  CMS_TRY(tc_pki_tree_algorithm(element.encoded, profile, limits, &workspace,
                                &parsed.digest_algorithm));
  CMS_TRY(tc_pki_tree_next(&fields, &workspace, &element));
  if (tc_pki_tag(&element, 0xa0)) {
    if (!element.value.length)
      return TC_TLV_INVALID;
    parsed.signed_attributes = element.encoded;
    CMS_TRY(tc_pki_tree_next(&fields, &workspace, &element));
  }
  if (!tc_pki_tag(&element, 0x30))
    return TC_TLV_INVALID;
  CMS_TRY(tc_pki_tree_algorithm(element.encoded, profile, limits, &workspace,
                                &parsed.signature_algorithm));
  CMS_TRY(tc_pki_tree_next(&fields, &workspace, &element));
  octets = 0;
  CMS_TRY(tc_pki_octets(element.encoded, profile, limits,
                        &(tc_pki_tree_workspace){frames.data, frames.capacity, work},
                        cms_octet_count, &octets));
  if (!octets)
    return TC_TLV_INVALID;
  parsed.signature = element.encoded;
  if (!tc_pki_end(&fields)) {
    CMS_TRY(tc_pki_tree_field(&fields, 0xa1, &workspace, &element));
    if (!element.value.length || !tc_pki_end(&fields))
      return TC_TLV_INVALID;
    parsed.unsigned_attributes = element.encoded;
  }
#undef CMS_TRY
  *out = parsed;
  return TC_TLV_OK;
}

/* RFC 8551 section 2.5.2: preserve capability order and optional parameters. */
static TC_TLV_result cms_capabilities(TC_bytes encoded, TC_TLV_profile profile,
                                      const TC_TLV_limits* limits,
                                      const tc_pki_tree_workspace* tree)
{
  TC_TLV_reader capabilities;
  TC_TLV_element element;
  tc_pki_oid_value capability;
  TC_TLV_result result = tc_pki_tree_open(encoded, 0x30, profile, limits, tree, &capabilities);
  if (result != TC_TLV_OK)
    return result;
  while ((result = tc_pki_tree_next(&capabilities, tree, &element)) == TC_TLV_OK) {
    result = tc_pki_tree_oid_value(element.encoded, 0x30, profile, limits, tree, &capability);
    if (result != TC_TLV_OK)
      return result;
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

typedef enum {
  CMS_ATTRIBUTE_CONTENT_TYPE,
  CMS_ATTRIBUTE_MESSAGE_DIGEST,
  CMS_ATTRIBUTE_SIGNING_TIME,
  CMS_ATTRIBUTE_SMIME_CAPABILITIES,
  CMS_ATTRIBUTE_ENTRY_UUID,
  CMS_ATTRIBUTE_SIGNER_NAME,
  CMS_ATTRIBUTE_FASCN,
  /* Recognized and skipped unless the policy rejects other attributes. */
  CMS_ATTRIBUTE_LISTED,
  /* twicFASC-N under the PIV set. Always UNSUPPORTED, so the alias of an
   * interpreted identifier is never skipped past the card binding. */
  CMS_ATTRIBUTE_EXCLUDED,
  CMS_ATTRIBUTE_OTHER
} cms_attribute_kind;

/* Classify an attribute type under the selected identifier set. */
static cms_attribute_kind cms_attribute_classify(TC_bytes oid, TC_CMS_attribute_oids oids)
{
  /* pkcs-9 1.2.840.113549.1.9 and id-aa 1.2.840.113549.1.9.16.2. */
  static const uint8_t pkcs9[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 9};
  static const uint8_t entry_uuid[] = {0x2b, 6, 1, 1, 0x10, 4}; /* RFC 4530 section 2.1. */
  enum {
    CONTENT_TYPE = 3,
    MESSAGE_DIGEST = 4,
    SIGNING_TIME = 5,
    SMIME_CAPABILITIES = 15,
    ID_AA = 16,
    ALGORITHM_PROTECTION = 52,  /* RFC 6211 section 2. */
    SIGNING_CERTIFICATE = 12,   /* ESS section 5.4.2, RFC 5035 section 5. */
    SIGNING_CERTIFICATE_V2 = 47 /* ESS section 5.4.1, RFC 5035 section 3. */
  };
  /* config.h makes CMS require TC_ENABLE_PIV_OIDS. */
  if (oids != TC_CMS_ATTRIBUTE_OIDS_CMS) {
    /* TWIC Part 2 v5 section 6 pairs twicFASC-N with pivFASC-N. */
    const TC_PIV_oid identity = TC_PIV_oid_identify(oid, TC_PIV_OIDS_TWIC_COMPATIBLE);
    if (identity == TC_PIV_OID_SIGNER_NAME)
      return CMS_ATTRIBUTE_SIGNER_NAME;
    if (identity == TC_PIV_OID_FASCN)
      return oids == TC_CMS_ATTRIBUTE_OIDS_PIV_TWIC ||
                     TC_PIV_oid_identify(oid, TC_PIV_OIDS_ONLY) == TC_PIV_OID_FASCN
                 ? CMS_ATTRIBUTE_FASCN
                 : CMS_ATTRIBUTE_EXCLUDED;
  }
  if (tc_pki_equal(oid, (TC_bytes){entry_uuid, sizeof entry_uuid}))
    return CMS_ATTRIBUTE_ENTRY_UUID;
  if (oid.length <= sizeof pkcs9 || memcmp(oid.data, pkcs9, sizeof pkcs9))
    return CMS_ATTRIBUTE_OTHER;
  const uint8_t* arcs = oid.data + sizeof pkcs9;
  if (oid.length == sizeof pkcs9 + 1) {
    switch (arcs[0]) {
    case CONTENT_TYPE:
      return CMS_ATTRIBUTE_CONTENT_TYPE;
    case MESSAGE_DIGEST:
      return CMS_ATTRIBUTE_MESSAGE_DIGEST;
    case SIGNING_TIME:
      return CMS_ATTRIBUTE_SIGNING_TIME;
    case SMIME_CAPABILITIES:
      return CMS_ATTRIBUTE_SMIME_CAPABILITIES;
    case ALGORITHM_PROTECTION:
      return CMS_ATTRIBUTE_LISTED;
    default:
      return CMS_ATTRIBUTE_OTHER;
    }
  }
  if (oid.length == sizeof pkcs9 + 3 && arcs[0] == ID_AA && arcs[1] == 2 &&
      (arcs[2] == SIGNING_CERTIFICATE || arcs[2] == SIGNING_CERTIFICATE_V2))
    return CMS_ATTRIBUTE_LISTED;
  return CMS_ATTRIBUTE_OTHER;
}

/* INVALID when an attribute before stop in the SET value has type oid. The
 * earlier members were parsed already, so their framing is valid. */
static TC_TLV_result cms_attribute_repeated(TC_bytes set, const uint8_t* stop, TC_bytes oid,
                                            TC_TLV_profile profile, const TC_TLV_limits* limits,
                                            size_t* work)
{
  TC_TLV_reader attributes, fields;
  TC_TLV_element attribute, type;
  TC_TLV_result result = TC_TLV_reader_init(
      &attributes, (TC_bytes){set.data, (size_t)(stop - set.data)}, profile, limits);
  if (result != TC_TLV_OK)
    return result;
  while ((result = TC_TLV_next(&attributes, &attribute)) == TC_TLV_OK) {
    result = TC_TLV_reader_init(&fields, attribute.value, profile, limits);
    if (result != TC_TLV_OK)
      return result;
    if (tc_pki_next(&fields, 6, &type) != TC_TLV_OK)
      return TC_TLV_INVALID;
    if (tc_pki_work_charge(work, type.value.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    if (tc_pki_equal(type.value, oid))
      return TC_TLV_INVALID;
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

/* X.690 section 11.6: DER orders the values of a SET OF by their encodings.
 * BER keeps the signer's order. Work for these bytes was charged with the
 * enclosing attribute. */
static TC_TLV_result cms_attribute_values_order(TC_bytes values, TC_TLV_profile profile,
                                                const TC_TLV_limits* limits)
{
  TC_TLV_reader reader;
  TC_TLV_element value;
  TC_bytes previous = {NULL, 0};
  if (profile != TC_TLV_DER)
    return TC_TLV_OK;
  TC_TLV_result result = TC_TLV_reader_init(&reader, values, profile, limits);
  if (result != TC_TLV_OK)
    return result;
  while ((result = TC_TLV_next(&reader, &value)) == TC_TLV_OK) {
    if (previous.data && tc_pki_compare(previous, value.encoded) > 0)
      return TC_TLV_INVALID;
    previous = value.encoded;
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

TC_TLV_result TC_CMS_signed_attributes_read(TC_bytes encoded,
                                            const TC_CMS_verification_policy* policy,
                                            const TC_TLV_limits* limits, TC_TLV_frames frames,
                                            size_t* work, TC_CMS_signed_attributes* out)
{
  static const uint8_t set_tag = 0x31;
  enum { FASCN_BYTES = 25, UUID_BYTES = 16 };
  TC_CMS_signed_attributes parsed = {0};
  TC_CMS_verification_policy selected;
  TC_TLV_element outer, attribute, element, value;
  TC_TLV_reader attributes, fields, values;
  TC_bytes previous = {NULL, 0}, oid;
  TC_TLV_result result;
  TC_TLV_profile profile;
  if (!limits || !work || !out)
    return TC_TLV_ARGUMENT;
  result = cms_policy_read(policy, &selected);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_reader_storage(encoded, limits, frames, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  const TC_CMS_attribute_encoding encoding = selected.attributes;
  profile = encoding == TC_CMS_ATTRIBUTES_DER ? TC_TLV_DER : TC_TLV_BER;
  result = cms_open(encoded, 0xa0, profile, limits, frames, work, &outer);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_reader_init(&attributes, outer.value, profile, limits);
  if (result != TC_TLV_OK)
    return result;
  while ((result = TC_TLV_next(&attributes, &attribute)) == TC_TLV_OK) {
    if (tc_pki_work_charge(work, attribute.encoded.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    if (!tc_pki_tag(&attribute, 0x30) || (encoding == TC_CMS_ATTRIBUTES_DER && previous.data &&
                                          tc_pki_compare(previous, attribute.encoded) > 0))
      return TC_TLV_INVALID;
    previous = attribute.encoded;
    result = TC_TLV_reader_init(&fields, attribute.value, profile, limits);
    if (result != TC_TLV_OK)
      return result;
    if (tc_pki_next(&fields, 6, &element) != TC_TLV_OK ||
        TC_DER_oid_contents(element.value) != TC_TLV_OK)
      return TC_TLV_INVALID;
    oid = element.value;
    if (tc_pki_next(&fields, 0x31, &element) != TC_TLV_OK || !tc_pki_end(&fields) ||
        !element.value.length)
      return TC_TLV_INVALID;
    const cms_attribute_kind kind = cms_attribute_classify(oid, selected.attribute_oids);
    const TC_CMS_other_attributes other = selected.other_attributes;
    if (kind == CMS_ATTRIBUTE_EXCLUDED ||
        (kind == CMS_ATTRIBUTE_LISTED && other == TC_CMS_OTHER_ATTRIBUTES_REJECT) ||
        (kind == CMS_ATTRIBUTE_OTHER && other != TC_CMS_OTHER_ATTRIBUTES_SKIP_ALL))
      return TC_TLV_UNSUPPORTED;
    if (kind == CMS_ATTRIBUTE_OTHER) {
      /* Values stay opaque. cms_open checked their framing and the nonempty
       * SET was checked above. */
      result = cms_attribute_values_order(element.value, profile, limits);
      if (result == TC_TLV_OK)
        result =
            cms_attribute_repeated(outer.value, attribute.encoded.data, oid, profile, limits, work);
      if (result != TC_TLV_OK)
        return result;
      continue;
    }
    result = TC_TLV_reader_init(&values, element.value, profile, limits);
    if (result != TC_TLV_OK)
      return result;
    if (TC_TLV_next(&values, &value) != TC_TLV_OK || !tc_pki_end(&values))
      return TC_TLV_INVALID;
    if (kind == CMS_ATTRIBUTE_CONTENT_TYPE) {
      if (parsed.content_type.data || !tc_pki_tag(&value, 6) ||
          TC_DER_oid_contents(value.value) != TC_TLV_OK)
        return TC_TLV_INVALID;
      parsed.content_type = value.value;
    } else if (kind == CMS_ATTRIBUTE_MESSAGE_DIGEST) {
      if (parsed.message_digest.data || !tc_pki_tag(&value, 4) || !value.value.length)
        return TC_TLV_INVALID;
      parsed.message_digest = value.value;
    } else if (kind == CMS_ATTRIBUTE_SIGNING_TIME) {
      if (parsed.has_signing_time || tc_x509_time_value(&value, &parsed.signing_time) != TC_TLV_OK)
        return TC_TLV_INVALID;
      if (tc_pki_tag(&value, 0x18) && parsed.signing_time.year >= 1950 &&
          parsed.signing_time.year <= 2049)
        return TC_TLV_INVALID;
      parsed.has_signing_time = 1;
    } else if (kind == CMS_ATTRIBUTE_FASCN || kind == CMS_ATTRIBUTE_ENTRY_UUID) {
      tc_pki_octets_storage octets = {{NULL, 0}, 0, 0, NULL, 0, work};
      const int fascn = kind == CMS_ATTRIBUTE_FASCN;
      TC_bytes* destination = fascn ? &parsed.fascn_octets : &parsed.entry_uuid_octets;
      const size_t required = fascn ? FASCN_BYTES : UUID_BYTES;
      if (destination->data)
        return TC_TLV_INVALID;
      result = tc_pki_octets(value.encoded, profile, limits,
                             &(tc_pki_tree_workspace){frames.data, frames.capacity, work},
                             tc_pki_octets_store_chunk, &octets);
      if (result != TC_TLV_OK)
        return result;
      if (octets.length != required)
        return TC_TLV_INVALID;
      if (fascn)
        parsed.fascn_oid = oid;
      *destination = value.encoded;
    } else if (kind == CMS_ATTRIBUTE_SIGNER_NAME) {
      const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
      if (parsed.signer_name.data)
        return TC_TLV_INVALID;
      result = tc_pki_tree_name(value.encoded, profile, limits, &tree);
      if (result != TC_TLV_OK)
        return result;
      parsed.signer_name = value.encoded;
    } else if (kind == CMS_ATTRIBUTE_SMIME_CAPABILITIES) {
      const tc_pki_tree_workspace tree = {frames.data, frames.capacity, work};
      if (parsed.smime_capabilities.data)
        return TC_TLV_INVALID;
      result = cms_capabilities(value.encoded, profile, limits, &tree);
      if (result != TC_TLV_OK)
        return result;
      parsed.smime_capabilities = value.encoded;
    } else {
      /* RFC 6211 section 2 and RFC 5035 sections 3 and 5: one SEQUENCE value
       * and one instance per SignerInfo. */
      if (!tc_pki_tag(&value, 0x30))
        return TC_TLV_INVALID;
      result =
          cms_attribute_repeated(outer.value, attribute.encoded.data, oid, profile, limits, work);
      if (result != TC_TLV_OK)
        return result;
    }
  }
  if (result != TC_TLV_END)
    return result;
  if (!parsed.content_type.data || !parsed.message_digest.data)
    return TC_TLV_INVALID;
  /* RFC 5652 section 5.4 signs the attributes under a SET OF tag in place of the wire's
   * implicit [0] tag. */
  parsed.signature_input[0] = (TC_bytes){&set_tag, 1};
  parsed.signature_input[1] = (TC_bytes){encoded.data + 1, encoded.length - 1};
  *out = parsed;
  return TC_TLV_OK;
}

#endif
