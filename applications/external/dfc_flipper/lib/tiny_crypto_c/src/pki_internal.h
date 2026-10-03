/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_INTERNAL_H_
#define TC_PKI_INTERNAL_H_
#include <tiny_crypto/der.h>
#include <tiny_crypto/x509.h>
#include "internal.h"
#include <string.h>

/* Final arc of the RFC 5280 id-ce extensions (2.5.29.n), as returned by
 * tc_pki_extension_id. Encoded OIDs are 55 1D n. */
enum {
  TC_PKI_EXT_SUBJECT_KEY_IDENTIFIER = 14,
  TC_PKI_EXT_KEY_USAGE = 15,
  TC_PKI_EXT_SUBJECT_ALT_NAME = 17,
  TC_PKI_EXT_ISSUER_ALT_NAME = 18,
  TC_PKI_EXT_BASIC_CONSTRAINTS = 19,
  TC_PKI_EXT_CRL_NUMBER = 20,
  TC_PKI_EXT_REASON_CODE = 21,
  TC_PKI_EXT_INVALIDITY_DATE = 24,
  TC_PKI_EXT_DELTA_CRL_INDICATOR = 27,
  TC_PKI_EXT_ISSUING_DISTRIBUTION_POINT = 28,
  TC_PKI_EXT_CERTIFICATE_ISSUER = 29,
  TC_PKI_EXT_NAME_CONSTRAINTS = 30,
  TC_PKI_EXT_CRL_DISTRIBUTION_POINTS = 31,
  TC_PKI_EXT_CERTIFICATE_POLICIES = 32,
  TC_PKI_EXT_POLICY_MAPPINGS = 33,
  TC_PKI_EXT_AUTHORITY_KEY_IDENTIFIER = 35,
  TC_PKI_EXT_POLICY_CONSTRAINTS = 36,
  TC_PKI_EXT_EXTENDED_KEY_USAGE = 37,
  TC_PKI_EXT_FRESHEST_CRL = 46,
  TC_PKI_EXT_INHIBIT_ANY_POLICY = 54
};

static inline int tc_pki_tag(const TC_TLV_element* element, unsigned tag)
{
  return tag <= 255 ? element->header.tag_length == 1 && element->header.tag[0] == tag
                    : element->header.tag_length == 2 && element->header.tag[0] == (tag >> 8) &&
                          element->header.tag[1] == (tag & 255);
}

static inline TC_TLV_result tc_pki_next(TC_TLV_reader* reader, unsigned tag,
                                        TC_TLV_element* element)
{
  TC_TLV_result result = TC_TLV_next(reader, element);
  if (result != TC_TLV_OK)
    return result;
  return tc_pki_tag(element, tag) ? TC_TLV_OK : TC_TLV_INVALID;
}

/* Read a required child with the given tag. A missing (END) or truncated
 * (MORE) child inside a complete parent is INVALID. */
static inline TC_TLV_result tc_pki_field(TC_TLV_reader* reader, unsigned tag,
                                         TC_TLV_element* element)
{
  TC_TLV_result result = tc_pki_next(reader, tag, element);
  return result == TC_TLV_END || result == TC_TLV_MORE ? TC_TLV_INVALID : result;
}

/* Optional context-specific fields in a SEQUENCE use one of the allowed
 * single-byte tags, each at most once and in the listed order. *previous is
 * the position after the last accepted field (0 before the first); *index
 * receives the new field's position in allowed. Any other tag is INVALID. */
static inline TC_TLV_result tc_pki_context_order(const TC_TLV_element* element,
                                                 const uint8_t* allowed, size_t count,
                                                 size_t* previous, size_t* index)
{
  if (element->header.tag_length == 1)
    for (size_t i = *previous; i < count; ++i)
      if (element->header.tag[0] == allowed[i]) {
        *index = i;
        *previous = i + 1;
        return TC_TLV_OK;
      }
  return TC_TLV_INVALID;
}

static inline int tc_pki_end(const TC_TLV_reader* reader)
{
  return reader->offset == reader->input.length;
}

/* Open a DER reader over the contents of a constructed element that parent
 * has just read. The child continues the parent's element count one level
 * deeper, and treats a truncated nested element as INVALID. Hand the count
 * back with tc_pki_child_close once the element is accepted. A parent with no
 * depth left returns LIMIT. */
