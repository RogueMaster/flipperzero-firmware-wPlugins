/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_EAC_CVC
#include <tiny_crypto/eac_cvc.h>
#include "pki_internal.h"

static const TC_TLV_limits unbounded = {SIZE_MAX,SIZE_MAX,SIZE_MAX,SIZE_MAX};
static TC_TLV_result open(TC_bytes span, TC_TLV_reader* reader)
{ return TC_TLV_reader_init(reader, span.data, span.length, TC_TLV_DER, &unbounded); }
static int field(TC_TLV_reader* reader, unsigned tag, TC_TLV_element* element)
{ return tc_pki_next(reader, tag, element) == TC_TLV_OK; }
static int uint_value(TC_bytes value, int zero)
{
  return value.length && (value.length == 1 ? zero || value.data[0] != 0 : value.data[0] != 0);
}
static int point_width(TC_bytes point, TC_bytes prime)
{
  return point.data && prime.length && prime.length <= (SIZE_MAX - 1) / 2 &&
    point.length == 1 + 2 * prime.length && point.data[0] == 4;
}
static TC_TLV_result key_contents(TC_bytes contents, int standalone, TC_EAC_CVC_public_key* out)
{
  static const uint8_t prefix[] = {4,0,0x7f,0,7,2,2};
  TC_EAC_CVC_public_key key;
  TC_TLV_reader reader;
  TC_TLV_element element;
  unsigned usage, family, id;
  memset(&key, 0, sizeof key);
  if (open(contents, &reader) != TC_TLV_OK || !field(&reader, 6, &element) ||
      TC_DER_oid_contents(element.value.data, element.value.length) != TC_TLV_OK) return TC_TLV_INVALID;
  key.oid = element.value;
  if (key.oid.length != 10 || memcmp(key.oid.data, prefix, sizeof prefix)) return TC_TLV_UNSUPPORTED;
  usage = key.oid.data[7]; family = key.oid.data[8]; id = key.oid.data[9];
  if (usage != 2 && !(standalone && usage == 5 && family == 2)) return TC_TLV_UNSUPPORTED;
  if (usage == 2 && family == 1 && id >= 1 && id <= 6) {
    key.algorithm = id == 3 || id == 4 || id == 6 ? TC_EAC_RSA_PSS : TC_EAC_RSA_V15;
    key.hash_bits = id == 1 || id == 3 ? 160 : id == 2 || id == 4 ? 256 : 512;
    if (!field(&reader, 0x81, &element) || !uint_value(element.value, 0)) return TC_TLV_INVALID;
    key.modulus = element.value;
    if (!field(&reader, 0x82, &element) || !uint_value(element.value, 0)) return TC_TLV_INVALID;
    key.exponent = element.value;
    if (!(key.modulus.data[key.modulus.length - 1] & 1) ||
        !(key.exponent.data[key.exponent.length - 1] & 1) ||
        (key.exponent.length == 1 && key.exponent.data[0] < 3) ||
        key.exponent.length > key.modulus.length ||
        (key.exponent.length == key.modulus.length &&
         memcmp(key.exponent.data, key.modulus.data, key.modulus.length) >= 0)) return TC_TLV_INVALID;
  } else if (family == 2 && id >= 1 && id <= 5) {
    static const unsigned hashes[] = {160,224,256,384,512};
    key.algorithm = usage == 2 ? TC_EAC_ECDSA : TC_EAC_ECDH; key.hash_bits = hashes[id - 1];
    if (TC_TLV_next(&reader, &element) != TC_TLV_OK) return TC_TLV_INVALID;
    if (tc_pki_tag(&element, 0x81)) {
      key.has_domain = 1; key.p = element.value;
      if (!uint_value(key.p, 0)) return TC_TLV_INVALID;
      if (!field(&reader, 0x82, &element) || !uint_value(element.value, 1)) return TC_TLV_INVALID;
      key.a = element.value;
      if (!field(&reader, 0x83, &element) || !uint_value(element.value, 1)) return TC_TLV_INVALID;
      key.b = element.value;
      if (!field(&reader, 0x84, &element)) return TC_TLV_INVALID;
      key.generator = element.value;
      if (!point_width(key.generator, key.p)) return TC_TLV_INVALID;
      if (!field(&reader, 0x85, &element) || !uint_value(element.value, 0)) return TC_TLV_INVALID;
      key.order = element.value;
      if (TC_TLV_next(&reader, &element) != TC_TLV_OK) return TC_TLV_INVALID;
    }
    if (!tc_pki_tag(&element, 0x86)) return TC_TLV_INVALID;
    key.point = element.value;
    if (key.point.length < 3 || !(key.point.length & 1) || key.point.data[0] != 4 ||
        (key.has_domain && !point_width(key.point, key.p))) return TC_TLV_INVALID;
    if (!tc_pki_end(&reader)) {
      if (!key.has_domain || !field(&reader, 0x87, &element) || !uint_value(element.value, 0)) return TC_TLV_INVALID;
      key.cofactor = element.value;
    }
  } else return TC_TLV_UNSUPPORTED;
  if (!tc_pki_end(&reader)) return TC_TLV_INVALID;
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_EAC_CVC_public_key_read(const uint8_t* data, size_t length,
    const TC_TLV_limits* limits, TC_EAC_CVC_public_key* out)
{
  TC_TLV_element element;
  TC_TLV_frame frame;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_TLV_read(data, length, TC_TLV_DER, limits, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x7f49) || element.encoded.length != length) return TC_TLV_INVALID;
  result = TC_TLV_walk(data, length, TC_TLV_DER, limits, &frame, 1, NULL, NULL);
  if (result != TC_TLV_OK) return result;
  return key_contents(element.value, 1, out);
}

