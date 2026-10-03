/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Finding a certificate among revoked entries, including indirect-CRL
 * certificate issuers and prepared lookups. */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_crl_internal.h"

static TC_TLV_result crl_prepared_find(const TC_X509_crl_prepared* prepared,
                                       const TC_X509_certificate* certificate, size_t* work,
                                       TC_X509_crl_match* out)
{
  if (!work || (prepared->count && (!prepared->targets || !prepared->matches)) ||
      prepared->count > SIZE_MAX / sizeof *prepared->targets ||
      prepared->count > SIZE_MAX / sizeof *prepared->matches)
    return TC_TLV_ARGUMENT;
  for (size_t i = 0; i < prepared->count; ++i) {
    const TC_X509_crl_target* target = &prepared->targets[i];
    if (!target->serial.data || !target->serial.length || !target->issuer.data ||
        !target->issuer.length)
      return TC_TLV_ARGUMENT;
    if (tc_pki_work_charge(work, 1) != TC_TLV_OK ||
        tc_pki_work_charge(work, target->serial.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    if (!tc_pki_equal(target->serial, certificate->serial))
      continue;
    if (tc_pki_work_charge(work, target->issuer.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    if (!tc_pki_equal(target->issuer, certificate->issuer))
      continue;
    *out = prepared->matches[i];
    return TC_TLV_OK;
  }
  return TC_TLV_UNSUPPORTED;
}

TC_TLV_result tc_x509_crl_find(const TC_X509_crl* crl, const TC_X509_crl_extensions* extensions,
                               const TC_X509_certificate* certificate,
                               const tc_x509_crl_decode* decode, TC_X509_crl_match* out)
{
  if (!decode)
    return TC_TLV_ARGUMENT;
  const TC_TLV_limits* limits = decode->limits;
  const tc_pki_tree_workspace* tree = decode->tree;
  const TC_X509_name_workspace* names = decode->names;
  TC_bytes* oids = decode->oids;
  const size_t capacity = decode->oid_capacity;
  tc_x509_crl_revoked_reader reader;
  tc_x509_crl_revoked_entry entry;
  TC_X509_crl_match parsed = {0};
  TC_TLV_result result;
  if (!certificate || !out || !certificate->serial.data || !certificate->serial.length ||
      !certificate->issuer.data || !certificate->issuer.length)
    return TC_TLV_ARGUMENT;
  if (crl && crl->prepared) {
    if (!tree || crl->encoded.length || crl->tbs.length || crl->revoked.length ||
        (crl->version != 1 && crl->version != 2) || !crl->issuer.data || !crl->issuer.length)
      return TC_TLV_ARGUMENT;
    result = tc_x509_crl_extension_policy(extensions);
    if (result != TC_TLV_OK)
      return result;
    return crl_prepared_find(crl->prepared, certificate, tree->work, out);
  }
  const TC_X509_crl_target query = {certificate->serial, certificate->issuer};
  result = tc_x509_crl_revoked_init(crl, extensions, limits, tree, &reader);
  if (result != TC_TLV_OK)
    return result;
  while ((result = tc_x509_crl_revoked_next(&reader, tree, oids, capacity, &entry)) == TC_TLV_OK) {
    result = tc_x509_crl_match_update(&entry, &query, limits, tree, names, &parsed);
    if (result != TC_TLV_OK)
      return result;
  }
  if (result != TC_TLV_END)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}

/* A NULL query asks whether any directoryName is present. */
static TC_TLV_result entry_issuer_directory_name(TC_bytes names, TC_bytes query,
                                                 const TC_TLV_limits* limits,
                                                 const tc_pki_tree_workspace* tree, int* found)
{
  enum { DIRECTORY_NAME = 0xa4 };
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result = TC_TLV_reader_init(&reader, names, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  while (!tc_pki_end(&reader)) {
    result = tc_pki_tree_next(&reader, tree, &element);
    if (result != TC_TLV_OK)
      return result;
    if (!tc_pki_tag(&element, DIRECTORY_NAME))
      continue;
    if (!query.data) {
      *found = 1;
      return TC_TLV_OK;
    }
    if (tc_pki_work_charge(tree->work, element.value.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    if (tc_pki_equal(element.value, query)) {
      *found = 1;
      return TC_TLV_OK;
    }
  }
  *found = 0;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_query_matches(const tc_x509_crl_revoked_entry* entry,
                                        const TC_X509_crl_target* certificate,
                                        const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree,
                                        const TC_X509_name_workspace* names, int* matched)
{
  if (!entry || !certificate || !tree || !tree->work || !matched || !entry->entry.serial.length ||
      !certificate->serial.length)
    return TC_TLV_ARGUMENT;
  /* Serial contents retain DER sign padding, so equality needs no conversion. */
  if (tc_pki_work_charge(tree->work, entry->entry.serial.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  if (!tc_pki_equal(entry->entry.serial, certificate->serial)) {
    *matched = 0;
    return TC_TLV_OK;
  }
  if (!entry->issuer.names.length)
    return TC_X509_name_equal(entry->issuer.name, certificate->issuer, limits, names, tree->work,
                              matched);
  return entry_issuer_directory_name(entry->issuer.names, certificate->issuer, limits, tree,
                                     matched);
}

TC_TLV_result tc_x509_crl_match_update(const tc_x509_crl_revoked_entry* entry,
                                       const TC_X509_crl_target* query, const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree,
                                       const TC_X509_name_workspace* names,
                                       TC_X509_crl_match* match)
{
  if (!match)
    return TC_TLV_ARGUMENT;
  int matched;
  TC_TLV_result result = tc_x509_crl_query_matches(entry, query, limits, tree, names, &matched);
  if (result != TC_TLV_OK || !matched)
    return result;
  if (match->found)
    return TC_TLV_INVALID;
  match->found = 1;
  match->reason = entry->extensions.reason;
  match->revoked_at = entry->entry.revoked_at;
  match->has_invalidity_date = !!(entry->extensions.present & TC_CRL_ENTRY_INVALIDITY);
  match->invalidity_date = entry->extensions.invalidity_date;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_revoked_init(const TC_X509_crl* crl,
                                       const TC_X509_crl_extensions* extensions,
                                       const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree,
                                       tc_x509_crl_revoked_reader* out)
{
  tc_x509_crl_revoked_reader parsed = {0};
  TC_TLV_result result;
  if (!crl || !extensions || !out || (crl->version != 1 && crl->version != 2) ||
      !crl->issuer.data || !crl->issuer.length)
    return TC_TLV_ARGUMENT;
  result = tc_x509_crl_extension_policy(extensions);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_entries_init(crl->revoked, limits, tree, &parsed.entries);
  if (result != TC_TLV_OK)
    return result;
  parsed.extensions = extensions;
  parsed.version = crl->version;
  parsed.issuer.name = crl->issuer;
  *out = parsed;
  return TC_TLV_OK;
}

/* Entry metadata has already validated the complete GeneralNames contents. */
static TC_TLV_result entry_issuer_has_dn(TC_bytes names, const TC_TLV_limits* limits,
                                         const tc_pki_tree_workspace* tree)
{
  int found;
  TC_TLV_result result =
      entry_issuer_directory_name(names, (TC_bytes){NULL, 0}, limits, tree, &found);
  return result == TC_TLV_OK && !found ? TC_TLV_INVALID : result;
}

TC_TLV_result tc_x509_crl_entry_resolve(const tc_x509_crl_entry* entry,
                                        const TC_X509_crl_extensions* extensions,
                                        const tc_x509_crl_entry_issuer* issuer,
                                        const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                        size_t capacity, tc_x509_crl_revoked_entry* out)
{
  if (!entry || !extensions || !issuer || !out)
    return TC_TLV_ARGUMENT;
  tc_x509_crl_revoked_entry parsed = {0};
  parsed.entry = *entry;
  parsed.issuer = *issuer;
  TC_TLV_result result = tc_x509_crl_entry_info_read(entry->extensions, limits, tree, oids,
                                                     capacity, &parsed.extensions);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_entry_policy(extensions, &parsed.extensions);
  if (result != TC_TLV_OK)
    return result;
  if (parsed.extensions.present & TC_CRL_ENTRY_ISSUER) {
    result = entry_issuer_has_dn(parsed.extensions.issuer, limits, tree);
    if (result != TC_TLV_OK)
      return result;
    parsed.issuer.name = (TC_bytes){NULL, 0};
    parsed.issuer.names = parsed.extensions.issuer;
  }
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_revoked_next(tc_x509_crl_revoked_reader* reader,
                                       const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                       size_t capacity, tc_x509_crl_revoked_entry* out)
{
  tc_x509_crl_revoked_reader next;
  tc_x509_crl_revoked_entry parsed = {0};
  TC_TLV_result result;
  if (!reader || !reader->extensions || !out)
    return TC_TLV_ARGUMENT;
  next = *reader;
  result = tc_x509_crl_entry_next(&next.entries, next.version, tree, &parsed.entry);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_entry_resolve(&parsed.entry, next.extensions, &next.issuer,
                                     &next.entries.limits, tree, oids, capacity, &parsed);
  if (result != TC_TLV_OK)
    return result;
  next.issuer = parsed.issuer;
  *reader = next;
  *out = parsed;
  return TC_TLV_OK;
}
#endif
