/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/piv_cms.h>
#if TC_ENABLE_PIV_OBJECTS
#include "pki_reader_internal.h"
#include "pki_tree_internal.h"
#include "pki_octets_internal.h"
#include "cms_base_internal.h"

TC_TLV_result TC_PIV_CBEFF_read(TC_bytes encoded, TC_PIV_CBEFF* out)
{
  enum { HEADER_BYTES = 88, HEADER_VERSION = 3, RECORD_LENGTH_OFFSET = 2,
    RECORD_LENGTH_BYTES = 4, SIGNATURE_LENGTH_OFFSET = 6, FASCN_OFFSET = 59, FASCN_BYTES = 25 };
  TC_bytes input, output;
  if (!out || tc_pki_storage_span(encoded.data,encoded.length,1,&input) != TC_TLV_OK ||
      tc_pki_storage_span(out,1,sizeof *out,&output) != TC_TLV_OK ||
      !tc_internal_ranges_disjoint(input.data,input.length,output.data,output.length)) return TC_TLV_ARGUMENT;
  if (encoded.length < HEADER_BYTES) return TC_TLV_INVALID;
  if (encoded.data[0] != HEADER_VERSION) return TC_TLV_UNSUPPORTED;
  uint32_t record_bytes = 0;
  for (size_t i = 0; i < RECORD_LENGTH_BYTES; ++i)
    record_bytes = (record_bytes << 8) | encoded.data[RECORD_LENGTH_OFFSET + i];
  const size_t signature_bytes = ((size_t)encoded.data[SIGNATURE_LENGTH_OFFSET] << 8) |
      encoded.data[SIGNATURE_LENGTH_OFFSET + 1];
  if (!record_bytes || !signature_bytes || record_bytes > encoded.length - HEADER_BYTES ||
      signature_bytes != encoded.length - HEADER_BYTES - record_bytes) return TC_TLV_INVALID;
  const size_t signed_bytes = HEADER_BYTES + (size_t)record_bytes;
  const TC_PIV_CBEFF parsed = {
    {encoded.data,signed_bytes}, {encoded.data + HEADER_BYTES,(size_t)record_bytes},
    {encoded.data + signed_bytes,signature_bytes}, {encoded.data + FASCN_OFFSET,FASCN_BYTES}
  };
  *out = parsed;
  return TC_TLV_OK;
}

static int cbeff_time(const uint8_t* bytes, TC_X509_time* out)
{
  if (bytes[0] > 99 || bytes[1] > 99 || bytes[7] != 'Z') return 0;
  *out = (TC_X509_time){(unsigned)bytes[0] * 100 + bytes[1],
    bytes[2],bytes[3],bytes[4],bytes[5],bytes[6]};
  return 1;
}