static int reference(TC_bytes value)
{
  size_t i;
  if (value.length < 7 || value.length > 16) return 0;
  for (i = 0; i < value.length; ++i) {
    uint8_t c = value.data[i];
    if (c < 0x20 || (c >= 0x7f && c <= 0x9f)) return 0;
    if (i < 2 && (c < 'A' || c > 'Z')) return 0;
    if (i >= value.length - 5 && !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) return 0;
  }
  return 1;
}
static int date(TC_bytes value, TC_EAC_date* out)
{
  unsigned i, year, month, day;
  if (value.length != 6) return 0;
  for (i = 0; i < 6; ++i) if (value.data[i] > 9) return 0;
  year = 2000 + 10 * value.data[0] + value.data[1];
  month = 10 * value.data[2] + value.data[3]; day = 10 * value.data[4] + value.data[5];
  if (!tc_pki_date(year, month, day)) return 0;
  out->year = year; out->month = (uint8_t)month; out->day = (uint8_t)day;
  return 1;
}

TC_TLV_result TC_EAC_CVC_extensions_init(TC_TLV_reader* reader, TC_bytes encoded,
    const TC_TLV_limits* limits)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (!encoded.data && !encoded.length) return TC_TLV_reader_init(reader, NULL, 0, TC_TLV_DER, limits);
  result = TC_TLV_read(encoded.data, encoded.length, TC_TLV_DER, limits, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x65) || element.encoded.length != encoded.length || !element.value.length)
    return TC_TLV_INVALID;
  return TC_TLV_reader_init(reader, element.value.data, element.value.length, TC_TLV_DER, limits);
}
TC_TLV_result TC_EAC_CVC_extension_next(TC_TLV_reader* reader, TC_EAC_CVC_extension* out)
{
  TC_TLV_reader next, inner;
  TC_TLV_element element;
  TC_EAC_CVC_extension extension;
  TC_TLV_result result;
  if (!reader || !out) return TC_TLV_ARGUMENT;
  next = *reader;
  result = TC_TLV_next(&next, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x73)) return TC_TLV_INVALID;
  result = TC_TLV_reader_init(&inner, element.value.data, element.value.length, TC_TLV_DER, &next.limits);
  if (result != TC_TLV_OK) return result;
  inner.elements = next.elements;
  result = tc_pki_next(&inner, 6, &element);
  if (result != TC_TLV_OK) return result == TC_TLV_LIMIT ? result : TC_TLV_INVALID;
  if (TC_DER_oid_contents(element.value.data, element.value.length) != TC_TLV_OK) return TC_TLV_INVALID;
  extension.oid = element.value;
  extension.fields.data = inner.input.data + inner.offset;
  extension.fields.length = inner.input.length - inner.offset;
  if (!extension.fields.length) return TC_TLV_INVALID;
  while ((result = TC_TLV_next(&inner, &element)) == TC_TLV_OK)
    if (element.header.tag_class != 2) return TC_TLV_INVALID;
  if (result != TC_TLV_END) return result == TC_TLV_LIMIT ? result : TC_TLV_INVALID;
  next.elements = inner.elements;
  *reader = next; *out = extension;
  return TC_TLV_OK;
}

