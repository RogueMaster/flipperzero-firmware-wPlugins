/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV and TWIC data object catalogs (SP 800-73-5 Part 1 Tables 2, 3 and 8,
 * TWIC Part 2 v5 4.5 to 4.7). */
#include <tiny_crypto/piv_catalog.h>
#if TC_ENABLE_PIV_CATALOG
#include <string.h>

enum {
  ALWAYS = TC_PIV_ACCESS_ALWAYS,
  PIN = TC_PIV_ACCESS_PIN,
  PIN_OR_OCC = TC_PIV_ACCESS_PIN_OR_OCC,
  VCI = TC_PIV_ACCESS_VCI,
  VCI_PIN = TC_PIV_ACCESS_VCI_PIN,
  VCI_PIN_OR_OCC = TC_PIV_ACCESS_VCI_PIN_OR_OCC,
  NEVER = TC_PIV_ACCESS_NEVER,
  M = TC_PIV_MANDATORY,
  C = TC_PIV_CONDITIONAL,
  O = TC_PIV_OPTIONAL,
  SECRET = TC_PIV_OBJECT_SECRET
};

/* A 3-byte tag 5F C1 xx, or DF C0 xx and DF C1 xx for TWIC. */
#define TAG3(a, b, c) {a, b, c}, 3
#define PIV_TAG(last) TAG3(0x5f, 0xc1, last)
/* Retired key management certificate n (1 to 20): 5FC10D to 5FC120, key
 * references 82 to 95, container IDs 1001 to 1014 (Table 8). */
#define RETIRED(n)                                                                                 \
  {PIV_TAG(0x0c + (n)), TC_PIV_KIND_CERTIFICATE, ALWAYS, VCI, O, 0x81 + (n), 0, 0x1000 + (n), 1895}

/* SP 800-73-5 Part 1 Table 3 order. Access rules from Table 2, M/O/C from
 * Tables 2 and 3, container IDs, capacities and key references from Table
 * 8. 5FC123 needs the PIN or OCC, and VCI on contactless (Table 2). */
static const TC_PIV_object_info piv_catalog[TC_PIV_CATALOG_PIV_OBJECTS] = {
    {PIV_TAG(0x07), TC_PIV_KIND_CCC, ALWAYS, VCI, M, 0, 0, 0xdb00, 170},
    {PIV_TAG(0x02), TC_PIV_KIND_CHUID, ALWAYS, ALWAYS, M, 0, 0, 0x3000, 2881},
    {PIV_TAG(0x05), TC_PIV_KIND_CERTIFICATE, ALWAYS, VCI, M, 0x9a, 0, 0x0101, 1857},
    {PIV_TAG(0x03), TC_PIV_KIND_FINGERPRINTS, PIN, VCI_PIN, M, 0, 0, 0x6010, 4006},
    {PIV_TAG(0x06), TC_PIV_KIND_SECURITY, ALWAYS, VCI, M, 0, 0, 0x9000, 1336},
    {PIV_TAG(0x08), TC_PIV_KIND_FACE, PIN, VCI_PIN, M, 0, 0, 0x6030, 12710},
    {PIV_TAG(0x01), TC_PIV_KIND_CERTIFICATE, ALWAYS, ALWAYS, M, 0x9e, 0, 0x0500, 1857},
    {PIV_TAG(0x0a), TC_PIV_KIND_CERTIFICATE, ALWAYS, VCI, C, 0x9c, 0, 0x0100, 1857},
    {PIV_TAG(0x0b), TC_PIV_KIND_CERTIFICATE, ALWAYS, VCI, C, 0x9d, 0, 0x0102, 1857},
    {PIV_TAG(0x09), TC_PIV_KIND_PRINTED, PIN_OR_OCC, VCI_PIN_OR_OCC, O, 0, 0, 0x3001, 245},
    {{0x7e}, 1, TC_PIV_KIND_DISCOVERY, ALWAYS, ALWAYS, O, 0, 0, 0x6050, 19},
    {PIV_TAG(0x0c), TC_PIV_KIND_KEY_HISTORY, ALWAYS, VCI, O, 0, 0, 0x6060, 128},
    RETIRED(1),
    RETIRED(2),
    RETIRED(3),
    RETIRED(4),
    RETIRED(5),
    RETIRED(6),
    RETIRED(7),
    RETIRED(8),
    RETIRED(9),
    RETIRED(10),
    RETIRED(11),
    RETIRED(12),
    RETIRED(13),
    RETIRED(14),
    RETIRED(15),
    RETIRED(16),
    RETIRED(17),
    RETIRED(18),
    RETIRED(19),
    RETIRED(20),
    {PIV_TAG(0x21), TC_PIV_KIND_IRIS, PIN, VCI_PIN, O, 0, 0, 0x1015, 7106},
    {{0x7f, 0x61}, 2, TC_PIV_KIND_BIT_GROUP, ALWAYS, ALWAYS, O, 0, 0, 0x1016, 65},
    {PIV_TAG(0x22), TC_PIV_KIND_SM_SIGNER, ALWAYS, ALWAYS, O, 0, 0, 0x1017, 2471},
    {PIV_TAG(0x23), TC_PIV_KIND_PAIRING_CODE, PIN_OR_OCC, VCI_PIN_OR_OCC, O, 0, SECRET, 0x1018, 12},
};