TC_TLV_result TC_PIV_CBEFF_metadata_read(TC_bytes encoded, TC_PIV_CBEFF_metadata* out)
{
  enum { SECURITY_OFFSET = 1, SIGNED_PLAIN = 0x0d, SIGNED_ENCRYPTED = 0x0f,
    FORMAT_OWNER_OFFSET = 8, FORMAT_TYPE_OFFSET = 10, CREATED_OFFSET = 12,
    VALID_FROM_OFFSET = 20, VALID_UNTIL_OFFSET = 28, BIOMETRIC_TYPE_OFFSET = 36,
    DATA_TYPE_OFFSET = 39, QUALITY_OFFSET = 40, CREATOR_OFFSET = 41,
    CREATOR_BYTES = 18, RESERVED_OFFSET = 84, RESERVED_BYTES = 4 };
  TC_PIV_CBEFF container;
  TC_PIV_CBEFF_metadata parsed = {0};
  TC_bytes input, output;
  int order;
  if (!out || tc_pki_storage_span(encoded.data,encoded.length,1,&input) != TC_TLV_OK ||
      tc_pki_storage_span(out,1,sizeof *out,&output) != TC_TLV_OK ||
      !tc_internal_ranges_disjoint(input.data,input.length,output.data,output.length)) return TC_TLV_ARGUMENT;
  TC_TLV_result result = TC_PIV_CBEFF_read(encoded,&container);
  if (result != TC_TLV_OK) return result;
  const uint8_t* bytes = container.signed_content.data;
  if (bytes[SECURITY_OFFSET] != SIGNED_PLAIN && bytes[SECURITY_OFFSET] != SIGNED_ENCRYPTED)
    return TC_TLV_INVALID;
  parsed.encrypted = bytes[SECURITY_OFFSET] == SIGNED_ENCRYPTED;
  if (!cbeff_time(bytes + CREATED_OFFSET,&parsed.created) ||
      !cbeff_time(bytes + VALID_FROM_OFFSET,&parsed.valid_from) ||
      !cbeff_time(bytes + VALID_UNTIL_OFFSET,&parsed.valid_until) ||
      TC_X509_time_compare(&parsed.created,&parsed.valid_from,&order) != TC_TLV_OK || order > 0 ||
      TC_X509_time_compare(&parsed.valid_from,&parsed.valid_until,&order) != TC_TLV_OK || order > 0)
    return TC_TLV_INVALID;
  parsed.quality = bytes[QUALITY_OFFSET] < 128 ? bytes[QUALITY_OFFSET] : (int)bytes[QUALITY_OFFSET] - 256;
  if (parsed.quality < -2 || parsed.quality > 100) return TC_TLV_INVALID;
  size_t creator_bytes = 0;
  while (creator_bytes < CREATOR_BYTES && bytes[CREATOR_OFFSET + creator_bytes]) {
    const uint8_t value = bytes[CREATOR_OFFSET + creator_bytes];
    if (value < 0x20 || value > 0x7e) return TC_TLV_INVALID;
    ++creator_bytes;
  }
  if (creator_bytes == CREATOR_BYTES) return TC_TLV_INVALID;
  parsed.creator = (TC_bytes){bytes + CREATOR_OFFSET,creator_bytes};
  for (size_t i = 0; i < RESERVED_BYTES; ++i)
    if (bytes[RESERVED_OFFSET + i]) return TC_TLV_INVALID;
  parsed.format_owner = (uint16_t)((uint16_t)bytes[FORMAT_OWNER_OFFSET] << 8) | bytes[FORMAT_OWNER_OFFSET + 1];
  parsed.format_type = (uint16_t)((uint16_t)bytes[FORMAT_TYPE_OFFSET] << 8) | bytes[FORMAT_TYPE_OFFSET + 1];
  parsed.biometric_type = ((uint32_t)bytes[BIOMETRIC_TYPE_OFFSET] << 16) |
      ((uint32_t)bytes[BIOMETRIC_TYPE_OFFSET + 1] << 8) | bytes[BIOMETRIC_TYPE_OFFSET + 2];
  parsed.data_type = bytes[DATA_TYPE_OFFSET];
  *out = parsed;
  return TC_TLV_OK;
}

TC_PIV_CBEFF_format TC_PIV_CBEFF_format_identify(const TC_PIV_CBEFF_metadata* metadata)
{
  enum { OWNER_INCITS = 0x001b, OWNER_ISO = 0x0101,
    FORMAT_FINGERPRINT_IMAGE = 0x0401, FORMAT_FINGERPRINT_TEMPLATE = 0x0201,
    FORMAT_IRIS_IMAGE = 0x0009, FORMAT_FACE_IMAGE = 0x0501,
    TYPE_FINGERPRINT = 0x000008, TYPE_IRIS = 0x000010, TYPE_FACE = 0x000002,
    PROCESSING_MASK = 0xe0, RAW = 0x20, PROCESSED = 0x80, INTERMEDIATE = 0x40 };
  static const struct {
    uint16_t owner, type;
    uint8_t biometric, processing;
    TC_PIV_CBEFF_format format;
  } formats[] = {
    {OWNER_INCITS,FORMAT_FINGERPRINT_IMAGE,TYPE_FINGERPRINT,RAW,TC_PIV_CBEFF_FINGERPRINT_IMAGE},
    {OWNER_INCITS,FORMAT_FINGERPRINT_TEMPLATE,TYPE_FINGERPRINT,PROCESSED,TC_PIV_CBEFF_FINGERPRINT_TEMPLATE},
    {OWNER_ISO,FORMAT_IRIS_IMAGE,TYPE_IRIS,INTERMEDIATE,TC_PIV_CBEFF_IRIS_IMAGE},
    {OWNER_INCITS,FORMAT_FACE_IMAGE,TYPE_FACE,RAW,TC_PIV_CBEFF_FACE_IMAGE}
  };
  if (!metadata) return TC_PIV_CBEFF_FORMAT_UNKNOWN;
  for (size_t i = 0; i < sizeof formats / sizeof *formats; ++i)
    if (metadata->format_owner == formats[i].owner && metadata->format_type == formats[i].type &&
        metadata->biometric_type == formats[i].biometric &&
        (metadata->data_type & PROCESSING_MASK) == formats[i].processing) return formats[i].format;
  return TC_PIV_CBEFF_FORMAT_UNKNOWN;
}

