/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * CRL and CRL-entry extensions: decoding, critical-extension policy,
 * issuing distribution point scope and freshness (RFC 5280 sections 5.2, 5.3). */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_crl_internal.h"
#include "x509_time_internal.h"
#include "pki_extensions_internal.h"

TC_TLV_result tc_x509_crl_entry_policy(const TC_X509_crl_extensions* crl,
                                       const tc_x509_crl_entry_info* entry)
{
  const unsigned known = TC_CRL_ENTRY_REASON | TC_CRL_ENTRY_INVALIDITY | TC_CRL_ENTRY_ISSUER;
  if (!crl || !entry || (entry->present & ~known) || (entry->critical & ~entry->present))
    return TC_TLV_ARGUMENT;
  if (entry->unknown_critical_oid.length)
    return TC_TLV_UNSUPPORTED;
  if (entry->critical & (TC_CRL_ENTRY_REASON | TC_CRL_ENTRY_INVALIDITY))
    return TC_TLV_INVALID;
  if ((entry->present & TC_CRL_ENTRY_ISSUER) &&
      (!(entry->critical & TC_CRL_ENTRY_ISSUER) || !(crl->present & TC_X509_CRL_EXT_DISTRIBUTION) ||
       !crl->distribution.indirect))
    return TC_TLV_INVALID;
  if ((entry->present & TC_CRL_ENTRY_REASON) && entry->reason == TC_PKI_CRL_REASON_REMOVE &&
      !(crl->present & TC_X509_CRL_EXT_DELTA))
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_extension_policy(const TC_X509_crl_extensions* info)
{
  const unsigned known = TC_X509_CRL_EXT_NUMBER | TC_X509_CRL_EXT_DELTA |
                         TC_X509_CRL_EXT_AUTHORITY | TC_X509_CRL_EXT_DISTRIBUTION |
                         TC_X509_CRL_EXT_FRESHEST | TC_X509_CRL_EXT_ISSUER_ALT;
  const unsigned must_be_critical = TC_X509_CRL_EXT_DELTA | TC_X509_CRL_EXT_DISTRIBUTION;
  const unsigned must_be_noncritical = TC_X509_CRL_EXT_NUMBER | TC_X509_CRL_EXT_FRESHEST;
  if (!info || (info->present & ~known) || (info->critical & ~info->present))
    return TC_TLV_ARGUMENT;
  if (info->unknown_critical_oid.length)
    return TC_TLV_UNSUPPORTED;
  if ((info->critical & must_be_noncritical) ||
      ((info->present & must_be_critical) != (info->critical & must_be_critical)))
    return TC_TLV_INVALID;
  if ((info->present & (TC_X509_CRL_EXT_DELTA | TC_X509_CRL_EXT_FRESHEST)) ==
      (TC_X509_CRL_EXT_DELTA | TC_X509_CRL_EXT_FRESHEST))
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_fresh_at(const TC_X509_crl* crl, const TC_X509_revocation_time* time,
                                   tc_x509_freshness* out)
{
  if (!crl || !time || !out || (crl->has_next_update != 0 && crl->has_next_update != 1))
    return TC_TLV_ARGUMENT;
  return tc_x509_freshness_at(time, &crl->this_update,
                              crl->has_next_update ? &crl->next_update : NULL, out);
}

TC_TLV_result tc_x509_crl_coverage_at(const TC_X509_crl* crl,
                                      const TC_X509_crl_extensions* extensions,
                                      const TC_X509_revocation_time* time,
                                      const tc_pki_distribution_point* point,
                                      TC_bytes certificate_issuer, int certificate_ca,
                                      const tc_x509_crl_decode* decode, tc_x509_crl_coverage* out)
{
  if (!decode)
    return TC_TLV_ARGUMENT;
  const TC_TLV_limits* limits = decode->limits;
  const tc_pki_tree_workspace* tree = decode->tree;
  const TC_X509_name_workspace* names = decode->names;
  tc_x509_crl_coverage coverage = {0};
  TC_TLV_result result;
  if (!crl || !extensions || !time || !point || !limits || !tree || !tree->work || !names || !out ||
      (certificate_ca != 0 && certificate_ca != 1))
    return TC_TLV_ARGUMENT;
  if (tc_pki_work_charge(tree->work, 1) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = tc_x509_crl_extension_policy(extensions);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_fresh_at(crl, time, &coverage.freshness);
  if (result != TC_TLV_OK)
    return result;
  if (coverage.freshness == TC_X509_FRESH_CURRENT) {
    const TC_X509_crl_distribution* idp =
        extensions->present & TC_X509_CRL_EXT_DISTRIBUTION ? &extensions->distribution : NULL;
    result = tc_x509_crl_scope_reasons(crl, idp, point, certificate_issuer, certificate_ca, decode,
                                       &coverage.reasons);
    if (result != TC_TLV_OK)
      return result;
  }
  *out = coverage;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_scope_reasons(const TC_X509_crl* crl, const TC_X509_crl_distribution* idp,
                                        const tc_pki_distribution_point* point,
                                        TC_bytes certificate_issuer, int certificate_ca,
                                        const tc_x509_crl_decode* decode, uint16_t* out)
{
  if (!decode)
    return TC_TLV_ARGUMENT;
  const TC_TLV_limits* limits = decode->limits;
  const tc_pki_tree_workspace* tree = decode->tree;
  const TC_X509_name_workspace* names = decode->names;
  TC_TLV_result result;
  int matched;
  uint16_t reasons = TC_X509_CRL_ALL_REASONS;
  if (!out || !crl || !point || !tree || !tree->work ||
      (certificate_ca != 0 && certificate_ca != 1))
    return TC_TLV_ARGUMENT;
  if (idp && (idp->attribute_only || (idp->user_only && certificate_ca) ||
              (idp->ca_only && !certificate_ca))) {
    *out = 0;
    return TC_TLV_OK;
  }
  result = tc_x509_crl_issuer_matches(crl, idp, point, certificate_issuer, limits, tree, names,
                                      &matched);
  if (result != TC_TLV_OK)
    return result;
  if (!matched) {
    *out = 0;
    return TC_TLV_OK;
  }
  result =
      tc_x509_crl_name_matches(crl, idp, point, certificate_issuer, limits, tree, names, &matched);
  if (result != TC_TLV_OK)
    return result;
  if (!matched) {
    *out = 0;
    return TC_TLV_OK;
  }
  if (idp && idp->has_reasons)
    reasons &= idp->reasons;
  if (point->has_reasons)
    reasons &= point->reasons;
  *out = reasons;
  return TC_TLV_OK;
}

typedef struct {
  TC_TLV_reader names;
  TC_bytes base, suffix;
  int single_name, done;
} distribution_cursor;
typedef struct {
  unsigned type;
  TC_bytes encoded, value, suffix;
} distribution_identity;

static TC_TLV_result distribution_cursor_init(const TC_X509_distribution_name* name, TC_bytes base,
                                              const TC_TLV_limits* limits,
                                              const tc_pki_tree_workspace* tree,
                                              distribution_cursor* out)
{
  distribution_cursor parsed = {0};
  TC_TLV_result result;
  parsed.single_name = name->relative;
  if (parsed.single_name) {
    result = tc_pki_rdn_contents_check(name->contents, limits, tree);
    if (result != TC_TLV_OK)
      return result;
    result = tc_pki_tree_name(base, TC_TLV_DER, limits, tree);
    parsed.base = base;
    parsed.suffix = name->contents;
  } else {
    result = tc_pki_general_names_contents_check(name->contents, limits, tree);
    if (result != TC_TLV_OK)
      return result;
    result = TC_TLV_reader_init(&parsed.names, name->contents, TC_TLV_DER, limits);
  }
  if (result != TC_TLV_OK)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}

static TC_TLV_result distribution_cursor_next(distribution_cursor* cursor,
                                              const tc_pki_tree_workspace* tree,
                                              distribution_identity* out)
{
  enum { DIRECTORY_NAME = 4, CHOICE_MASK = 0x1f };
  distribution_identity parsed = {0};
  TC_TLV_element element;
  TC_TLV_result result;
  if (cursor->single_name) {
    if (cursor->done)
      return TC_TLV_END;
    if (tc_pki_work_charge(tree->work, 1) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    parsed.type = DIRECTORY_NAME;
    parsed.value = cursor->base;
    parsed.suffix = cursor->suffix;
    cursor->done = 1;
  } else {
    result = tc_pki_tree_next(&cursor->names, tree, &element);
    if (result != TC_TLV_OK)
      return result;
    parsed.type = element.header.tag[0] & CHOICE_MASK;
    parsed.encoded = element.encoded;
    parsed.value = element.value;
  }
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_name_matches(const TC_X509_crl* crl, const TC_X509_crl_distribution* idp,
                                       const tc_pki_distribution_point* point,
                                       TC_bytes certificate_issuer, const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree,
                                       const TC_X509_name_workspace* names, int* matched)
{
  enum { DIRECTORY_NAME = 4 };
  distribution_cursor left, right, right_start;
  distribution_identity a, b;
  TC_bytes base = certificate_issuer;
  TC_X509_distribution_name target;
  TC_TLV_result result;
  if (!crl || !point || !tree || !tree->work || !matched)
    return TC_TLV_ARGUMENT;
  if (!idp || !idp->name.encoded.length) {
    *matched = 1;
    return TC_TLV_OK;
  }
  target = point->name;
  if (!target.encoded.length) {
    target = (TC_X509_distribution_name){{NULL, 0}, point->issuer, 0};
  } else if (target.relative && point->issuer.length) {
    result = tc_pki_distribution_issuer_name(point->issuer, limits, tree, &base);
    if (result != TC_TLV_OK)
      return result;
  }
  result = distribution_cursor_init(&idp->name, crl->issuer, limits, tree, &left);
  if (result != TC_TLV_OK)
    return result;
  if (!point->name.encoded.length && !point->issuer.length) {
    /* Issuer-wide fallback uses the original DN without a GeneralName copy. */
    result = tc_pki_tree_name(certificate_issuer, TC_TLV_DER, limits, tree);
    right_start = (distribution_cursor){0};
    right_start.single_name = 1;
    right_start.base = certificate_issuer;
  } else
    result = distribution_cursor_init(&target, base, limits, tree, &right_start);
  if (result != TC_TLV_OK)
    return result;
  while ((result = distribution_cursor_next(&left, tree, &a)) == TC_TLV_OK) {
    right = right_start;
    while ((result = distribution_cursor_next(&right, tree, &b)) == TC_TLV_OK) {
      int equal;
      if (a.type != b.type)
        continue;
      if (a.type == DIRECTORY_NAME) {
        result = tc_pki_name_appended_equal(a.value, a.suffix, b.value, b.suffix, limits, names,
                                            tree, &equal);
        if (result != TC_TLV_OK)
          return result;
      } else {
        /* IDP locators use the certificate's original encoding (5.2.5). */
        if (tc_pki_work_charge(tree->work, a.encoded.length) != TC_TLV_OK)
          return TC_TLV_LIMIT;
        equal = tc_pki_equal(a.encoded, b.encoded);
      }
      if (equal) {
        *matched = 1;
        return TC_TLV_OK;
      }
    }
    if (result != TC_TLV_END)
      return result;
  }
  if (result != TC_TLV_END)
    return result;
  *matched = 0;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_issuer_matches(const TC_X509_crl* crl,
                                         const TC_X509_crl_distribution* idp,
                                         const tc_pki_distribution_point* point,
                                         TC_bytes certificate_issuer, const TC_TLV_limits* limits,
                                         const tc_pki_tree_workspace* tree,
                                         const TC_X509_name_workspace* names, int* matched)
{
  TC_bytes issuer;
  TC_TLV_result result;
  if (!crl || !point || !tree || !tree->work || !matched || !crl->issuer.length)
    return TC_TLV_ARGUMENT;
  if (!point->issuer.length)
    return TC_X509_name_equal(certificate_issuer, crl->issuer, limits, names, tree->work, matched);
  result = tc_pki_distribution_issuer_name(point->issuer, limits, tree, &issuer);
  if (result != TC_TLV_OK)
    return result;
  if (!idp || !idp->indirect) {
    *matched = 0;
    return TC_TLV_OK;
  }
  /* An explicit cRLIssuer must retain the CRL issuer's encoding (4.2.1.13). */
  if (tc_pki_work_charge(tree->work, issuer.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  *matched = tc_pki_equal(issuer, crl->issuer);
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_distribution_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                            const tc_pki_tree_workspace* tree,
                                            TC_X509_crl_distribution* out)
{
  enum {
    NAME = 0xa0,
    USER_ONLY = 0x81,
    CA_ONLY = 0x82,
    REASONS = 0x83,
    INDIRECT = 0x84,
    ATTRIBUTE_ONLY = 0x85,
    FIELD_MASK = 0x1f
  };
  TC_X509_crl_distribution parsed = {0};
  TC_TLV_reader fields;
  TC_TLV_element element;
  TC_TLV_result result;
  unsigned previous = 0;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = tc_pki_tree_open(encoded, 0x30, TC_TLV_DER, limits, tree, &fields);
  if (result != TC_TLV_OK)
    return result;
  if (tc_pki_end(&fields))
    return TC_TLV_INVALID;
  while (!tc_pki_end(&fields)) {
    result = tc_pki_tree_next(&fields, tree, &element);
    if (result != TC_TLV_OK)
      return result;
    if (element.header.tag_length != 1)
      return TC_TLV_INVALID;
    const unsigned tag = element.header.tag[0];
    const unsigned field = (tag & FIELD_MASK) + 1;
    if (field <= previous)
      return TC_TLV_INVALID;
    previous = field;
    if (tag == NAME) {
      result = tc_pki_distribution_name_read(element.value, limits, tree, &parsed.name);
      if (result != TC_TLV_OK)
        return result;
    } else if (tag == REASONS) {
      result = tc_pki_reason_flags(element.value, &parsed.reasons);
      if (result != TC_TLV_OK)
        return result;
      parsed.has_reasons = 1;
    } else {
      /* DEFAULT FALSE is omitted in DER. Present booleans must be TRUE. */
      if (element.value.length != 1 || element.value.data[0] != 0xff)
        return TC_TLV_INVALID;
      switch (tag) {
      case USER_ONLY:
        parsed.user_only = 1;
        break;
      case CA_ONLY:
        parsed.ca_only = 1;
        break;
      case INDIRECT:
        parsed.indirect = 1;
        break;
      case ATTRIBUTE_ONLY:
        parsed.attribute_only = 1;
        break;
      default:
        return TC_TLV_INVALID;
      }
    }
  }
  if (parsed.user_only + parsed.ca_only + parsed.attribute_only > 1)
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

static TC_TLV_result crl_scalar(TC_bytes encoded, unsigned tag, TC_TLV_element* out)
{
  const TC_TLV_limits limits = {encoded.length, encoded.length, 1, 0};
  TC_TLV_result result = TC_TLV_read(encoded, TC_TLV_DER, &limits, out);
  if (result != TC_TLV_OK)
    return result;
  return tc_pki_tag(out, tag) && out->encoded.length == encoded.length ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result tc_x509_crl_number_read(TC_bytes encoded, TC_bytes* out)
{
  TC_bytes value;
  int negative;
  TC_TLV_result result;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = TC_DER_integer(encoded, &value, &negative);
  if (result != TC_TLV_OK)
    return result;
  if (negative)
    return TC_TLV_INVALID;
  *out = value;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_invalidity_date_read(TC_bytes encoded, TC_X509_time* out)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = crl_scalar(encoded, 0x18, &element);
  if (result != TC_TLV_OK)
    return result;
  return tc_x509_time_value(&element, out);
}

/* Presence and criticality bookkeeping shared by both extension scopes.
 * flag 0 marks an extension the scope does not recognize. The first such
 * critical OID is kept for the policy step. */
static void crl_extension_note(const TC_X509_extension* extension, unsigned flag, unsigned* present,
                               unsigned* critical, TC_bytes* unknown_critical)
{
  if (!flag) {
    if (extension->critical && !unknown_critical->length)
      *unknown_critical = extension->oid;
    return;
  }
  *present |= flag;
  if (extension->critical)
    *critical |= flag;
}

/* Charge a scalar or name value before decoding it. */
static TC_TLV_result crl_extension_charge(const TC_X509_extension* extension,
                                          const tc_pki_tree_workspace* tree)
{
  return tc_pki_work_charge(tree->work, extension->value.length) == TC_TLV_OK ? TC_TLV_OK
                                                                              : TC_TLV_LIMIT;
}

/* GeneralNames SEQUENCE. out borrows its contents and changes only on OK. */
static TC_TLV_result crl_general_names_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                            const tc_pki_tree_workspace* tree, TC_bytes* out)
{
  TC_bytes contents;
  TC_TLV_result result = TC_DER_sequence(encoded, &contents);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_general_names_contents_check(contents, limits, tree);
  if (result != TC_TLV_OK)
    return result;
  *out = contents;
  return TC_TLV_OK;
}

/* CRL-level FreshestCRL. RFC 5280 5.2.6 permits only distribution names. */
static TC_TLV_result crl_freshest_check(TC_bytes encoded, const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree)
{
  TC_TLV_reader points;
  tc_pki_distribution_point point;
  TC_TLV_result result = tc_pki_distribution_points_init(encoded, limits, tree, &points);
  if (result != TC_TLV_OK)
    return result;
  while (!tc_pki_end(&points)) {
    result = tc_pki_distribution_point_next(&points, tree, &point);
    if (result != TC_TLV_OK)
      return result;
    if (point.has_reasons || point.issuer.length)
      return TC_TLV_INVALID;
  }
  return TC_TLV_OK;
}

/* AuthorityKeyIdentifier with a validated authorityCertIssuer when present. */
static TC_TLV_result crl_authority_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree,
                                        TC_X509_authority_key_identifier* out)
{
  TC_X509_authority_key_identifier identifier;
  TC_TLV_result result = TC_X509_authority_key_identifier_read(encoded, limits, &identifier);
  if (result != TC_TLV_OK)
    return result;
  if (identifier.issuer.length) {
    result = tc_pki_general_names_contents_check(identifier.issuer, limits, tree);
    if (result != TC_TLV_OK)
      return result;
  }
  *out = identifier;
  return TC_TLV_OK;
}

static unsigned crl_extension_flag(unsigned id)
{
  switch (id) {
  case TC_PKI_EXT_CRL_NUMBER:
    return TC_X509_CRL_EXT_NUMBER;
  case TC_PKI_EXT_DELTA_CRL_INDICATOR:
    return TC_X509_CRL_EXT_DELTA;
  case TC_PKI_EXT_AUTHORITY_KEY_IDENTIFIER:
    return TC_X509_CRL_EXT_AUTHORITY;
  case TC_PKI_EXT_ISSUING_DISTRIBUTION_POINT:
    return TC_X509_CRL_EXT_DISTRIBUTION;
  case TC_PKI_EXT_FRESHEST_CRL:
    return TC_X509_CRL_EXT_FRESHEST;
  case TC_PKI_EXT_ISSUER_ALT_NAME:
    return TC_X509_CRL_EXT_ISSUER_ALT;
  default:
    return 0;
  }
}

static unsigned crl_entry_extension_flag(unsigned id)
{
  switch (id) {
  case TC_PKI_EXT_REASON_CODE:
    return TC_CRL_ENTRY_REASON;
  case TC_PKI_EXT_INVALIDITY_DATE:
    return TC_CRL_ENTRY_INVALIDITY;
  case TC_PKI_EXT_CERTIFICATE_ISSUER:
    return TC_CRL_ENTRY_ISSUER;
  default:
    return 0;
  }
}

typedef struct {
  const TC_TLV_limits* limits;
  const tc_pki_tree_workspace* tree;
  TC_X509_crl_extensions* out;
} crl_extensions_context;

/* One CRL extension (RFC 5280 5.2). Entry extensions are unrecognized here.
 * Writes go to provisional storage owned by tc_x509_crl_extension_info_read. */
static TC_TLV_result crl_extension_visit(void* context, const TC_X509_extension* extension)
{
  const crl_extensions_context* state = context;
  TC_X509_crl_extensions* out = state->out;
  const unsigned id = tc_pki_extension_id(extension);
  TC_TLV_result result;
  crl_extension_note(extension, crl_extension_flag(id), &out->present, &out->critical,
                     &out->unknown_critical_oid);
  switch (id) {
  case TC_PKI_EXT_FRESHEST_CRL:
    result = crl_freshest_check(extension->value, state->limits, state->tree);
    if (result == TC_TLV_OK)
      out->freshest = extension->value;
    return result;
  case TC_PKI_EXT_ISSUING_DISTRIBUTION_POINT:
    result = tc_x509_crl_distribution_read(extension->value, state->limits, state->tree,
                                           &out->distribution);
    if (result == TC_TLV_OK)
      out->distribution_encoded = extension->value;
    return result;
  case TC_PKI_EXT_AUTHORITY_KEY_IDENTIFIER:
    result = crl_extension_charge(extension, state->tree);
    if (result != TC_TLV_OK)
      return result;
    return crl_authority_read(extension->value, state->limits, state->tree, &out->authority);
  case TC_PKI_EXT_ISSUER_ALT_NAME:
    result = crl_extension_charge(extension, state->tree);
    if (result != TC_TLV_OK)
      return result;
    return crl_general_names_read(extension->value, state->limits, state->tree, &out->issuer_alt);
  case TC_PKI_EXT_CRL_NUMBER:
    result = crl_extension_charge(extension, state->tree);
    if (result != TC_TLV_OK)
      return result;
    return tc_x509_crl_number_read(extension->value, &out->number);
  case TC_PKI_EXT_DELTA_CRL_INDICATOR:
    result = crl_extension_charge(extension, state->tree);
    if (result != TC_TLV_OK)
      return result;
    return tc_x509_crl_number_read(extension->value, &out->base_number);
  default:
    return TC_TLV_OK;
  }
}

typedef struct {
  const TC_TLV_limits* limits;
  const tc_pki_tree_workspace* tree;
  tc_x509_crl_entry_info* out;
} crl_entry_extensions_context;

/* One CRL entry extension (RFC 5280 5.3). CRL-level extensions are
 * unrecognized here. Writes go to provisional storage owned by
 * tc_x509_crl_entry_info_read. */
static TC_TLV_result crl_entry_extension_visit(void* context, const TC_X509_extension* extension)
{
  const crl_entry_extensions_context* state = context;
  tc_x509_crl_entry_info* out = state->out;
  const unsigned id = tc_pki_extension_id(extension);
  const unsigned flag = crl_entry_extension_flag(id);
  TC_TLV_result result;
  crl_extension_note(extension, flag, &out->present, &out->critical, &out->unknown_critical_oid);
  if (!flag)
    return TC_TLV_OK;
  result = crl_extension_charge(extension, state->tree);
  if (result != TC_TLV_OK)
    return result;
  switch (id) {
  case TC_PKI_EXT_REASON_CODE:
    return tc_pki_crl_reason_read(extension->value, &out->reason);
  case TC_PKI_EXT_INVALIDITY_DATE:
    return tc_x509_crl_invalidity_date_read(extension->value, &out->invalidity_date);
  default:
    return crl_general_names_read(extension->value, state->limits, state->tree, &out->issuer);
  }
}

TC_TLV_result tc_x509_crl_entry_info_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                          const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                          size_t capacity, tc_x509_crl_entry_info* out)
{
  tc_x509_crl_entry_info parsed = {0};
  crl_entry_extensions_context context = {limits, tree, &parsed};
  TC_TLV_result result;
  if (!out)
    return TC_TLV_ARGUMENT;
  result = tc_pki_extensions_visit(encoded, limits, tree, oids, capacity, crl_entry_extension_visit,
                                   &context);
  if (result != TC_TLV_OK)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_extension_info_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                              const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                              size_t capacity, TC_X509_crl_extensions* out)
{
  TC_X509_crl_extensions parsed = {0};
  crl_extensions_context context = {limits, tree, &parsed};
  TC_TLV_result result;
  if (!out)
    return TC_TLV_ARGUMENT;
  result =
      tc_pki_extensions_visit(encoded, limits, tree, oids, capacity, crl_extension_visit, &context);
  if (result != TC_TLV_OK)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}
#endif
