/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Content-signer and card-profile policy shared by the PIV and TWIC credential
 * validators. These helpers turn the card profile and the signer certificate
 * into path options: the content-signing EKU (PIV or TWIC OID, per the TWIC
 * part 2 section 6 pairs), id-fpki-common-piv-contentSigning
 * (2.16.840.1.101.3.2.1.3.39) as an explicit policy for PIV cards, and the
 * card-expiration bound on the signer. Signature and path checks run
 * afterwards in the validators. */
#include <string.h>
#include <tiny_crypto/credential.h>
#include <tiny_crypto/piv_biometric.h>
#include <tiny_crypto/piv_cms.h>

#if TC_ENABLE_CREDENTIAL
#include "internal.h"
#include "pki_budget_internal.h"
#include "pki_extensions_internal.h"
#include "pki_source_internal.h"
#include "piv_oid_internal.h"
#include "validation_internal.h"
#include "cms_internal.h"
#include "credential_status_internal.h"
#include "credential_policy_internal.h"
#include "credential_text_internal.h"

/* What one pass over the signer's extensions looks for and found. */
typedef struct {
  /* Certificate policy the signer must assert, or NULL when none is required. */
  const TC_bytes* required_policy;
  int policy_found;
  /* Set when the path purpose comes from the signer's content-signing EKU. */
  int find_purpose;
  TC_PIV_oid_profile purpose_oids;
  TC_bytes purpose;
} signer_extensions;