static int cms_kind_valid(TC_PIV_CMS_kind kind)
{
  return kind == TC_PIV_CMS_CHUID || kind == TC_PIV_CMS_BIOMETRIC ||
      kind == TC_PIV_CMS_BIOMETRIC_LEGACY || kind == TC_PIV_CMS_SECURITY;
}

static int cms_identifiers_present(TC_PIV_CMS_kind kind, TC_bytes fascn, TC_bytes uuid)
{
  return kind == TC_PIV_CMS_CHUID || kind == TC_PIV_CMS_SECURITY || (fascn.data &&
      (kind == TC_PIV_CMS_BIOMETRIC_LEGACY || uuid.data));
}

TC_TLV_result TC_PIV_CMS_read(TC_bytes encoded, TC_PIV_CMS_kind kind,
    TC_PIV_oid_profile oids, TC_CMS_attribute_encoding attributes,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t frame_capacity,
    size_t* work, TC_PIV_CMS_object* out)
{
  TC_PIV_CMS_object parsed = {0};
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!cms_kind_valid(kind) ||
      (oids != TC_PIV_OIDS_ONLY && oids != TC_PIV_OIDS_TWIC_COMPATIBLE) ||
      (attributes != TC_CMS_ATTRIBUTES_DER && attributes != TC_CMS_ATTRIBUTES_BER_DEFINITE_ORDER))
    return TC_TLV_ARGUMENT;
  result = tc_pki_reader_storage(encoded,limits,frames,frame_capacity,work,out,sizeof *out);
  if (result != TC_TLV_OK) return result;
  const tc_pki_tree_workspace tree = {frames,frame_capacity,work};
  result = TC_CMS_signed_data_read(encoded,limits,frames,frame_capacity,work,&parsed.envelope);
  if (result != TC_TLV_OK) return result;
  const int security = kind == TC_PIV_CMS_SECURITY;
  if (parsed.envelope.version != 3 || parsed.envelope.has_content != security ||
      parsed.envelope.revocations.data) return TC_TLV_INVALID;
  if (security) {
    /* PIV id-icao-ldsSecurityObject, SP 800-85B AS06.04.06. */
    static const uint8_t lds_type[] = {0x2b,0x1b,1,1,1};
    if (!tc_pki_equal(parsed.envelope.content_type,(TC_bytes){lds_type,sizeof lds_type}) ||
        parsed.envelope.certificates.data) return TC_TLV_INVALID;
  } else if (TC_PIV_oid_identify(parsed.envelope.content_type,oids) !=
      (kind == TC_PIV_CMS_CHUID ? TC_PIV_OID_CHUID_CONTENT : TC_PIV_OID_BIOMETRIC_CONTENT))
    return TC_TLV_INVALID;
  if (parsed.envelope.certificates.data) {
    result = tc_pki_tree_open(parsed.envelope.certificates,0xa0,TC_TLV_BER,limits,&tree,&reader);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_tree_field(&reader,0x30,&tree,&element);
    if (result != TC_TLV_OK) return result;
    parsed.certificate = element.encoded;
    result = tc_pki_tree_next(&reader,&tree,&element);
    if (result != TC_TLV_END) return result == TC_TLV_OK ? TC_TLV_INVALID : result;
  } else if (kind == TC_PIV_CMS_CHUID) return TC_TLV_INVALID;
  result = TC_CMS_signers_init(parsed.envelope.signers,limits,frames,frame_capacity,work,&reader);
  if (result != TC_TLV_OK) return result;
  result = TC_CMS_signer_next(&reader,frames,frame_capacity,work,&parsed.signer);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (!security && parsed.signer.version != 1) return TC_TLV_INVALID;
  result = TC_CMS_signer_next(&reader,frames,frame_capacity,work,&parsed.signer);
  if (result != TC_TLV_END) return result == TC_TLV_OK ? TC_TLV_INVALID : result;
  result = TC_CMS_signed_attributes_read(parsed.signer.signed_attributes,attributes,
      limits,frames,frame_capacity,work,&parsed.attributes);
  if (result != TC_TLV_OK) return result;
  result = tc_cms_digest_algorithms(parsed.envelope.digest_algorithms,
      &parsed.signer.digest_algorithm,limits,&tree,NULL);
  if (result != TC_TLV_OK) return result;
  if ((!security && !parsed.attributes.signer_name.data) ||
      !tc_pki_equal(parsed.attributes.content_type,parsed.envelope.content_type)) return TC_TLV_INVALID;
  if (parsed.attributes.fascn_oid.data &&
      TC_PIV_oid_identify(parsed.attributes.fascn_oid,oids) != TC_PIV_OID_FASCN) return TC_TLV_INVALID;
  if (!cms_identifiers_present(kind,parsed.attributes.fascn_octets,parsed.attributes.entry_uuid_octets))
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_CMS_identifiers_match(const TC_PIV_CMS_object* object,
    TC_PIV_CMS_kind kind, TC_bytes fascn, TC_bytes uuid, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, int* matched)
{
  enum { FASCN_BYTES = 25, UUID_BYTES = 16, WRITE_COUNT = 3, READ_COUNT = 7,
    STORAGE_WORK = WRITE_COUNT * READ_COUNT + WRITE_COUNT * (WRITE_COUNT - 1) / 2 };
  TC_bytes writes[WRITE_COUNT], reads[READ_COUNT];
  size_t checks = STORAGE_WORK;
  int fascn_matches = 1, uuid_matches = 1;
  TC_TLV_result result;
  if (!object || !limits || !work || !matched || !fascn.data || !uuid.data ||
      !cms_kind_valid(kind) ||
      fascn.length != FASCN_BYTES || uuid.length != UUID_BYTES ||
      tc_pki_storage_span(frames,frame_capacity,sizeof *frames,&writes[0]) != TC_TLV_OK ||
      tc_pki_storage_span(work,1,sizeof *work,&writes[1]) != TC_TLV_OK ||
      tc_pki_storage_span(matched,1,sizeof *matched,&writes[2]) != TC_TLV_OK ||
      tc_pki_storage_span(object,1,sizeof *object,&reads[0]) != TC_TLV_OK ||
      tc_pki_storage_span(limits,1,sizeof *limits,&reads[1]) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  reads[2] = fascn; reads[3] = uuid;
  reads[4] = object->attributes.fascn_octets;
  reads[5] = object->attributes.entry_uuid_octets;
  reads[6] = object->envelope.encoded;
  for (size_t i = 0; i < WRITE_COUNT; ++i) {
    result = tc_pki_storage_input(writes,i,writes[i],&checks);
    if (result != TC_TLV_OK) return result;
  }
  for (size_t i = 0; i < READ_COUNT; ++i) {
    result = tc_pki_storage_input(writes,WRITE_COUNT,reads[i],&checks);
    if (result != TC_TLV_OK) return result;
  }
  if (!cms_identifiers_present(kind,reads[4],reads[5])) return TC_TLV_INVALID;
  result = tc_x509_path_charge(work,STORAGE_WORK);
  if (result != TC_TLV_OK) return result;
  if (reads[4].data) {
    result = tc_pki_octets_equal(reads[4],4,fascn,TC_TLV_BER,limits,frames,frame_capacity,work,&fascn_matches);
    if (result != TC_TLV_OK) return result;
  }
  if (reads[5].data) {
    result = tc_pki_octets_equal(reads[5],4,uuid,TC_TLV_BER,limits,frames,frame_capacity,work,&uuid_matches);
    if (result != TC_TLV_OK) return result;
  }
  *matched = fascn_matches && uuid_matches;
  return TC_TLV_OK;
}
#endif