static inline TC_TLV_result tc_pki_child_open(TC_TLV_reader* child, const TC_TLV_reader* parent,
                                              TC_bytes contents)
{
  TC_TLV_limits budget = parent->limits;
  TC_TLV_reader opened;
  TC_TLV_result result;
  if (!budget.max_depth)
    return TC_TLV_LIMIT;
  --budget.max_depth;
  result = TC_TLV_reader_init(&opened, contents, TC_TLV_DER, &budget);
  if (result != TC_TLV_OK)
    return result;
  opened.root = 0;
  opened.elements = parent->elements;
  *child = opened;
  return TC_TLV_OK;
}

static inline void tc_pki_child_close(TC_TLV_reader* parent, const TC_TLV_reader* child)
{
  parent->elements = child->elements;
}

/* Open a DER reader over a complete constructed value with the given tag.
 * The outer element counts as one element and one level, so the reader's
 * limits cover the value as TC_TLV_walk counts it. Empty contents are
 * INVALID unless allow_empty is set. reader changes only on OK. */
static inline TC_TLV_result tc_pki_value_open(TC_TLV_reader* reader, TC_bytes value, unsigned tag,
                                              const TC_TLV_limits* limits, int allow_empty)
{
  TC_TLV_reader outer, contents;
  TC_TLV_element element;
  TC_TLV_result result;
  /* The returned reader is rewritten while it reads value. */
  if (!reader || !tc_internal_ranges_disjoint(reader, sizeof *reader, value.data, value.length))
    return TC_TLV_ARGUMENT;
  result = TC_TLV_reader_init(&outer, value, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_next(&outer, &element);
  if (result == TC_TLV_END || result == TC_TLV_MORE)
    return TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, tag) || !element.header.constructed || !tc_pki_end(&outer) ||
      (!allow_empty && !element.value.length))
    return TC_TLV_INVALID;
  /* The outer element stays charged to the returned reader. */
  result = tc_pki_child_open(&contents, &outer, element.value);
  if (result != TC_TLV_OK)
    return result;
  *reader = contents;
  return TC_TLV_OK;
}

