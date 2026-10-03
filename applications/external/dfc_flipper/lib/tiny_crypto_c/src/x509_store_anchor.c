/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509_path.h>
#if TC_ENABLE_X509_PATH
#include "x509_store_anchor_internal.h"
#include "pki_internal.h"
#include "pki_extensions_internal.h"
#include "pki_storage_internal.h"
#include "x509_time_internal.h"
#include <string.h>

TC_TLV_result tc_x509_anchor_policy_set(TC_bytes contents, const TC_TLV_limits* limits,
                                        const TC_X509_workspace* workspace, int qualifiers_allowed)
{
  TC_TLV_reader policies;
  TC_X509_policy policy;
  size_t count = 0;
  TC_TLV_result result = TC_TLV_reader_init(&policies, contents, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  if (!contents.length)
    return TC_TLV_INVALID;
  while ((result = tc_x509_policy_information_next(&policies, &policy)) == TC_TLV_OK) {
    if (count == workspace->extension_capacity)
      return TC_TLV_LIMIT;
    if (policy.qualifiers.data && !qualifiers_allowed)
      return TC_TLV_INVALID;
    workspace->extension_oids[count++] = policy.oid;
  }
  if (result != TC_TLV_END)
    return result;
  return tc_pki_spans_unique(workspace->extension_oids, count, NULL);
}

TC_TLV_result tc_x509_anchor_subtrees(const TC_X509_name_constraints* names,
                                      const TC_TLV_limits* limits,
                                      const TC_X509_workspace* workspace)
{
  const TC_bytes lists[] = {names->permitted, names->excluded};
  for (size_t i = 0; i < 2; ++i) {
    TC_X509_general_subtrees_reader reader;
    TC_X509_general_subtree subtree;
    TC_TLV_result result;
    if (!lists[i].data)
      continue;
    result = TC_X509_general_subtrees_init(&reader, lists[i], limits, workspace->frames);
    if (result != TC_TLV_OK)
      return result;
    while ((result = TC_X509_general_subtree_next(&reader, &subtree)) == TC_TLV_OK) {
    }
    if (result != TC_TLV_END)
      return result;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_anchor_extensions(TC_bytes encoded, const TC_TLV_limits* limits,
                                        const TC_X509_workspace* workspace, int trust_anchor_info,
                                        TC_X509_store_anchor* out)
{
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result result;
  size_t count = 0;
  result = TC_X509_extensions_init(&reader, encoded, limits);
  if (result != TC_TLV_OK)
    return result;
  while ((result = TC_X509_extension_next(&reader, &extension)) == TC_TLV_OK) {
    if (count == workspace->extension_capacity)
      return TC_TLV_LIMIT;
    workspace->extension_oids[count++] = extension.oid;
    const unsigned id = tc_pki_extension_id(&extension);
    if (!id)
      continue;
    /* RFC 5914 section 2.6: these duplicate CertPathControls and must not
     * appear in TrustAnchorInfo exts. Reject them so a constraint is never
     * silently dropped. */
    if (trust_anchor_info && tc_pki_extension_path_control(id))
      return TC_TLV_INVALID;
    switch (id) {
    case TC_PKI_EXT_CERTIFICATE_POLICIES: {
      TC_TLV_element value;
      result = TC_TLV_read(extension.value, TC_TLV_DER, limits, &value);
      if (result != TC_TLV_OK || !tc_pki_tag(&value, 0x30) ||
          value.encoded.length != extension.value.length)
        return TC_TLV_INVALID;
      /* Checked after the loop, which owns the OID scratch until then. */
      out->policy_set = value.value;
      break;
    }
    case TC_PKI_EXT_NAME_CONSTRAINTS:
      result = TC_X509_name_constraints_read(extension.value, limits, &out->names);
      if (result != TC_TLV_OK)
        return result;
      result = tc_x509_anchor_subtrees(&out->names, limits, workspace);
      if (result != TC_TLV_OK)
        return result;
      break;
    /* RFC 5937 section 2: the presence of these fields sets the Boolean
     * path inputs. Their SkipCerts counts do not apply to the anchor. */
    case TC_PKI_EXT_POLICY_CONSTRAINTS: {
      TC_X509_policy_constraints constraints;
      result = TC_X509_policy_constraints_read(extension.value, limits, &constraints);
      if (result != TC_TLV_OK)
        return result;
      if (constraints.has_require_explicit_policy)
        out->policy_flags |= TC_X509_PATH_REQUIRE_EXPLICIT_POLICY;
      if (constraints.has_inhibit_policy_mapping)
        out->policy_flags |= TC_X509_PATH_INHIBIT_MAPPING;
      break;
    }
    case TC_PKI_EXT_INHIBIT_ANY_POLICY: {
      uint32_t skip;
      result = TC_DER_uint32(extension.value, &skip);
      if (result != TC_TLV_OK)
        return result;
      out->policy_flags |= TC_X509_PATH_INHIBIT_ANY_POLICY;
      break;
    }
    case TC_PKI_EXT_BASIC_CONSTRAINTS: {
      TC_X509_basic_constraints basic;
      result = TC_X509_basic_constraints_read(extension.value, limits, &basic);
      if (result != TC_TLV_OK)
        return result;
      if (basic.has_path_length) {
        out->has_path_len = 1;
        out->path_len = basic.path_length;
      }
      break;
    }
    case TC_PKI_EXT_SUBJECT_KEY_IDENTIFIER:
      result = TC_X509_subject_key_identifier_read(extension.value, limits, &out->key_id);
      if (result != TC_TLV_OK)
        return result;
      break;
    case TC_PKI_EXT_KEY_USAGE: {
      uint16_t usage;
      result = TC_X509_key_usage_read(extension.value, limits, &usage);
      if (result != TC_TLV_OK)
        return result;
      if (!(usage & TC_KEY_USAGE_CERT_SIGN))
        return TC_TLV_INVALID;
      if (usage & TC_KEY_USAGE_CRL_SIGN)
        out->usage |= TC_X509_ANCHOR_USAGE_CRL_SIGN;
      break;
    }
    default:
      break;
    }
  }
  if (result != TC_TLV_END)
    return result;
  result = tc_pki_spans_unique(workspace->extension_oids, count, NULL);
  if (result != TC_TLV_OK || !out->policy_set.data)
    return result;
  return tc_x509_anchor_policy_set(out->policy_set, limits, workspace, 1);
}

/* An anchor issues certificates, so its subject is non-empty (RFC 5280
 * section 4.1.2.6). A reversed validity period is malformed. */
TC_TLV_result tc_x509_anchor_certificate(const TC_X509_certificate* certificate,
                                         const TC_TLV_limits* limits,
                                         const TC_X509_workspace* workspace,
                                         TC_X509_store_anchor* out)
{
  TC_bytes contents;
  int order;
  if (certificate->subject.length == 2 ||
      TC_X509_time_compare(&certificate->not_before, &certificate->not_after, &order) !=
          TC_TLV_OK ||
      order > 0)
    return TC_TLV_INVALID;
  out->trust.name = certificate->subject;
  out->trust.public_key = certificate->public_key;
  if (certificate->version < 3)
    out->usage |= TC_X509_ANCHOR_USAGE_CRL_SIGN;
  if (!certificate->extensions.data)
    return TC_TLV_OK;
  if (TC_DER_sequence(certificate->extensions, &contents) != TC_TLV_OK)
    return TC_TLV_INVALID;
  out->certificate_extensions = contents;
  return tc_x509_anchor_extensions(certificate->extensions, limits, workspace, 0, out);
}

/* The record and the parser scratch are written. The certificate view and
 * the DER it borrows are read while both are written. */
static TC_TLV_result anchor_storage(const TC_X509_certificate* certificate,
                                    const TC_X509_workspace* workspace,
                                    const TC_X509_store_anchor* out)
{
  TC_bytes writes[3];
  const TC_bytes inputs[] = {certificate->encoded, certificate->tbs, certificate->subject,
                             certificate->spki, certificate->extensions};
  tc_pki_storage_plan plan;
  tc_pki_storage_plan_begin(&plan, writes, sizeof writes / sizeof *writes, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, workspace->frames.data, workspace->frames.capacity);
  TC_PKI_PLAN_WRITE(&plan, workspace->extension_oids, workspace->extension_capacity);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, certificate, 1);
  TC_PKI_PLAN_INPUT(&plan, workspace, 1);
  tc_pki_storage_plan_input_spans(&plan, inputs, sizeof inputs / sizeof *inputs);
  return tc_pki_storage_plan_finish(&plan, NULL);
}

TC_TLV_result TC_X509_store_anchor_from_certificate(const TC_X509_certificate* certificate,
                                                    const TC_TLV_limits* limits,
                                                    const TC_X509_workspace* workspace,
                                                    TC_X509_store_anchor* out)
{
  TC_X509_store_anchor anchor = {0};
  TC_TLV_result result;
  if (!certificate || !limits || !workspace || !out ||
      (!workspace->frames.data && workspace->frames.capacity) ||
      (!workspace->extension_oids && workspace->extension_capacity))
    return TC_TLV_ARGUMENT;
  result = anchor_storage(certificate, workspace, out);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_anchor_certificate(certificate, limits, workspace, &anchor);
  if (result != TC_TLV_OK) {
    memset(out, 0, sizeof *out);
    return result;
  }
  *out = anchor;
  return TC_TLV_OK;
}
#endif