/* TWIC card application, TWIC Part 2 v5 4.5 table order. The capacities
 * are the 4.6 and 4.7 maximum values plus their structure bytes. */
#define TWIC_CHUID {PIV_TAG(0x02), TC_PIV_KIND_CHUID, ALWAYS, ALWAYS, M, 0, 0, 0x3000, 2469}
#define TWIC_UNSIGNED_CHUID                                                                        \
  {PIV_TAG(0x04), TC_PIV_KIND_UNSIGNED_CHUID, ALWAYS, ALWAYS, M, 0, 0, 0x3002, 57}
/* The TWIC Privacy Key reads on contact only (4.5, 4.6.2). */
#define TWIC_PRIVACY_KEY                                                                           \
  {TAG3(0xdf, 0xc1, 0x01), TC_PIV_KIND_TWIC_PRIVACY_KEY, ALWAYS, NEVER, M, 0, SECRET, 0x2001, 40}
#define TWIC_FINGERPRINTS                                                                          \
  {TAG3(0xdf, 0xc1, 0x03), TC_PIV_KIND_FINGERPRINTS, ALWAYS, ALWAYS, M, 0, 0, 0x2003, 2504}
#define TWIC_SECURITY                                                                              \
  {TAG3(0xdf, 0xc1, 0x0f), TC_PIV_KIND_SECURITY, ALWAYS, ALWAYS, M, 0, 0, 0x9000, 920}

static const TC_PIV_object_info twic_legacy_catalog[] = {
    TWIC_CHUID, TWIC_UNSIGNED_CHUID, TWIC_PRIVACY_KEY, TWIC_FINGERPRINTS, TWIC_SECURITY,
};

/* NEXGEN adds the card authentication certificate, the Discovery Object
 * (4.7.5, ISO form) and the TPK-encrypted objects of 4.7. */
static const TC_PIV_object_info twic_nexgen_catalog[] = {
    {PIV_TAG(0x01), TC_PIV_KIND_CERTIFICATE, ALWAYS, ALWAYS, M, 0x9e, 0, 0x0500, 1863},
    TWIC_CHUID,
    TWIC_UNSIGNED_CHUID,
    {{0x7e}, 1, TC_PIV_KIND_DISCOVERY, ALWAYS, ALWAYS, M, 0, 0, 0x6050, 20},
    {TAG3(0xdf, 0xc0, 0x01), TC_PIV_KIND_TWIC_PERSONAL, ALWAYS, ALWAYS, O, 0, 0, 0x6011, 4100},
    {TAG3(0xdf, 0xc0, 0x02), TC_PIV_KIND_TWIC_SIGNATURE_IMAGE, ALWAYS, ALWAYS, O, 0, 0, 0x6012,
     8132},
    TWIC_PRIVACY_KEY,
    TWIC_FINGERPRINTS,
    {TAG3(0xdf, 0xc1, 0x08), TC_PIV_KIND_FACE, ALWAYS, ALWAYS, M, 0, 0, 0x6030, 16714},
    {TAG3(0xdf, 0xc1, 0x09), TC_PIV_KIND_PRINTED, ALWAYS, ALWAYS, M, 0, 0, 0x3001, 203},
    TWIC_SECURITY,
    {TAG3(0xdf, 0xc1, 0x21), TC_PIV_KIND_IRIS, ALWAYS, ALWAYS, O, 0, 0, 0x1015, 7104},
};

/* The catalog of the pair and its entry count, or NULL. */
static const TC_PIV_object_info* catalog(TC_PIV_application_id application,
                                         TC_PIV_card_profile profile, size_t* count)
{
  if (application == TC_PIV_APPLICATION_PIV && profile == TC_PIV_CARD) {
    *count = sizeof piv_catalog / sizeof *piv_catalog;
    return piv_catalog;
  }
  if (application == TC_PIV_APPLICATION_TWIC && profile == TC_TWIC_LEGACY_CARD) {
    *count = sizeof twic_legacy_catalog / sizeof *twic_legacy_catalog;
    return twic_legacy_catalog;
  }
  if (application == TC_PIV_APPLICATION_TWIC && profile == TC_TWIC_NEXGEN_CARD) {
    *count = sizeof twic_nexgen_catalog / sizeof *twic_nexgen_catalog;
    return twic_nexgen_catalog;
  }
  *count = 0;
  return NULL;
}

size_t TC_PIV_catalog_count(TC_PIV_application_id application, TC_PIV_card_profile profile)
{
  size_t count;
  (void)catalog(application, profile, &count);
  return count;
}

const TC_PIV_object_info* TC_PIV_catalog_at(TC_PIV_application_id application,
                                            TC_PIV_card_profile profile, size_t index)
{
  size_t count;
  const TC_PIV_object_info* entries = catalog(application, profile, &count);
  return index < count ? &entries[index] : NULL;
}

const TC_PIV_object_info* TC_PIV_catalog_find(TC_PIV_application_id application,
                                              TC_PIV_card_profile profile, TC_bytes tag)
{
  size_t count;
  const TC_PIV_object_info* entries = catalog(application, profile, &count);
  if (!tag.data)
    return NULL;
  for (size_t i = 0; i < count; ++i)
    if (entries[i].tag_length == tag.length && !memcmp(entries[i].tag, tag.data, tag.length))
      return &entries[i];
  return NULL;
}
#endif
