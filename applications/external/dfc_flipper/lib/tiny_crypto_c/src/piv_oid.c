/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PIV and TWIC object identifiers from TWIC Part 2 v5 section 6. Each TWIC
 * identifier has the same meaning as its PIV pair and uses the same arcs
 * beneath the TWIC root. PIV-only entries come from FIPS 201-3 Tables B-1
 * and B-2 and from the Federal PKI Common Policy section 1.2. */
#include "piv_oid_internal.h"
#if TC_ENABLE_PIV_OIDS
#include <string.h>

#define PIV_ROOT 0x60, 0x86, 0x48, 1, 0x65, 3        /* 2.16.840.1.101.3 */
#define TWIC_ROOT 0x2b, 6, 1, 4, 1, 0x81, 0xe3, 0x52 /* 1.3.6.1.4.1.29138 */
#define OID_SPAN(contents) {contents, sizeof contents}
#define OID_NONE {NULL, 0}

/* Certificate policies. */
static const uint8_t twic_digital_signature[] = {TWIC_ROOT, 2, 1, 3, 5};
static const uint8_t fpki_common_policy[] = {PIV_ROOT, 2, 1, 3, 6};
static const uint8_t twic_key_management[] = {TWIC_ROOT, 2, 1, 3, 6};
static const uint8_t fpki_common_devices[] = {PIV_ROOT, 2, 1, 3, 8};
static const uint8_t twic_devices[] = {TWIC_ROOT, 2, 1, 3, 8};
static const uint8_t fpki_common_authentication[] = {PIV_ROOT, 2, 1, 3, 13};
static const uint8_t twic_authentication[] = {TWIC_ROOT, 2, 1, 3, 13};
static const uint8_t fpki_common_card_auth[] = {PIV_ROOT, 2, 1, 3, 17};
static const uint8_t twic_card_auth_policy[] = {TWIC_ROOT, 2, 1, 3, 17};
static const uint8_t fpki_common_content_signing[] = {PIV_ROOT, 2, 1, 3, 39};
/* CMS content types and attributes. */
static const uint8_t piv_chuid_content[] = {PIV_ROOT, 6, 1};
static const uint8_t piv_biometric_content[] = {PIV_ROOT, 6, 2};
static const uint8_t piv_signer_name[] = {PIV_ROOT, 6, 5};
static const uint8_t piv_fascn[] = {PIV_ROOT, 6, 6};
static const uint8_t twic_fascn[] = {TWIC_ROOT, 6, 6};
/* Extended key usages. */
static const uint8_t piv_content_signing[] = {PIV_ROOT, 6, 7};
static const uint8_t twic_content_signing[] = {TWIC_ROOT, 6, 7};
static const uint8_t piv_card_auth[] = {PIV_ROOT, 6, 8};
static const uint8_t twic_card_auth[] = {TWIC_ROOT, 6, 8};
/* Certificate extensions. */
static const uint8_t piv_naci[] = {PIV_ROOT, 6, 9, 1};
static const uint8_t twic_interim[] = {TWIC_ROOT, 6, 9, 1};

/* One row per identifier. An empty span means the namespace has no entry. */
static const struct {
  TC_PIV_oid id;
  TC_bytes piv;
  TC_bytes twic;
} piv_oids[] = {
    {TC_PIV_OID_POLICY_DIGITAL_SIGNATURE, OID_NONE, OID_SPAN(twic_digital_signature)},
    {TC_PIV_OID_POLICY_COMMON, OID_SPAN(fpki_common_policy), OID_SPAN(twic_key_management)},
    {TC_PIV_OID_POLICY_DEVICES, OID_SPAN(fpki_common_devices), OID_SPAN(twic_devices)},
    {TC_PIV_OID_POLICY_AUTHENTICATION, OID_SPAN(fpki_common_authentication),
     OID_SPAN(twic_authentication)},
    {TC_PIV_OID_POLICY_CARD_AUTHENTICATION, OID_SPAN(fpki_common_card_auth),
     OID_SPAN(twic_card_auth_policy)},
    {TC_PIV_OID_POLICY_CONTENT_SIGNING, OID_SPAN(fpki_common_content_signing), OID_NONE},
    {TC_PIV_OID_CHUID_CONTENT, OID_SPAN(piv_chuid_content), OID_NONE},
    {TC_PIV_OID_BIOMETRIC_CONTENT, OID_SPAN(piv_biometric_content), OID_NONE},
    {TC_PIV_OID_SIGNER_NAME, OID_SPAN(piv_signer_name), OID_NONE},
    {TC_PIV_OID_FASCN, OID_SPAN(piv_fascn), OID_SPAN(twic_fascn)},
    {TC_PIV_OID_CONTENT_SIGNING, OID_SPAN(piv_content_signing), OID_SPAN(twic_content_signing)},
    {TC_PIV_OID_CARD_AUTHENTICATION, OID_SPAN(piv_card_auth), OID_SPAN(twic_card_auth)},
    {TC_PIV_OID_BACKGROUND_CHECK, OID_SPAN(piv_naci), OID_SPAN(twic_interim)},
};

static int oid_equal(TC_bytes oid, const TC_bytes* entry)
{
  return entry->length && oid.length == entry->length &&
         !memcmp(oid.data, entry->data, entry->length);
}

TC_PIV_oid TC_PIV_oid_identify(TC_bytes oid, TC_PIV_oid_profile profile)
{
  if (!oid.data || (profile != TC_PIV_OIDS_ONLY && profile != TC_PIV_OIDS_TWIC_COMPATIBLE))
    return TC_PIV_OID_UNKNOWN;
  for (size_t i = 0; i < sizeof piv_oids / sizeof *piv_oids; ++i) {
    if (oid_equal(oid, &piv_oids[i].piv))
      return piv_oids[i].id;
    /* TWIC Part 2 v5 section 6: TWIC readers accept either namespace. */
    if (profile == TC_PIV_OIDS_TWIC_COMPATIBLE && oid_equal(oid, &piv_oids[i].twic))
      return piv_oids[i].id;
  }
  return TC_PIV_OID_UNKNOWN;
}

const TC_bytes* tc_piv_oid_contents(TC_PIV_oid id, tc_piv_oid_namespace space)
{
  if (space != TC_PIV_OID_NAMESPACE_PIV && space != TC_PIV_OID_NAMESPACE_TWIC)
    return NULL;
  for (size_t i = 0; i < sizeof piv_oids / sizeof *piv_oids; ++i) {
    if (piv_oids[i].id != id)
      continue;
    const TC_bytes* contents =
        space == TC_PIV_OID_NAMESPACE_PIV ? &piv_oids[i].piv : &piv_oids[i].twic;
    return contents->length ? contents : NULL;
  }
  return NULL;
}
#endif