TC_TLV_result TC_EAC_CVC_read(const uint8_t* data, size_t length,
    const TC_TLV_limits* limits, TC_EAC_CVC_workspace* workspace, TC_EAC_CVC* out)
{
  static const uint8_t roles[] = {4,0,0x7f,0,7,3,1,2};
  TC_EAC_CVC certificate;
  TC_TLV_element element;
  TC_TLV_reader reader, body, chat, extensions;
  TC_EAC_CVC_extension extension;
  TC_TLV_result result;
  if (!out || !workspace) return TC_TLV_ARGUMENT;
  result = TC_TLV_read(data, length, TC_TLV_DER, limits, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element, 0x7f21) || element.encoded.length != length) return TC_TLV_INVALID;
  result = TC_TLV_walk(data, length, TC_TLV_DER, limits, workspace->frames,
      workspace->frame_capacity, NULL, NULL);
  if (result != TC_TLV_OK) return result;
  memset(&certificate, 0, sizeof certificate); certificate.encoded = element.encoded;
  if (open(element.value, &reader) != TC_TLV_OK || !field(&reader, 0x7f4e, &element)) return TC_TLV_INVALID;
  certificate.signed_data = element.encoded;
  if (open(element.value, &body) != TC_TLV_OK || !field(&body, 0x5f29, &element) || element.value.length != 1)
    return TC_TLV_INVALID;
  if (element.value.data[0]) return TC_TLV_UNSUPPORTED;
  if (!field(&body, 0x42, &element) || !reference(element.value)) return TC_TLV_INVALID;
  certificate.issuer = element.value;
  if (!field(&body, 0x7f49, &element)) return TC_TLV_INVALID;
  result = key_contents(element.value, 0, &certificate.public_key);
  if (result != TC_TLV_OK) return result;
  if (!field(&body, 0x5f20, &element) || !reference(element.value)) return TC_TLV_INVALID;
  certificate.holder = element.value;
  if (!field(&body, 0x7f4c, &element) || open(element.value, &chat) != TC_TLV_OK || !field(&chat, 6, &element) ||
      TC_DER_oid_contents(element.value.data, element.value.length) != TC_TLV_OK) return TC_TLV_INVALID;
  certificate.authorization_oid = element.value;
  if (element.value.length != 9 || memcmp(element.value.data, roles, sizeof roles) ||
      element.value.data[8] < 1 || element.value.data[8] > 3) return TC_TLV_UNSUPPORTED;
  certificate.terminal_type = (TC_EAC_terminal_type)element.value.data[8];
  if (!field(&chat, 0x53, &element) || !tc_pki_end(&chat) ||
      element.value.length != (certificate.terminal_type == TC_EAC_AT ? 5u : 1u)) return TC_TLV_INVALID;
  certificate.authorization = element.value;
  certificate.role = (TC_EAC_role)(element.value.data[0] >> 6);
  if (certificate.public_key.algorithm == TC_EAC_ECDSA) {
    if (certificate.role != TC_EAC_CVCA && certificate.public_key.has_domain) return TC_TLV_INVALID;
    if (certificate.role == TC_EAC_CVCA && tc_pki_equal(certificate.issuer, certificate.holder) &&
        !certificate.public_key.has_domain) return TC_TLV_INVALID;
  }
  if (!field(&body, 0x5f25, &element) || !date(element.value, &certificate.effective) ||
      !field(&body, 0x5f24, &element) || !date(element.value, &certificate.expiration)) return TC_TLV_INVALID;
  if (certificate.effective.year > certificate.expiration.year ||
      (certificate.effective.year == certificate.expiration.year &&
       (certificate.effective.month > certificate.expiration.month ||
        (certificate.effective.month == certificate.expiration.month &&
         certificate.effective.day > certificate.expiration.day)))) return TC_TLV_INVALID;
  if (!tc_pki_end(&body)) {
    if (!field(&body, 0x65, &element) || !tc_pki_end(&body)) return TC_TLV_INVALID;
    certificate.extensions = element.encoded;
    if (TC_EAC_CVC_extensions_init(&extensions, element.encoded, limits) != TC_TLV_OK) return TC_TLV_INVALID;
    while ((result = TC_EAC_CVC_extension_next(&extensions, &extension)) == TC_TLV_OK) {}
    if (result != TC_TLV_END) return TC_TLV_INVALID;
  }
  if (!field(&reader, 0x5f37, &element) || !element.value.length || !tc_pki_end(&reader)) return TC_TLV_INVALID;
  certificate.signature = element.value;
  *out = certificate;
  return TC_TLV_OK;
}

TC_TLV_result TC_EAC_CVC_check_encoding(const TC_EAC_CVC* certificate,
    const TC_EAC_CVC_public_key* issuer, const TC_EAC_CVC_public_key* inherited)
{
  const TC_EAC_CVC_public_key* subject;
  const TC_EAC_CVC_public_key* domain;
  size_t width;
  if (!certificate || !issuer || !certificate->signature.data) return TC_TLV_ARGUMENT;
  subject = &certificate->public_key;
  if (subject->algorithm == TC_EAC_ECDSA) {
    domain = subject->has_domain ? subject : inherited;
    if (!domain || domain->algorithm != TC_EAC_ECDSA || !domain->has_domain || !domain->p.data || !domain->p.length)
      return TC_TLV_ARGUMENT;
    if (!point_width(subject->point, domain->p)) return TC_TLV_INVALID;
  }
  if (issuer->algorithm == TC_EAC_ECDSA) {
    if (!issuer->has_domain || !issuer->order.data || !issuer->order.length) return TC_TLV_ARGUMENT;
    width = issuer->order.length;
    if (width > SIZE_MAX / 2) return TC_TLV_LIMIT;
    if (certificate->signature.length != 2 * width) return TC_TLV_INVALID;
  } else if (issuer->algorithm == TC_EAC_RSA_V15 || issuer->algorithm == TC_EAC_RSA_PSS) {
    if (!issuer->modulus.data || !issuer->modulus.length) return TC_TLV_ARGUMENT;
    if (certificate->signature.length != issuer->modulus.length) return TC_TLV_INVALID;
  } else return TC_TLV_UNSUPPORTED;
  return TC_TLV_OK;
}
#endif
