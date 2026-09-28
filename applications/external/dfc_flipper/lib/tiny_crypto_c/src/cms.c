/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/cms.h>
#include <tiny_crypto/x509.h>
#include <tiny_crypto/piv_oid.h>
#if TC_ENABLE_CMS
#include "cms_base_internal.h"
#include "pki_internal.h"
#include "x509_time_internal.h"
#include "pki_children_internal.h"
#include "pki_octets_internal.h"
#include "pki_tree_internal.h"
#include "pki_storage_internal.h"
#include "pki_reader_internal.h"
#include "cms_digest_internal.h"
#include "pki_octets_hash_internal.h"
#include "cms_signature_internal.h"
#include "pki_status_internal.h"
#include "pki_hash_parts_internal.h"

static const uint8_t cms_data_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,7,1};

TC_TLV_result tc_cms_signed_data_check(const TC_CMS_signed_data* input,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    size_t signer_index, TC_CMS_signer_info* selected);

/* Unknown hashes may belong to unused signers. When selecting a hash, compare
 * its OID and validate parameters rather than comparing BER encodings. */
TC_TLV_result tc_cms_digest_algorithms(TC_bytes encoded, const TC_DER_algorithm* required,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree, TC_hash_algorithm* out)
{
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  TC_hash_algorithm hash = TC_HASH_UNKNOWN;
  int listed = 0;
  if (required) {
    if (tc_x509_path_charge(tree->work,required->oid.length) != TC_TLV_OK ||
        tc_x509_path_charge(tree->work,required->parameters.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = tc_pki_hash_algorithm_profile(required,TC_TLV_BER,&hash);
    if (result != TC_TLV_OK) return result;
  }
  result = tc_pki_tree_open(encoded,0x31,TC_TLV_BER,limits,tree,&reader);
  if (result != TC_TLV_OK) return result;
  while (!tc_pki_end(&reader)) {
    TC_DER_algorithm algorithm;
    result = tc_pki_tree_next(&reader,tree,&element);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_tree_algorithm(element.encoded,TC_TLV_BER,limits,tree,&algorithm);
    if (result != TC_TLV_OK) return result;
    if (!required) continue;
    if (tc_x509_path_charge(tree->work,algorithm.oid.length) != TC_TLV_OK ||
        tc_x509_path_charge(tree->work,required->oid.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (!tc_pki_equal(algorithm.oid,required->oid)) continue;
    if (tc_x509_path_charge(tree->work,algorithm.parameters.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = tc_pki_hash_parameters_profile(&algorithm,TC_TLV_BER);
    if (result != TC_TLV_OK) return result;
    listed = 1;
  }
  if (required && !listed) return TC_TLV_INVALID;
  if (out) *out = hash;
  return TC_TLV_OK;
}

void tc_cms_signer_spans(const TC_CMS_signer_info* signer, TC_bytes* spans)
{
  spans[0] = signer->encoded; spans[1] = signer->issuer;
  spans[2] = signer->serial; spans[3] = signer->subject_key_id;
  spans[4] = signer->digest_algorithm.oid; spans[5] = signer->digest_algorithm.parameters;
  spans[6] = signer->signature_algorithm.oid; spans[7] = signer->signature_algorithm.parameters;
  spans[8] = signer->signed_attributes; spans[9] = signer->unsigned_attributes;
  spans[10] = signer->signature;
}

static TC_TLV_result cms_signature_storage(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes input, const TC_X509_public_key* key,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_CMS_signature_workspace* workspace, size_t* work)
{
  enum { WRITE_COUNT = 3, METADATA_COUNT = 5 };
  TC_bytes writes[WRITE_COUNT], metadata[METADATA_COUNT];
  if (!signer || !key || !provider || !limits || !workspace || !work ||
      tc_pki_storage_span(signer,1,sizeof *signer,&metadata[0]) != TC_TLV_OK ||
      tc_pki_storage_span(key,1,sizeof *key,&metadata[1]) != TC_TLV_OK ||
      tc_pki_storage_span(provider,1,sizeof *provider,&metadata[2]) != TC_TLV_OK ||
      tc_pki_storage_span(limits,1,sizeof *limits,&metadata[3]) != TC_TLV_OK ||
      tc_pki_storage_span(workspace,1,sizeof *workspace,&metadata[4]) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  if (tc_pki_storage_span(workspace->frames,workspace->frame_capacity,sizeof *workspace->frames,&writes[0]) != TC_TLV_OK ||
      tc_pki_storage_span(workspace->signature,workspace->signature_capacity,1,&writes[1]) != TC_TLV_OK ||
      tc_pki_storage_span(work,1,sizeof *work,&writes[2]) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  TC_bytes signer_fields[TC_CMS_SIGNER_SPAN_COUNT];
  tc_cms_signer_spans(signer,signer_fields);
  const TC_bytes fields[] = {
    key->algorithm.oid,key->algorithm.parameters,key->key,key->modulus,key->exponent,key->curve_oid,
    content_type,input
  };
  const size_t field_count = sizeof fields / sizeof *fields;
  const size_t required = WRITE_COUNT * (WRITE_COUNT - 1) / 2 +
      WRITE_COUNT * (METADATA_COUNT + field_count + TC_CMS_SIGNER_SPAN_COUNT);
  size_t checks = required;
  for (size_t i = 0; i < WRITE_COUNT; ++i)
    if (tc_pki_storage_input(writes,i,writes[i],&checks) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  for (size_t i = 0; i < METADATA_COUNT; ++i)
    if (tc_pki_storage_input(writes,WRITE_COUNT,metadata[i],&checks) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  for (size_t i = 0; i < TC_CMS_SIGNER_SPAN_COUNT; ++i)
    if (tc_pki_storage_input(writes,WRITE_COUNT,signer_fields[i],&checks) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  for (size_t i = 0; i < field_count; ++i)
    if (tc_pki_storage_input(writes,WRITE_COUNT,fields[i],&checks) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  return tc_x509_path_charge(work,required);
}

static TC_X509_signature_result cms_verify_digest(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes digest, TC_CMS_attribute_encoding encoding,
    const TC_X509_public_key* key, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_CMS_signature_workspace* workspace, size_t* work,
    const tc_cms_signature_algorithm* algorithm, tc_hash_workspace* hash_workspace, uint8_t* scratch,
    TC_bytes* signer_name)
{
  TC_CMS_signed_attributes attributes;
  tc_hash_info hash;
  TC_bytes signature, signed_digest = digest;
  TC_TLV_result checked;
  int matched;
  if (signer_name) *signer_name = (TC_bytes){NULL,0};
  if (signer->signed_attributes.length) {
    checked = TC_CMS_signed_attributes_read(signer->signed_attributes,encoding,limits,
        workspace->frames,workspace->frame_capacity,work,&attributes);
    if (checked != TC_TLV_OK) return tc_pki_signature_error(checked);
    if (signer_name) *signer_name = attributes.signer_name;
    checked = tc_cms_content_digest_check(&attributes,content_type,algorithm->content_hash,digest,work,&matched);
    if (checked != TC_TLV_OK) return tc_pki_signature_error(checked);
    if (!matched) return TC_X509_SIGNATURE_INVALID;
    if (!tc_hash_available(algorithm->signature.hash) || !tc_hash_info_get(algorithm->signature.hash,&hash))
      return TC_X509_SIGNATURE_UNSUPPORTED;
    for (size_t i = 0; i < sizeof attributes.signature_input / sizeof *attributes.signature_input; ++i)
      if (tc_x509_path_charge(work,attributes.signature_input[i].length) != TC_TLV_OK)
        return TC_X509_SIGNATURE_LIMIT;
    /* Binding is complete; the content digest buffer can now be reused. */
    if (tc_hash_digest_parts(algorithm->signature.hash,attributes.signature_input,
        sizeof attributes.signature_input / sizeof *attributes.signature_input,scratch,hash_workspace) != TC_OK)
      return TC_X509_SIGNATURE_ERROR;
    signed_digest = (TC_bytes){scratch,hash.digest_length};
  } else {
    /* RFC 5652 section 5.3 requires signed attributes for other content types. */
    if (tc_x509_path_charge(work,content_type.length) != TC_TLV_OK) return TC_X509_SIGNATURE_LIMIT;
    if (!tc_pki_equal(content_type,(TC_bytes){cms_data_oid,sizeof cms_data_oid}))
      return TC_X509_SIGNATURE_INVALID;
  }
  checked = tc_pki_octets_contiguous(signer->signature,4,TC_TLV_BER,limits,
      workspace->frames,workspace->frame_capacity,work,workspace->signature,
      workspace->signature_capacity,&signature);
  return checked == TC_TLV_OK ? TC_X509_signature_verify_digest(signed_digest,
      &algorithm->signature,signature,key,provider,work) : tc_pki_signature_error(checked);
}

TC_TLV_result tc_cms_hash_content(TC_bytes input, TC_CMS_content_encoding encoding,
    TC_hash_algorithm algorithm, const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    tc_hash_workspace* scratch, uint8_t* digest)
{
  if (!tc_hash_available(algorithm)) return TC_TLV_UNSUPPORTED;
  if (encoding == TC_CMS_CONTENT_BER_OCTETS)
    return tc_pki_octets_hash(input,TC_TLV_BER,limits,tree->frames,tree->capacity,
        algorithm,scratch,tree->work,digest);
  if (encoding != TC_CMS_CONTENT_RAW) return TC_TLV_ARGUMENT;
  return tc_pki_hash_parts(&input,1,algorithm,limits,tree,scratch,digest);
}

TC_X509_signature_result tc_cms_signer_verify(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes input, tc_cms_verify_input input_kind,
    TC_CMS_verification_policy policy, const TC_X509_public_key* key,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_CMS_signature_workspace* workspace, size_t* work, TC_bytes* signer_name)
{
  enum { MAX_DIGEST_BYTES = 64 };
  tc_cms_signature_algorithm algorithm;
  tc_hash_workspace hash_workspace;
  tc_hash_info hash;
  uint8_t scratch[MAX_DIGEST_BYTES];
  TC_bytes digest = input;
  TC_TLV_result checked;
  TC_X509_signature_result result;
  if (!tc_cms_verification_policy_valid(policy))
    return TC_X509_SIGNATURE_ERROR;
  checked = cms_signature_storage(signer,content_type,input,key,provider,limits,workspace,work);
  if (checked != TC_TLV_OK) return tc_pki_signature_error(checked);
  if (!provider->verify_digest) return TC_X509_SIGNATURE_UNSUPPORTED;
  if (!content_type.data || !content_type.length ||
      (input_kind == TC_CMS_VERIFY_DIGEST && !input.data)) return TC_X509_SIGNATURE_ERROR;
  const tc_pki_tree_workspace tree = {workspace->frames,workspace->frame_capacity,work};
  checked = tc_cms_signature_resolve_policy(signer,key,TC_TLV_BER,limits,&tree,policy.rsa_parameters,&algorithm);
  if (checked != TC_TLV_OK) return tc_pki_signature_error(checked);
  if (!tc_hash_info_get(algorithm.content_hash,&hash)) return TC_X509_SIGNATURE_UNSUPPORTED;
  if (input_kind == TC_CMS_VERIFY_DIGEST) {
    if (digest.length != hash.digest_length) return TC_X509_SIGNATURE_ERROR;
  } else {
    checked = tc_cms_hash_content(input,input_kind == TC_CMS_VERIFY_BER ?
        TC_CMS_CONTENT_BER_OCTETS : TC_CMS_CONTENT_RAW,algorithm.content_hash,
        limits,&tree,&hash_workspace,scratch);
    if (checked != TC_TLV_OK) {
      result = tc_pki_signature_error(checked);
      goto done;
    }
    digest = (TC_bytes){scratch,hash.digest_length};
  }
  result = cms_verify_digest(signer,content_type,digest,policy.attributes,key,provider,limits,
      workspace,work,&algorithm,&hash_workspace,scratch,signer_name);
done:
  TC_secure_zero(scratch,sizeof scratch);
  TC_secure_zero(&hash_workspace,sizeof hash_workspace);
  return result;
}

TC_X509_signature_result TC_CMS_signer_verify_digest(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes digest, TC_CMS_attribute_encoding encoding,
    const TC_X509_public_key* key, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_CMS_signature_workspace* workspace, size_t* work)
{
  return TC_CMS_signer_verify_digest_with_policy(signer,content_type,digest,
      (TC_CMS_verification_policy){encoding,TC_CMS_RSA_PARAMETERS_NULL},key,provider,limits,workspace,work);
}

TC_X509_signature_result TC_CMS_signer_verify_digest_with_policy(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes digest, TC_CMS_verification_policy policy,
    const TC_X509_public_key* key, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_CMS_signature_workspace* workspace, size_t* work)
{
  return tc_cms_signer_verify(signer,content_type,digest,TC_CMS_VERIFY_DIGEST,policy,key,
      provider,limits,workspace,work,NULL);
}

TC_X509_signature_result TC_CMS_signer_verify_content(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes content, TC_CMS_content_encoding content_encoding,
    TC_CMS_attribute_encoding attribute_encoding, const TC_X509_public_key* key,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_CMS_signature_workspace* workspace, size_t* work)
{
  return TC_CMS_signer_verify_content_with_policy(signer,content_type,content,content_encoding,
      (TC_CMS_verification_policy){attribute_encoding,TC_CMS_RSA_PARAMETERS_NULL},key,provider,limits,workspace,work);
}

TC_X509_signature_result TC_CMS_signer_verify_content_with_policy(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes content, TC_CMS_content_encoding content_encoding,
    TC_CMS_verification_policy policy, const TC_X509_public_key* key,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_CMS_signature_workspace* workspace, size_t* work)
{
  tc_cms_verify_input input_kind;
  if (content_encoding == TC_CMS_CONTENT_RAW) input_kind = TC_CMS_VERIFY_RAW;
  else if (content_encoding == TC_CMS_CONTENT_BER_OCTETS) input_kind = TC_CMS_VERIFY_BER;
  else return TC_X509_SIGNATURE_ERROR;
  return tc_cms_signer_verify(signer,content_type,content,input_kind,policy,key,
      provider,limits,workspace,work,NULL);
}

TC_TLV_result TC_CMS_content_digest_check(const TC_CMS_signed_attributes* attributes,
    TC_bytes expected_type, TC_hash_algorithm algorithm, TC_bytes digest,
    size_t* work, int* matched)
{
  enum { INPUT_COUNT = 5, OUTPUT_COUNT = 2,
    STORAGE_WORK = INPUT_COUNT * OUTPUT_COUNT + 1 };
  TC_bytes inputs[INPUT_COUNT], outputs[OUTPUT_COUNT];
  size_t checks = STORAGE_WORK;
  if (!attributes || !work || !matched ||
      tc_pki_storage_span(attributes,1,sizeof *attributes,&inputs[0]) != TC_TLV_OK ||
      tc_pki_storage_span(work,1,sizeof *work,&outputs[0]) != TC_TLV_OK ||
      tc_pki_storage_span(matched,1,sizeof *matched,&outputs[1]) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  inputs[1] = expected_type;
  inputs[2] = digest;
  inputs[3] = attributes->content_type;
  inputs[4] = attributes->message_digest;
  if (!expected_type.data || !expected_type.length || !digest.data ||
      !attributes->content_type.data || !attributes->content_type.length ||
      !attributes->message_digest.data) return TC_TLV_ARGUMENT;
  /* Validate every range before writing the budget or comparison result. */
  if (tc_pki_storage_input(outputs,1,outputs[1],&checks) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  for (size_t i = 0; i < INPUT_COUNT; ++i)
    if (tc_pki_storage_input(outputs,OUTPUT_COUNT,inputs[i],&checks) != TC_TLV_OK)
      return TC_TLV_ARGUMENT;
  if (tc_x509_path_charge(work,STORAGE_WORK) != TC_TLV_OK) return TC_TLV_LIMIT;
  return tc_cms_content_digest_check(attributes,expected_type,algorithm,digest,work,matched);
}

TC_TLV_result TC_CMS_content_digest(TC_bytes encoded, TC_hash_algorithm algorithm,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t frame_capacity,
    size_t* work, uint8_t* digest, size_t digest_capacity)
{
  tc_hash_workspace scratch;
  tc_hash_info info;
  TC_TLV_result result = tc_pki_reader_storage(encoded,limits,frames,frame_capacity,
      work,digest,digest_capacity);
  if (result != TC_TLV_OK) return result;
  if (!tc_hash_available(algorithm) || !tc_hash_info_get(algorithm,&info))
    return TC_TLV_UNSUPPORTED;
  if (digest_capacity < info.digest_length) return TC_TLV_LIMIT;
  return tc_pki_octets_hash(encoded,TC_TLV_BER,limits,frames,frame_capacity,
      algorithm,&scratch,work,digest);
}

TC_TLV_result TC_CMS_signed_data_read(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_CMS_signed_data* out)
{
  TC_CMS_signed_data parsed;
  const tc_pki_tree_workspace tree = {frames,frame_capacity,work};
  TC_TLV_result result = tc_pki_reader_storage(encoded,limits,frames,frame_capacity,work,out,sizeof *out);
  if (result != TC_TLV_OK) return result;
  result = tc_cms_signed_data_read(encoded,limits,frames,frame_capacity,work,&parsed);
  if (result != TC_TLV_OK) return result;
  result = tc_cms_signed_data_version_check(&parsed,limits,&tree);
  if (result != TC_TLV_OK) return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_CMS_signer_info_read(TC_bytes encoded, TC_TLV_profile profile,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t frame_capacity,
    size_t* work, TC_CMS_signer_info* out)
{
  if (profile != TC_TLV_DER && profile != TC_TLV_BER) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_reader_storage(encoded,limits,frames,frame_capacity,work,out,sizeof *out);
  if (result != TC_TLV_OK) return result;
  return tc_cms_signer_info_read(encoded,profile,limits,frames,frame_capacity,work,out);
}

TC_TLV_result TC_CMS_signers_init(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_TLV_reader* out)
{
  TC_TLV_reader reader = {0};
  const tc_pki_tree_workspace tree = {frames,frame_capacity,work};
  TC_TLV_result result = tc_pki_reader_storage(encoded,limits,frames,frame_capacity,work,out,sizeof *out);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_tree_open(encoded,0x31,TC_TLV_BER,limits,&tree,&reader);
  if (result != TC_TLV_OK) return result;
  *out = reader;
  return TC_TLV_OK;
}

TC_TLV_result TC_CMS_signer_next(TC_TLV_reader* reader, TC_TLV_frame* frames,
    size_t frame_capacity, size_t* work, TC_CMS_signer_info* out)
{
  TC_bytes metadata;
  TC_TLV_reader next;
  TC_TLV_element element;
  TC_CMS_signer_info parsed;
  const tc_pki_tree_workspace tree = {frames,frame_capacity,work};
  TC_TLV_result result;
  if (!reader || tc_pki_storage_span(reader,1,sizeof *reader,&metadata) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  result = tc_pki_reader_storage_check(reader->input,reader,sizeof *reader,frames,frame_capacity,work,out,sizeof *out);
  if (result != TC_TLV_OK) return result;
  if (reader->profile != TC_TLV_BER || reader->offset > reader->input.length) return TC_TLV_ARGUMENT;
  /* Exhaustion needs no parsing budget and leaves all caller storage alone. */
  if (tc_pki_end(reader)) return TC_TLV_END;
  result = tc_x509_path_charge(work,TC_PKI_READER_STORAGE_WORK);
  if (result != TC_TLV_OK) return result;
  next = *reader;
  result = tc_pki_tree_next(&next,&tree,&element);
  if (result != TC_TLV_OK) return result;
  result = tc_cms_signer_info_read(element.encoded,TC_TLV_BER,&next.limits,
      frames,frame_capacity,work,&parsed);
  if (result != TC_TLV_OK) return result;
  *reader = next;
  *out = parsed;
  return TC_TLV_OK;
}


TC_TLV_result tc_cms_signed_data_read(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, tc_cms_signed_data* out)
{
  static const uint8_t signed_data_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,7,2};
  enum { SIGNED_DATA_FIELDS = 6 };
  TC_TLV_element fields[SIGNED_DATA_FIELDS];
  TC_bytes encap;
  tc_cms_signed_data parsed = {0};
  size_t count, index;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  parsed.encoded = encoded;
#define CMS_FIELDS(input, tag) \
  tc_pki_children(input,tag,TC_TLV_BER,limits,frames,frame_capacity,work,fields,SIGNED_DATA_FIELDS,&count)
  result = CMS_FIELDS(encoded,0x30);
  if (result != TC_TLV_OK) return result;
  if (count != 2 || !tc_pki_tag(&fields[0],6) ||
      !tc_pki_equal(fields[0].value,(TC_bytes){signed_data_oid,sizeof signed_data_oid}) ||
      !tc_pki_tag(&fields[1],0xa0)) return TC_TLV_INVALID;
  encoded = fields[1].encoded;
  result = CMS_FIELDS(encoded,0xa0);
  if (result != TC_TLV_OK) return result;
  if (count != 1 || !tc_pki_tag(&fields[0],0x30)) return TC_TLV_INVALID;
  encoded = fields[0].encoded;
  result = CMS_FIELDS(encoded,0x30);
  if (result != TC_TLV_OK) return result;
  if (count < 4 || !tc_pki_tag(&fields[0],2) || !tc_pki_tag(&fields[1],0x31) ||
      !tc_pki_tag(&fields[2],0x30)) return TC_TLV_INVALID;
  result = TC_DER_uint32_contents(fields[0].value.data,fields[0].value.length,&parsed.version);
  if (result != TC_TLV_OK) return result;
  parsed.digest_algorithms = fields[1].encoded;
  encap = fields[2].encoded;
  index = 3;
  if (index < count && tc_pki_tag(&fields[index],0xa0)) parsed.certificates = fields[index++].encoded;
  if (index < count && tc_pki_tag(&fields[index],0xa1)) parsed.revocations = fields[index++].encoded;
  if (index + 1 != count || !tc_pki_tag(&fields[index],0x31)) return TC_TLV_INVALID;
  parsed.signers = fields[index].encoded;
  result = CMS_FIELDS(encap,0x30);
  if (result != TC_TLV_OK) return result;
  if ((count != 1 && count != 2) || !tc_pki_tag(&fields[0],6) ||
      TC_DER_oid_contents(fields[0].value.data,fields[0].value.length) != TC_TLV_OK)
    return TC_TLV_INVALID;
  parsed.content_type = fields[0].value;
  if (count == 2) {
    if (!tc_pki_tag(&fields[1],0xa0)) return TC_TLV_INVALID;
    encoded = fields[1].encoded;
    result = CMS_FIELDS(encoded,0xa0);
    if (result != TC_TLV_OK) return result;
    if (count != 1 || (!tc_pki_tag(&fields[0],4) && !tc_pki_tag(&fields[0],0x24))) return TC_TLV_INVALID;
    parsed.has_content = 1;
    parsed.content = fields[0].encoded;
    result = tc_pki_octets(parsed.content,TC_TLV_BER,limits,frames,frame_capacity,work,NULL,NULL);
    if (result != TC_TLV_OK) return result;
  }
#undef CMS_FIELDS
  const tc_pki_tree_workspace tree = {frames,frame_capacity,work};
  result = tc_cms_digest_algorithms(parsed.digest_algorithms,NULL,limits,&tree,NULL);
  if (result != TC_TLV_OK) return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_classify_certificate(const TC_TLV_element* element,
    tc_cms_certificate_kind* out)
{
  if (tc_pki_tag(element,0x30)) *out = TC_CMS_CERT_X509;
  else if (tc_pki_tag(element,0xa0)) *out = TC_CMS_CERT_EXTENDED;
  else if (tc_pki_tag(element,0xa1)) *out = TC_CMS_CERT_ATTRIBUTE_V1;
  else if (tc_pki_tag(element,0xa2)) *out = TC_CMS_CERT_ATTRIBUTE_V2;
  else if (tc_pki_tag(element,0xa3)) *out = TC_CMS_CERT_OTHER;
  else return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_signed_data_check(const TC_CMS_signed_data* input,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    size_t signer_index, TC_CMS_signer_info* selected)
{
  enum { BASE_VERSION = 1, EXTENDED_VERSION = 3, ATTRIBUTE_V2_VERSION = 4, OTHER_VERSION = 5 };
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  uint32_t required;
  int found = 0;
  if (!input || !limits || !tree || !tree->work ||
      !input->content_type.data || !input->content_type.length) return TC_TLV_ARGUMENT;
  if (tc_x509_path_charge(tree->work,input->content_type.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  required = tc_pki_equal(input->content_type,(TC_bytes){cms_data_oid,sizeof cms_data_oid}) ?
      BASE_VERSION : EXTENDED_VERSION;
  if (input->certificates.length) {
    result = tc_pki_tree_open(input->certificates,0xa0,TC_TLV_BER,limits,tree,&reader);
    if (result != TC_TLV_OK) return result;
    while (!tc_pki_end(&reader)) {
      tc_cms_certificate_kind kind;
      uint32_t version = BASE_VERSION;
      result = tc_pki_tree_next(&reader,tree,&element);
      if (result != TC_TLV_OK) return result;
      result = tc_cms_classify_certificate(&element,&kind);
      if (result != TC_TLV_OK) return result;
      if (kind == TC_CMS_CERT_OTHER) version = OTHER_VERSION;
      else if (kind == TC_CMS_CERT_ATTRIBUTE_V2) version = ATTRIBUTE_V2_VERSION;
      else if (kind == TC_CMS_CERT_ATTRIBUTE_V1) version = EXTENDED_VERSION;
      if (version > required) required = version;
    }
  }
  if (input->revocations.length) {
    result = tc_pki_tree_open(input->revocations,0xa1,TC_TLV_BER,limits,tree,&reader);
    if (result != TC_TLV_OK) return result;
    while (!tc_pki_end(&reader)) {
      result = tc_pki_tree_next(&reader,tree,&element);
      if (result != TC_TLV_OK) return result;
      if (tc_pki_tag(&element,0xa1)) required = OTHER_VERSION;
      else if (!tc_pki_tag(&element,0x30)) return TC_TLV_INVALID;
    }
  }
  result = tc_pki_tree_open(input->signers,0x31,TC_TLV_BER,limits,tree,&reader);
  if (result != TC_TLV_OK) return result;
  while (!tc_pki_end(&reader)) {
    tc_cms_signer_info signer;
    result = tc_pki_tree_next(&reader,tree,&element);
    if (result != TC_TLV_OK) return result;
    result = tc_cms_signer_info_read(element.encoded,TC_TLV_BER,limits,
        tree->frames,tree->capacity,tree->work,&signer);
    if (result != TC_TLV_OK) return result;
    if (signer.version == EXTENDED_VERSION && required < EXTENDED_VERSION)
      required = EXTENDED_VERSION;
    if (selected && !found) {
      if (signer_index) --signer_index;
      else { *selected = signer; found = 1; }
    }
  }
  return input->version == required && (!selected || found) ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result tc_cms_signed_data_version_check(const tc_cms_signed_data* input,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree)
{
  return tc_cms_signed_data_check(input,limits,tree,0,NULL);
}

static void cms_definite(void* user, const TC_TLV_event* event)
{
  if (event->kind == TC_TLV_BEGIN && event->header.indefinite) *(int*)user = 0;
}

static TC_TLV_result cms_open(TC_bytes encoded, unsigned tag, TC_TLV_profile profile, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_TLV_element* outer)
{
  int definite = 1;
  TC_TLV_result result = TC_TLV_read(encoded.data,encoded.length,profile,limits,outer);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(outer,tag) || outer->encoded.length != encoded.length || !outer->value.length)
    return TC_TLV_INVALID;
  if (tc_x509_path_charge(work,encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = TC_TLV_walk(encoded.data,encoded.length,profile,limits,frames,frame_capacity,cms_definite,&definite);
  return result != TC_TLV_OK ? result : definite ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result cms_octet_count(void* context, TC_bytes bytes)
{
  size_t* count = context;
  if (bytes.length > SIZE_MAX - *count) return TC_TLV_LIMIT;
  *count += bytes.length;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_signer_info_read(TC_bytes encoded, TC_TLV_profile profile, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, tc_cms_signer_info* out)
{
  tc_cms_signer_info parsed = {0};
  const tc_pki_tree_workspace workspace = {frames,frame_capacity,work};
  TC_TLV_element element;
  TC_TLV_reader fields, identifier;
  size_t octets;
  TC_TLV_result result;
  if (!limits || !work || !out || (profile != TC_TLV_DER && profile != TC_TLV_BER))
    return TC_TLV_ARGUMENT;
  parsed.encoded = encoded;
#define CMS_TRY(call) do { result = (call); if (result != TC_TLV_OK) \
  return result == TC_TLV_END ? TC_TLV_INVALID : result; } while (0)
  CMS_TRY(tc_pki_tree_open(encoded,0x30,profile,limits,&workspace,&fields));
  CMS_TRY(tc_pki_tree_field(&fields,2,&workspace,&element));
  CMS_TRY(TC_DER_uint32_contents(element.value.data,element.value.length,&parsed.version));
  CMS_TRY(tc_pki_tree_next(&fields,&workspace,&element));
  if (tc_pki_tag(&element,0x80) || tc_pki_tag(&element,0xa0)) {
    if (parsed.version != 3) return TC_TLV_INVALID;
    octets = 0;
    CMS_TRY(tc_pki_octets_implicit(element.encoded,0x80,profile,limits,frames,
        frame_capacity,work,cms_octet_count,&octets));
    if (!octets) return TC_TLV_INVALID;
    parsed.subject_key_id = element.encoded;
  } else if (tc_pki_tag(&element,0x30)) {
    if (parsed.version != 1) return TC_TLV_INVALID;
    CMS_TRY(TC_TLV_reader_init(&identifier,element.value.data,element.value.length,profile,limits));
    CMS_TRY(tc_pki_tree_field(&identifier,0x30,&workspace,&element));
    parsed.issuer = element.encoded;
    CMS_TRY(tc_pki_tree_name(parsed.issuer,profile,limits,&workspace));
    CMS_TRY(tc_pki_tree_field(&identifier,2,&workspace,&element));
    if (!tc_pki_end(&identifier)) return TC_TLV_INVALID;
    CMS_TRY(TC_DER_integer_contents(element.value.data,element.value.length));
    parsed.serial = element.value;
    parsed.serial_negative = (element.value.data[0] & 0x80) != 0;
  } else return TC_TLV_INVALID;
  CMS_TRY(tc_pki_tree_field(&fields,0x30,&workspace,&element));
  CMS_TRY(tc_pki_tree_algorithm(element.encoded,profile,limits,&workspace,&parsed.digest_algorithm));
  CMS_TRY(tc_pki_tree_next(&fields,&workspace,&element));
  if (tc_pki_tag(&element,0xa0)) {
    if (!element.value.length) return TC_TLV_INVALID;
    parsed.signed_attributes = element.encoded;
    CMS_TRY(tc_pki_tree_next(&fields,&workspace,&element));
  }
  if (!tc_pki_tag(&element,0x30)) return TC_TLV_INVALID;
  CMS_TRY(tc_pki_tree_algorithm(element.encoded,profile,limits,&workspace,&parsed.signature_algorithm));
  CMS_TRY(tc_pki_tree_next(&fields,&workspace,&element));
  octets = 0;
  CMS_TRY(tc_pki_octets(element.encoded,profile,limits,frames,frame_capacity,work,cms_octet_count,&octets));
  if (!octets) return TC_TLV_INVALID;
  parsed.signature = element.encoded;
  if (!tc_pki_end(&fields)) {
    CMS_TRY(tc_pki_tree_field(&fields,0xa1,&workspace,&element));
    if (!element.value.length || !tc_pki_end(&fields)) return TC_TLV_INVALID;
    parsed.unsigned_attributes = element.encoded;
  }
#undef CMS_TRY
  *out = parsed;
  return TC_TLV_OK;
}

/* RFC 8551 section 2.5.2: preserve capability order and optional parameters. */
static TC_TLV_result cms_capabilities(TC_bytes encoded, TC_TLV_profile profile,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree)
{
  TC_TLV_reader capabilities;
  TC_TLV_element element;
  tc_pki_oid_value capability;
  TC_TLV_result result = tc_pki_tree_open(encoded,0x30,profile,limits,tree,&capabilities);
  if (result != TC_TLV_OK) return result;
  while ((result = tc_pki_tree_next(&capabilities,tree,&element)) == TC_TLV_OK) {
    result = tc_pki_tree_oid_value(element.encoded,0x30,profile,limits,tree,&capability);
    if (result != TC_TLV_OK) return result;
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

TC_TLV_result TC_CMS_signed_attributes_read(TC_bytes encoded,
    TC_CMS_attribute_encoding encoding, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_CMS_signed_attributes* out)
{
  static const uint8_t set_tag = 0x31;
  static const uint8_t attribute_prefix[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,9};
  static const uint8_t entry_uuid_oid[] = {0x2b,6,1,1,0x10,4}; /* RFC 4530. */
  enum { CONTENT_TYPE = 3, MESSAGE_DIGEST = 4, SIGNING_TIME = 5,
         SMIME_CAPABILITIES = 15, SIGNER_NAME = 256, FASCN, ENTRY_UUID,
         FASCN_BYTES = 25, UUID_BYTES = 16 };
  TC_CMS_signed_attributes parsed = {0};
  TC_TLV_element outer, attribute, element, value;
  TC_TLV_reader attributes, fields, values;
  TC_bytes previous = {NULL,0}, oid;
  TC_TLV_result result;
  TC_TLV_profile profile;
  if (!limits || !work || !out) return TC_TLV_ARGUMENT;
  if (encoding != TC_CMS_ATTRIBUTES_DER && encoding != TC_CMS_ATTRIBUTES_BER_DEFINITE_ORDER)
    return TC_TLV_ARGUMENT;
  result = tc_pki_reader_storage(encoded,limits,frames,frame_capacity,work,out,sizeof *out);
  if (result != TC_TLV_OK) return result;
  profile = encoding == TC_CMS_ATTRIBUTES_DER ? TC_TLV_DER : TC_TLV_BER;
  result = cms_open(encoded,0xa0,profile,limits,frames,frame_capacity,work,&outer);
  if (result != TC_TLV_OK) return result;
  result = TC_TLV_reader_init(&attributes,outer.value.data,outer.value.length,profile,limits);
  if (result != TC_TLV_OK) return result;
  while ((result = TC_TLV_next(&attributes,&attribute)) == TC_TLV_OK) {
    unsigned kind;
    if (tc_x509_path_charge(work,attribute.encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (!tc_pki_tag(&attribute,0x30) ||
        (encoding == TC_CMS_ATTRIBUTES_DER && previous.data &&
         tc_pki_compare(previous,attribute.encoded) > 0)) return TC_TLV_INVALID;
    previous = attribute.encoded;
    result = TC_TLV_reader_init(&fields,attribute.value.data,attribute.value.length,profile,limits);
    if (result != TC_TLV_OK) return result;
    if (tc_pki_next(&fields,6,&element) != TC_TLV_OK ||
        TC_DER_oid_contents(element.value.data,element.value.length) != TC_TLV_OK)
      return TC_TLV_INVALID;
    oid = element.value;
    if (tc_pki_next(&fields,0x31,&element) != TC_TLV_OK || !tc_pki_end(&fields) || !element.value.length)
      return TC_TLV_INVALID;
    const TC_PIV_oid identity = TC_PIV_oid_identify(oid,TC_PIV_OIDS_TWIC_COMPATIBLE);
    if (identity == TC_PIV_OID_SIGNER_NAME) kind = SIGNER_NAME;
    else if (identity == TC_PIV_OID_FASCN) kind = FASCN;
    else if (tc_pki_equal(oid,(TC_bytes){entry_uuid_oid,sizeof entry_uuid_oid})) kind = ENTRY_UUID;
    else if (oid.length == sizeof attribute_prefix + 1 && !memcmp(oid.data,attribute_prefix,sizeof attribute_prefix))
      kind = oid.data[sizeof attribute_prefix];
    else return TC_TLV_UNSUPPORTED;
    if (kind != CONTENT_TYPE && kind != MESSAGE_DIGEST && kind != SIGNING_TIME &&
        kind != SMIME_CAPABILITIES && kind != SIGNER_NAME && kind != FASCN &&
        kind != ENTRY_UUID) return TC_TLV_UNSUPPORTED;
    result = TC_TLV_reader_init(&values,element.value.data,element.value.length,profile,limits);
    if (result != TC_TLV_OK) return result;
    if (TC_TLV_next(&values,&value) != TC_TLV_OK || !tc_pki_end(&values)) return TC_TLV_INVALID;
    if (kind == CONTENT_TYPE) {
      if (parsed.content_type.data || !tc_pki_tag(&value,6) ||
          TC_DER_oid_contents(value.value.data,value.value.length) != TC_TLV_OK)
        return TC_TLV_INVALID;
      parsed.content_type = value.value;
    } else if (kind == MESSAGE_DIGEST) {
      if (parsed.message_digest.data || !tc_pki_tag(&value,4) || !value.value.length) return TC_TLV_INVALID;
      parsed.message_digest = value.value;
    } else if (kind == SIGNING_TIME) {
      if (parsed.has_signing_time || tc_x509_time_value(&value,&parsed.signing_time) != TC_TLV_OK)
        return TC_TLV_INVALID;
      if (tc_pki_tag(&value,0x18) && parsed.signing_time.year >= 1950 && parsed.signing_time.year <= 2049)
        return TC_TLV_INVALID;
      parsed.has_signing_time = 1;
    } else if (kind == FASCN || kind == ENTRY_UUID) {
      tc_pki_octets_storage octets = {{NULL,0},0,0,NULL,0,work};
      TC_bytes* destination = kind == FASCN ? &parsed.fascn_octets : &parsed.entry_uuid_octets;
      const size_t required = kind == FASCN ? FASCN_BYTES : UUID_BYTES;
      if (destination->data) return TC_TLV_INVALID;
      result = tc_pki_octets(value.encoded,profile,limits,frames,frame_capacity,
          work,tc_pki_octets_store_chunk,&octets);
      if (result != TC_TLV_OK) return result;
      if (octets.length != required) return TC_TLV_INVALID;
      if (kind == FASCN) parsed.fascn_oid = oid;
      *destination = value.encoded;
    } else if (kind == SIGNER_NAME) {
      const tc_pki_tree_workspace tree = {frames,frame_capacity,work};
      if (parsed.signer_name.data) return TC_TLV_INVALID;
      result = tc_pki_tree_name(value.encoded,profile,limits,&tree);
      if (result != TC_TLV_OK) return result;
      parsed.signer_name = value.encoded;
    } else {
      const tc_pki_tree_workspace tree = {frames,frame_capacity,work};
      if (parsed.smime_capabilities.data) return TC_TLV_INVALID;
      result = cms_capabilities(value.encoded,profile,limits,&tree);
      if (result != TC_TLV_OK) return result;
      parsed.smime_capabilities = value.encoded;
    }
  }
  if (result != TC_TLV_END) return result;
  if (!parsed.content_type.data || !parsed.message_digest.data) return TC_TLV_INVALID;
  /* RFC 5652 section 5.4 signs SET OF, not the wire's implicit [0] tag. */
  parsed.signature_input[0] = (TC_bytes){&set_tag,1};
  parsed.signature_input[1] = (TC_bytes){encoded.data + 1,encoded.length - 1};
  *out = parsed;
  return TC_TLV_OK;
}

#endif
