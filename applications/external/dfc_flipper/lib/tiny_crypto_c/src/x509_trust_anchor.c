/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_TRUST_ANCHOR_FORMAT
#include <tiny_crypto/x509_trust_anchor.h>
#include <tiny_crypto/x509_path.h>
#include "pki_internal.h"
#include "pki_storage_internal.h"
#include "string_internal.h"
#include "pki_bits_internal.h"
#include "x509_store_anchor_internal.h"

/* Helpers for the TrustAnchorInfo choice. */
#if TC_TAF_ENABLE_TRUST_ANCHOR_INFO
static TC_TLV_result contents_reader(TC_TLV_reader* reader, TC_bytes bytes,
                                     const TC_TLV_limits* limits)
{
  return TC_TLV_reader_init(reader, bytes, TC_TLV_DER, limits);
}

static TC_TLV_result utf8_string(TC_bytes value, size_t max_characters)
{
  size_t offset = 0, count = 0;
  uint32_t point;
  TC_TLV_result result;
  while ((result = tc_asn1_string_next(0x0c, value, &offset, &point)) == TC_TLV_OK) {
    if (++count > max_characters)
      return TC_TLV_LIMIT;
  }
  return result == TC_TLV_END && count ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result encoded_name(TC_bytes name, const TC_TLV_limits* limits)
{
  TC_TLV_reader reader;
  TC_bytes rdn;
  TC_TLV_result result = TC_X509_name_init(&reader, name, limits);
  if (result != TC_TLV_OK)
    return result;
  result = TC_X509_rdn_next(&reader, &rdn);
  if (result != TC_TLV_OK)
    return result == TC_TLV_END ? TC_TLV_INVALID : result;
  while ((result = TC_X509_rdn_next(&reader, &rdn)) == TC_TLV_OK) {
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

/* RFC 5914 section 2.5 CertPolicyFlags: inhibitPolicyMapping (0),
 * requireExplicitPolicy (1), inhibitAnyPolicy (2). DER named bits drop
 * trailing zero bits and clear the unused padding bits. */
static TC_TLV_result policy_flags(TC_bytes contents, unsigned* flags)
{
  enum { POLICY_FLAG_BITS = 3 };
  TC_bytes bits;
  unsigned unused;
  uint16_t named = 0;
  TC_TLV_result result = tc_der_bit_string_contents(contents, &bits, &unused);
  if (result == TC_TLV_OK)
    result = tc_pki_named_bits(bits, unused, POLICY_FLAG_BITS, &named);
  if (result != TC_TLV_OK)
    return result;
  *flags = ((named & 1u) ? TC_X509_PATH_INHIBIT_MAPPING : 0u) |
           ((named & 2u) ? TC_X509_PATH_REQUIRE_EXPLICIT_POLICY : 0u) |
           ((named & 4u) ? TC_X509_PATH_INHIBIT_ANY_POLICY : 0u);
  return TC_TLV_OK;
}

static TC_TLV_result name_constraints(TC_bytes contents, const TC_TLV_limits* limits,
                                      const TC_X509_workspace* workspace,
                                      TC_X509_name_constraints* out)
{
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_X509_name_constraints names = {{NULL, 0}, {NULL, 0}};
  static const uint8_t subtree_tags[] = {0xa0, 0xa1}; /* permitted, excluded */
  size_t previous = 0, index;
  TC_TLV_result result = contents_reader(&reader, contents, limits);
  if (result != TC_TLV_OK)
    return result;
  if (tc_pki_end(&reader))
    return TC_TLV_INVALID;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    if (tc_pki_context_order(&element, subtree_tags, sizeof subtree_tags, &previous, &index) !=
            TC_TLV_OK ||
        !element.value.length)
      return TC_TLV_INVALID;
    if (index == 0)
      names.permitted = element.value;
    else
      names.excluded = element.value;
  }
  if (result != TC_TLV_END)
    return result;
  result = tc_x509_anchor_subtrees(&names, limits, workspace);
  if (result != TC_TLV_OK)
    return result;
  *out = names;
  return TC_TLV_OK;
}

static TC_TLV_result cert_path_controls(TC_bytes contents, const TC_TLV_limits* limits,
                                        const TC_X509_workspace* workspace,
                                        TC_X509_store_anchor* out, TC_bytes pubkey, TC_bytes key_id)
{
  TC_TLV_reader reader;
  TC_TLV_element element;
  /* RFC 5914 CertPathControls after taName: certificate [0], policySet [1],
   * policyFlags [2], nameConstr [3], pathLenConstraint [4]. */
  static const uint8_t control_tags[] = {0xa0, 0xa1, 0x82, 0xa3, 0x84};
  size_t previous = 0, index;
  TC_TLV_result result = contents_reader(&reader, contents, limits);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_field(&reader, 0x30, &element);
  if (result != TC_TLV_OK || !element.value.length ||
      encoded_name(element.encoded, limits) != TC_TLV_OK)
    return TC_TLV_INVALID;
  out->trust.name = element.encoded;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    if (tc_pki_context_order(&element, control_tags, sizeof control_tags, &previous, &index) !=
        TC_TLV_OK)
      return TC_TLV_INVALID;
    switch (index) {
    case 0: {
      /* certificate [0] IMPLICIT Certificate must match taName, pubKey and keyId. */
      TC_X509_store_anchor embedded = {0};
      TC_X509_certificate certificate;
      result = tc_x509_certificate_read(element.encoded, 0xa0, limits, workspace, &certificate);
      if (result == TC_TLV_OK)
        result = tc_x509_anchor_certificate(&certificate, limits, workspace, &embedded);
      if (result != TC_TLV_OK)
        return result;
      if (!tc_pki_equal(embedded.trust.name, out->trust.name) ||
          !tc_pki_equal(certificate.spki, pubkey) ||
          (embedded.key_id.data && !tc_pki_equal(embedded.key_id, key_id)))
        return TC_TLV_INVALID;
      out->policy_set = embedded.policy_set;
      out->policy_flags = embedded.policy_flags;
      out->names = embedded.names;
      out->path_len = embedded.path_len;
      out->has_path_len = embedded.has_path_len;
      out->usage = embedded.usage;
      out->certificate_extensions = embedded.certificate_extensions;
      break;
    }
    case 1:
      result = tc_x509_anchor_policy_set(element.value, limits, workspace, 0);
      if (result != TC_TLV_OK)
        return result;
      out->policy_set = element.value;
      out->replaced_controls |= TC_X509_ANCHOR_REPLACED_POLICY_SET;
      break;
    case 2:
      result = policy_flags(element.value, &out->policy_flags);
      if (result != TC_TLV_OK)
        return result;
      if (!out->policy_set.data && (out->policy_flags & TC_X509_PATH_REQUIRE_EXPLICIT_POLICY))
        return TC_TLV_INVALID;
      out->replaced_controls |= TC_X509_ANCHOR_REPLACED_POLICY_FLAGS;
      break;
    case 3:
      result = name_constraints(element.value, limits, workspace, &out->names);
      if (result != TC_TLV_OK)
        return result;
      out->replaced_controls |= TC_X509_ANCHOR_REPLACED_NAMES;
      break;
    default: {
      uint32_t length;
      result = TC_DER_uint32_contents(element.value, &length);
      if (result != TC_TLV_OK)
        return result;
      out->path_len = length;
      out->has_path_len = 1;
      out->replaced_controls |= TC_X509_ANCHOR_REPLACED_PATH_LEN;
      break;
    }
    }
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

static TC_TLV_result trust_anchor_info(TC_bytes contents, const TC_TLV_limits* limits,
                                       const TC_X509_workspace* workspace,
                                       TC_X509_store_anchor* out)
{
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result = contents_reader(&reader, contents, limits);
  TC_bytes spki;
  /* RFC 5914 TrustAnchorInfo after keyId: taTitle, certPath, exts [1],
   * taTitleLangTag [2]. */
  static const uint8_t optional_tags[] = {0x0c, 0x30, 0xa1, 0x82};
  size_t previous = 0, index;
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_next(&reader, &element);
  if (result != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (tc_pki_tag(&element, 2)) {
    uint32_t version;
    result = TC_DER_uint32(element.encoded, &version);
    if (result != TC_TLV_OK)
      return result;
    if (version == 1)
      return TC_TLV_INVALID; /* DEFAULT v1 is omitted in DER. */
    return TC_TLV_UNSUPPORTED;
  }
  if (!tc_pki_tag(&element, 0x30))
    return TC_TLV_INVALID;
  spki = element.encoded;
  result = TC_X509_subject_public_key(spki, &out->trust.public_key);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_field(&reader, 4, &element);
  if (result != TC_TLV_OK)
    return result;
  out->key_id = element.value;
  out->x509_unusable = 1;
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    if (tc_pki_context_order(&element, optional_tags, sizeof optional_tags, &previous, &index) !=
        TC_TLV_OK)
      return TC_TLV_INVALID;
    if (index == 0) {
      result = utf8_string(element.value, 64);
      if (result != TC_TLV_OK)
        return result;
      out->title = element.value;
    } else if (index == 1) {
      result = cert_path_controls(element.value, limits, workspace, out, spki, out->key_id);
      if (result != TC_TLV_OK)
        return result;
      out->x509_unusable = 0;
    } else if (index == 2) {
      TC_TLV_element inner;
      result = TC_TLV_read(element.value, TC_TLV_DER, limits, &inner);
      if (result != TC_TLV_OK || !tc_pki_tag(&inner, 0x30) ||
          inner.encoded.length != element.value.length)
        return TC_TLV_INVALID;
      out->extensions = inner.value;
      /* Path-control extensions are rejected here, so only a basicConstraints
       * pathLen remains. CertPathControls values are always enforced
       * (RFC 5914 section 2.5), so exts can only lower the limit. */
      TC_X509_store_anchor overrides = {0};
      result = tc_x509_anchor_extensions(inner.encoded, limits, workspace, 1, &overrides);
      if (result != TC_TLV_OK)
        return result;
      if (overrides.has_path_len && (!out->has_path_len || overrides.path_len < out->path_len)) {
        out->path_len = overrides.path_len;
        out->has_path_len = 1;
      }
    } else {
      result = utf8_string(element.value, SIZE_MAX);
      if (result != TC_TLV_OK)
        return result;
      out->title_language = element.value;
    }
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}
#endif

/* The list reader serves every enabled choice. */
TC_TLV_result TC_X509_trust_anchor_list_init(TC_X509_trust_anchor_reader* reader, TC_bytes encoded,
                                             const TC_TLV_limits* limits,
                                             const TC_X509_workspace* workspace)
{
  TC_X509_trust_anchor_reader parsed;
  TC_TLV_result result;
  TC_bytes writes[3];
  tc_pki_storage_plan plan;
  if (!reader || !limits || !workspace)
    return TC_TLV_ARGUMENT;
  /* init walks the list into the frames, and next rewrites the reader and
   * fills both workspace arrays while it reads the list. */
  tc_pki_storage_plan_begin(&plan, writes, sizeof writes / sizeof *writes, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, reader, 1);
  TC_PKI_PLAN_WRITE(&plan, workspace->frames.data, workspace->frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, workspace->extension_oids, workspace->extension_capacity);
  tc_pki_storage_plan_seal(&plan);
  tc_pki_storage_plan_input_span(&plan, encoded);
  TC_PKI_PLAN_INPUT(&plan, workspace, 1);
  if (tc_pki_storage_plan_finish(&plan, NULL) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  /* RFC 5914 section 4: TrustAnchorList ::= SEQUENCE SIZE (1..MAX). */
  result = tc_pki_value_open(&parsed.reader, encoded, 0x30, limits, 0);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_walk(encoded, TC_TLV_DER, limits, workspace->frames, NULL, NULL);
  if (result != TC_TLV_OK)
    return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  /* The walk bounded the whole list. Each anchor is decoded under limits. */
  parsed.reader.limits = *limits;
  parsed.workspace = workspace;
  *reader = parsed;
  return TC_TLV_OK;
}

/* next advances the reader and fills out and both workspace arrays while it
 * reads the list and the workspace struct. out joins the disjoint set that
 * init checked for the reader and the arrays. */
static TC_TLV_result next_storage(const TC_X509_trust_anchor_reader* reader,
                                  const TC_X509_store_anchor* out)
{
  const TC_X509_workspace* workspace = reader->workspace;
  TC_bytes writes[4];
  tc_pki_storage_plan plan;
  if (!workspace)
    return TC_TLV_ARGUMENT;
  tc_pki_storage_plan_begin(&plan, writes, sizeof writes / sizeof *writes, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, reader, 1);
  TC_PKI_PLAN_WRITE(&plan, workspace->frames.data, workspace->frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, workspace->extension_oids, workspace->extension_capacity);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  tc_pki_storage_plan_seal(&plan);
  tc_pki_storage_plan_input_span(&plan, reader->reader.input);
  TC_PKI_PLAN_INPUT(&plan, workspace, 1);
  return tc_pki_storage_plan_finish(&plan, NULL) == TC_TLV_OK ? TC_TLV_OK : TC_TLV_ARGUMENT;
}

TC_TLV_result TC_X509_trust_anchor_next(TC_X509_trust_anchor_reader* reader,
                                        TC_X509_store_anchor* out)
{
  TC_TLV_reader next;
  TC_TLV_element choice;
  TC_X509_store_anchor parsed = {0};
  const TC_TLV_limits* limits;
  const TC_X509_workspace* workspace;
  TC_TLV_result result;
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  result = next_storage(reader, out);
  if (result != TC_TLV_OK)
    return result;
  limits = &reader->reader.limits;
  workspace = reader->workspace;
  next = reader->reader;
  result = TC_TLV_next(&next, &choice);
  if (result != TC_TLV_OK)
    return result;
  if (tc_pki_tag(&choice, 0x30)) {
#if TC_TAF_ENABLE_CERTIFICATE
    TC_X509_certificate certificate;
    result = tc_x509_certificate_read(choice.encoded, 0x30, limits, workspace, &certificate);
    if (result == TC_TLV_OK)
      result = tc_x509_anchor_certificate(&certificate, limits, workspace, &parsed);
    if (result != TC_TLV_OK)
      return result;
#else
    return TC_TLV_UNSUPPORTED;
#endif
  } else if (tc_pki_tag(&choice, 0xa1)) {
#if TC_TAF_ENABLE_TBS_CERTIFICATE
    /* tbsCert [1] EXPLICIT TBSCertificate. */
    TC_X509_certificate certificate;
    result = tc_x509_tbs_read(choice.value, limits, workspace, &certificate);
    if (result == TC_TLV_OK)
      result = tc_x509_anchor_certificate(&certificate, limits, workspace, &parsed);
    if (result != TC_TLV_OK)
      return result;
#else
    return TC_TLV_UNSUPPORTED;
#endif
  } else if (tc_pki_tag(&choice, 0xa2)) {
#if TC_TAF_ENABLE_TRUST_ANCHOR_INFO
    TC_TLV_element info;
    result = TC_TLV_read(choice.value, TC_TLV_DER, limits, &info);
    if (result != TC_TLV_OK || !tc_pki_tag(&info, 0x30) ||
        info.encoded.length != choice.value.length)
      return TC_TLV_INVALID;
    result = trust_anchor_info(info.value, limits, workspace, &parsed);
    if (result != TC_TLV_OK)
      return result;
#else
    return TC_TLV_UNSUPPORTED;
#endif
  } else
    return TC_TLV_INVALID;
  reader->reader = next;
  *out = parsed;
  return TC_TLV_OK;
}
#endif
