/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_INTERNAL_H_
#define TC_PKI_INTERNAL_H_
#include <tiny_crypto/der.h>
#include <string.h>

static inline int tc_pki_tag(const TC_TLV_element* element, unsigned tag)
{
  return tag <= 255 ? element->header.tag_length == 1 && element->header.tag[0] == tag :
    element->header.tag_length == 2 && element->header.tag[0] == (tag >> 8) &&
    element->header.tag[1] == (tag & 255);
}

static inline TC_TLV_result tc_pki_next(TC_TLV_reader* reader, unsigned tag, TC_TLV_element* element)
{
  TC_TLV_result result = TC_TLV_next(reader, element);
  if (result != TC_TLV_OK) return result;
  return tc_pki_tag(element, tag) ? TC_TLV_OK : TC_TLV_INVALID;
}

static inline int tc_pki_end(const TC_TLV_reader* reader)
{ return reader->offset == reader->input.length; }

static inline int tc_pki_equal(TC_bytes a, TC_bytes b)
{ return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length)); }

/* NULL is primitive and empty in both BER and DER. Only length framing differs. */
static inline TC_TLV_result tc_pki_null(TC_bytes encoded, TC_TLV_profile profile)
{
  const TC_TLV_limits limits = {encoded.length,encoded.length,1,0};
  TC_TLV_element element;
  TC_TLV_result result;
  if (profile != TC_TLV_DER && profile != TC_TLV_BER) return TC_TLV_ARGUMENT;
  result = TC_TLV_read(encoded.data,encoded.length,profile,&limits,&element);
  if (result != TC_TLV_OK) return result;
  return tc_pki_tag(&element,5) && !element.value.length &&
      element.encoded.length == encoded.length ? TC_TLV_OK : TC_TLV_INVALID;
}

/* Lexicographic ordering for DER SET OF members, including their headers. */
static inline int tc_pki_compare(TC_bytes a, TC_bytes b)
{
  size_t length = a.length < b.length ? a.length : b.length;
  int order = length ? memcmp(a.data,b.data,length) : 0;
  return order ? order : a.length < b.length ? -1 : a.length > b.length;
}

TC_TLV_result tc_x509_pss_parameters(TC_bytes encoded);
typedef struct {
  TC_DER_algorithm hash, mgf_hash;
  /* Nonnegative INTEGER contents, retaining sign padding. Default is 20.
   * Native verification checks representability and key-specific salt limits. */
  TC_bytes salt_length;
} tc_pki_pss_parameters;
/* Decode explicit parameters, applying RFC 4055 defaults. Unknown digest OIDs
 * remain available for provider selection. out changes only on OK. */
TC_TLV_result tc_pki_pss_read(TC_bytes encoded, tc_pki_pss_parameters* out);
struct tc_pki_tree_workspace;
TC_TLV_result tc_pki_pss_read_profile(TC_bytes encoded, TC_TLV_profile profile,
    const TC_TLV_limits* limits, const struct tc_pki_tree_workspace* tree,
    tc_pki_pss_parameters* out);

typedef enum {
  TC_X509_ATTRIBUTE_UNKNOWN, TC_X509_ATTRIBUTE_DIRECTORY,
  TC_X509_ATTRIBUTE_PRINTABLE, TC_X509_ATTRIBUTE_COUNTRY,
  TC_X509_ATTRIBUTE_EMAIL, TC_X509_ATTRIBUTE_DOMAIN
} tc_x509_attribute_kind;
tc_x509_attribute_kind tc_x509_attribute_syntax(TC_bytes oid);
int tc_x509_attribute_value(TC_bytes oid, const TC_TLV_element* element);
int tc_x509_attribute_type(TC_bytes oid, unsigned tag, size_t length);
/* Consume nonempty DER RDN contents, including SET OF ordering. */
TC_TLV_result tc_x509_rdn_contents(TC_TLV_reader* attributes);

static inline int tc_pki_date(unsigned year, unsigned month, unsigned day)
{
  unsigned days;
  if (!year || month < 1 || month > 12 || !day) return 0;
  if (month == 2) days = 28 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
  else days = (month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31;
  return day <= days;
}

static inline TC_EC_curve tc_pki_curve(TC_bytes oid, unsigned* bits)
{
  static const uint8_t p256[] = {0x2a,0x86,0x48,0xce,0x3d,3,1,7};
  static const uint8_t sec[] = {0x2b,0x81,4,0};
  static const uint8_t brainpool[] = {0x2b,0x24,3,3,2,8,1,1};
  *bits = 0;
  if (oid.length == sizeof p256 && !memcmp(oid.data, p256, sizeof p256)) {
    *bits = 256; return TC_EC_P256;
  }
  if (oid.length == 5 && !memcmp(oid.data, sec, sizeof sec)) {
    switch (oid.data[4]) {
      case 0x22: *bits = 384; return TC_EC_P384;
      case 0x23: *bits = 521; return TC_EC_P521;
      case 0x21: *bits = 224; return TC_EC_P224;
      case 0x0a: *bits = 256; return TC_EC_SECP256K1;
      default: break;
    }
  }
  if (oid.length == 9 && !memcmp(oid.data, brainpool, sizeof brainpool)) {
    switch (oid.data[8]) {
      case 7: *bits = 256; return TC_EC_BRAINPOOL_P256;
      case 11: *bits = 384; return TC_EC_BRAINPOOL_P384;
      case 13: *bits = 512; return TC_EC_BRAINPOOL_P512;
      default: break;
    }
  }
  return TC_EC_UNKNOWN;
}
#endif