static TC_TLV_result signer_policies_scan(TC_bytes value, signer_extensions* scan,
                                          const TC_TLV_limits* limits,
                                          const TC_X509_path_workspace* storage, size_t* work)
{
  TC_X509_policy_reader policies;
  TC_X509_policy policy;
  if (tc_pki_work_charge(work, value.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  TC_TLV_result status =
      TC_X509_policies_init(&policies, value, limits, storage->oids, storage->oid_capacity);
  if (status != TC_TLV_OK)
    return status;
  while ((status = TC_X509_policy_next(&policies, &policy)) == TC_TLV_OK)
    if (policy.oid.length == scan->required_policy->length &&
        !memcmp(policy.oid.data, scan->required_policy->data, policy.oid.length))
      scan->policy_found = 1;
  return status == TC_TLV_END ? TC_TLV_OK : status;
}

static TC_TLV_result signer_purpose_scan(TC_bytes value, signer_extensions* scan,
                                         const TC_TLV_limits* limits,
                                         const TC_X509_path_workspace* storage, size_t* work)
{
  size_t count;
  if (tc_pki_work_charge(work, value.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  TC_TLV_result status =
      TC_X509_extended_key_usage_read(value, limits, storage->oids, storage->oid_capacity, &count);
  if (status != TC_TLV_OK)
    return status;
  /* The span borrows certificate bytes, so later scans may reuse the OID slots. */
  for (size_t i = 0; i < count && !scan->purpose.length; ++i)
    if (TC_PIV_oid_identify(storage->oids[i], scan->purpose_oids) == TC_PIV_OID_CONTENT_SIGNING)
      scan->purpose = storage->oids[i];
  return TC_TLV_OK;
}

/* Walk the signer's extensions once for certificatePolicies and EKU. */
static TC_TLV_result signer_extensions_scan(const TC_X509_certificate* signer,
                                            signer_extensions* scan, const TC_TLV_limits* limits,
                                            const TC_X509_path_workspace* storage, size_t* work)
{
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result status = tc_pki_extensions_init(&reader, signer, limits, work);
  if (status != TC_TLV_OK)
    return status;
  while ((status = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
    const unsigned id = tc_pki_extension_id(&extension);
    if (id == TC_PKI_EXT_CERTIFICATE_POLICIES && scan->required_policy)
      status = signer_policies_scan(extension.value, scan, limits, storage, work);
    else if (id == TC_PKI_EXT_EXTENDED_KEY_USAGE && scan->find_purpose)
      status = signer_purpose_scan(extension.value, scan, limits, storage, work);
    if (status != TC_TLV_OK)
      return status;
  }
  return status == TC_TLV_END ? TC_TLV_OK : status;
}

TC_TLV_result tc_credential_signer_read(TC_bytes certificate, const TC_TLV_limits* limits,
                                        const TC_X509_path_workspace* storage, size_t* work,
                                        TC_X509_certificate* out)
{
  TC_X509_workspace parser = {storage->frames, storage->oids, storage->oid_capacity};
  if (tc_pki_work_charge(work, certificate.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  return TC_X509_read(certificate, limits, &parser, out);
}

TC_TLV_result tc_credential_signer_policy(const TC_X509_certificate* signer, int piv,
                                          int twic_compatible, const TC_X509_time* card_expiration,
                                          TC_X509_path_options* policy,
                                          const TC_X509_path_workspace* storage, size_t* work)
{
  /* id-fpki-common-piv-contentSigning, Common Policy section 1.2. The span is
   * static, so the path options may keep it as the initial policy set. */
  const TC_bytes* required_policy =
      piv ? tc_piv_oid_contents(TC_PIV_OID_POLICY_CONTENT_SIGNING, TC_PIV_OID_NAMESPACE_PIV) : NULL;
  /* TWIC signers may carry either content-signing OID (TWIC Part 2 section 6),
   * so their own EKU selects the purpose. */
  signer_extensions scan = {required_policy,
                            0,
                            !piv || !policy->purpose.length,
                            twic_compatible ? TC_PIV_OIDS_TWIC_COMPATIBLE : TC_PIV_OIDS_ONLY,
                            {NULL, 0}};
  TC_TLV_result status = signer_extensions_scan(signer, &scan, &policy->parsing, storage, work);
  if (status != TC_TLV_OK)
    return status;
  if ((scan.find_purpose && !scan.purpose.length) || (required_policy && !scan.policy_found))
    return TC_TLV_INVALID;
  if (piv && card_expiration) {
    int order;
    status = TC_X509_time_compare(&signer->not_after, card_expiration, &order);
    if (status != TC_TLV_OK)
      return status;
    if (order < 0)
      return TC_TLV_INVALID;
  }
  if (scan.find_purpose)
    policy->purpose = scan.purpose;
  if (piv) {
    policy->initial_policies = required_policy;
    policy->initial_policy_count = 1;
    policy->flags |= TC_X509_PATH_REQUIRE_EXPLICIT_POLICY;
  }
  policy->key_usage |= TC_KEY_USAGE_DIGITAL_SIGNATURE;
  policy->flags |= TC_X509_PATH_REQUIRE_KEY_USAGE | TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE |
                   TC_X509_PATH_INHIBIT_ANY_PURPOSE;
  return TC_TLV_OK;
}

TC_TLV_result tc_credential_chuid_expiration_check(TC_bytes expiration, const TC_X509_time* at,
                                                   int* valid)
{
  if (!at || !valid || !expiration.data || expiration.length != 8)
    return TC_TLV_ARGUMENT;
  unsigned year, month, day;
  if (!tc_credential_yyyymmdd(expiration.data, expiration.length, &year, &month, &day))
    return TC_TLV_INVALID;
  /* The card remains valid through the last second of its expiration day. */
  const TC_X509_time expires = {year, (uint8_t)month, (uint8_t)day, 23, 59, 59};
  int order;
  TC_TLV_result result = TC_X509_time_compare(at, &expires, &order);
  if (result == TC_TLV_OK)
    *valid = order <= 0;
  return result;
}

int tc_credential_profile(TC_PIV_card_profile profile, const TC_validation_options* options,
                          int* piv, TC_PIV_oid_profile* oids)
{
  if (!options || !piv || !oids ||
      (profile != TC_PIV_CARD && profile != TC_TWIC_LEGACY_CARD && profile != TC_TWIC_NEXGEN_CARD))
    return 0;
  *piv = profile == TC_PIV_CARD;
  *oids = *piv ? TC_PIV_OIDS_ONLY : TC_PIV_OIDS_TWIC_COMPATIBLE;
  return (!options->certificate.purpose.data && !options->certificate.purpose.length) ||
         TC_PIV_oid_identify(options->certificate.purpose, *oids) == TC_PIV_OID_CONTENT_SIGNING;
}

#endif