static inline int tc_pki_equal(TC_bytes a, TC_bytes b)
{
  return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

/* NULL is primitive and empty in both BER and DER. Only length framing differs. */
static inline TC_TLV_result tc_pki_null(TC_bytes encoded, TC_TLV_profile profile)
{
  const TC_TLV_limits limits = {encoded.length, encoded.length, 1, 0};
  TC_TLV_element element;
  TC_TLV_result result;
  if (profile != TC_TLV_DER && profile != TC_TLV_BER)
    return TC_TLV_ARGUMENT;
  result = TC_TLV_read(encoded, profile, &limits, &element);
  if (result != TC_TLV_OK)
    return result;
  return tc_pki_tag(&element, 5) && !element.value.length &&
                 element.encoded.length == encoded.length
             ? TC_TLV_OK
             : TC_TLV_INVALID;
}

/* Lexicographic ordering for DER SET OF members, including their headers. */
static inline int tc_pki_compare(TC_bytes a, TC_bytes b)
{
  size_t length = a.length < b.length ? a.length : b.length;
  int order = length ? memcmp(a.data, b.data, length) : 0;
  return order ? order : a.length < b.length ? -1 : a.length > b.length;
}

TC_TLV_result tc_x509_pss_parameters(TC_bytes encoded);
/* Parse a DER Certificate whose outer element has the given tag: 0x30, or a
 * context tag for an IMPLICIT Certificate. Rules and output match
 * TC_X509_read. */
TC_TLV_result tc_x509_certificate_read(TC_bytes encoded, unsigned tag, const TC_TLV_limits* limits,
                                       const TC_X509_workspace* workspace,
                                       TC_X509_certificate* out);
/* TC_X509_certificate_names_check with the subjectAltName value supplied.
 * san.data is NULL when the certificate has no subjectAltName. */
TC_TLV_result tc_x509_certificate_names_check_san(const TC_X509_certificate* certificate,
                                                  TC_bytes san,
                                                  const TC_X509_name_constraints* constraints,
                                                  const TC_TLV_limits* limits,
                                                  const TC_X509_constraint_workspace* workspace,
                                                  size_t* work, int* permitted);
/* Read the next RFC 5280 PolicyInformation from reader and charge its
 * fields to the reader's budget. qualifiers keeps the complete SEQUENCE
 * encoding and is {NULL, 0} when absent. reader and out change only on OK. */
TC_TLV_result tc_x509_policy_information_next(TC_TLV_reader* reader, TC_X509_policy* out);
/* Parse a bare DER TBSCertificate with the TC_X509_read field rules. encoded,
 * signature and the outer signatureAlgorithm stay empty. The TBS signature
 * field is returned in signature_algorithm. */
TC_TLV_result tc_x509_tbs_read(TC_bytes encoded, const TC_TLV_limits* limits,
                               const TC_X509_workspace* workspace, TC_X509_certificate* out);
typedef struct {
  TC_DER_algorithm hash, mgf_hash;
  /* Nonnegative INTEGER contents, retaining sign padding. Default is 20.
   * Native verification checks representability and key-specific salt limits. */
  TC_bytes salt_length;
} tc_pki_pss_parameters;
/* Decode explicit parameters, applying RFC 4055 defaults. Unknown digest OIDs
 * remain available for provider selection. out changes only on OK. */
TC_TLV_result tc_pki_pss_read(TC_bytes encoded, tc_pki_pss_parameters* out);
/* Scratch for decoding one encoded object: nesting frames and the shared
 * work budget. Frames, work and the input are disjoint. */
typedef struct tc_pki_tree_workspace {
  TC_TLV_frame* frames;
  size_t capacity;
  size_t* work;
} tc_pki_tree_workspace;
TC_TLV_result tc_pki_pss_read_profile(TC_bytes encoded, TC_TLV_profile profile,
                                      const TC_TLV_limits* limits,
                                      const tc_pki_tree_workspace* tree,
                                      tc_pki_pss_parameters* out);

typedef enum {
  TC_X509_ATTRIBUTE_UNKNOWN,
  TC_X509_ATTRIBUTE_DIRECTORY,
  TC_X509_ATTRIBUTE_PRINTABLE,
  TC_X509_ATTRIBUTE_COUNTRY,
  TC_X509_ATTRIBUTE_EMAIL,
  TC_X509_ATTRIBUTE_DOMAIN
} tc_x509_attribute_kind;
tc_x509_attribute_kind tc_x509_attribute_syntax(TC_bytes oid);
int tc_x509_attribute_value(TC_bytes oid, const TC_TLV_element* element);
int tc_x509_attribute_type(TC_bytes oid, unsigned tag, size_t length);
/* Consume nonempty DER RDN contents, including SET OF ordering. */
TC_TLV_result tc_x509_rdn_contents(TC_TLV_reader* attributes);

/* Named curves from RFC 5480 section 2.1.1.1, plus the Brainpool curves.
 * X9.62 prime curves are 1.2.840.10045.3.1.{1 secp192r1, 7 secp256r1}. */
static inline TC_EC_curve tc_pki_curve(TC_bytes oid, unsigned* bits)
{
  static const uint8_t prime[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 3, 1};
  static const uint8_t sec[] = {0x2b, 0x81, 4, 0};
  static const uint8_t brainpool[] = {0x2b, 0x24, 3, 3, 2, 8, 1, 1};
  *bits = 0;
  if (oid.length == sizeof prime + 1 && !memcmp(oid.data, prime, sizeof prime)) {
    switch (oid.data[sizeof prime]) {
    case 1:
      *bits = 192;
      return TC_EC_P192;
    case 7:
      *bits = 256;
      return TC_EC_P256;
    default:
      break;
    }
  }
  if (oid.length == 5 && !memcmp(oid.data, sec, sizeof sec)) {
    switch (oid.data[4]) {
    case 0x22:
      *bits = 384;
      return TC_EC_P384;
    case 0x23:
      *bits = 521;
      return TC_EC_P521;
    case 0x21:
      *bits = 224;
      return TC_EC_P224;
    case 0x0a:
      *bits = 256;
      return TC_EC_SECP256K1;
    default:
      break;
    }
  }
  if (oid.length == 9 && !memcmp(oid.data, brainpool, sizeof brainpool)) {
    switch (oid.data[8]) {
    case 7:
      *bits = 256;
      return TC_EC_BRAINPOOL_P256;
    case 11:
      *bits = 384;
      return TC_EC_BRAINPOOL_P384;
    case 13:
      *bits = 512;
      return TC_EC_BRAINPOOL_P512;
    default:
      break;
    }
  }
  return TC_EC_UNKNOWN;
}
#endif
