/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_X509_CRL_H_
#define TINY_CRYPTO_X509_CRL_H_
#include <tiny_crypto/x509.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_X509_time revoked_at, invalidity_date;
  unsigned reason;
  int found, has_invalidity_date;
} TC_X509_crl_match;
typedef struct { TC_bytes serial, issuer; } TC_X509_crl_target;

/* Library-managed source results. Targets, matches and digest stay unchanged
 * while the prepared CRL is used. A lookup covers an exact serial/issuer pair.
 * Signature verification and trust policy are applied by the revocation resolver. */
typedef struct {
  const TC_X509_crl_target* targets;
  const TC_X509_crl_match* matches;
  size_t count;
  TC_bytes digest;
  TC_hash_algorithm hash;
} TC_X509_crl_prepared;

typedef struct {
  TC_bytes encoded, tbs, issuer, signature, revoked, extensions;
  TC_DER_algorithm signature_algorithm;
  TC_X509_time this_update, next_update;
  unsigned version;
  int has_next_update;
  /* Source-backed records retain metadata spans and prepared results. Their
   * encoded, tbs and revoked spans are empty. Buffer-backed records use NULL. */
  const TC_X509_crl_prepared* prepared;
} TC_X509_crl;

typedef struct {
  TC_X509_distribution_name name;
  uint16_t reasons;
  int has_reasons, user_only, ca_only, indirect, attribute_only;
} TC_X509_crl_distribution;
enum {
  TC_X509_CRL_EXT_NUMBER = 1u << 0, TC_X509_CRL_EXT_DELTA = 1u << 1,
  TC_X509_CRL_EXT_AUTHORITY = 1u << 2, TC_X509_CRL_EXT_DISTRIBUTION = 1u << 3,
  TC_X509_CRL_EXT_FRESHEST = 1u << 4, TC_X509_CRL_EXT_ISSUER_ALT = 1u << 5
};
typedef struct {
  TC_bytes number, base_number, distribution_encoded, freshest, issuer_alt;
  TC_bytes unknown_critical_oid;
  TC_X509_authority_key_identifier authority;
  TC_X509_crl_distribution distribution;
  unsigned present, critical;
} TC_X509_crl_extensions;

/* Library-managed index storage. policy describes extension support, not trust. */
typedef struct {
  TC_X509_crl crl;
  TC_X509_crl_extensions extensions;
  TC_TLV_result policy;
} TC_X509_crl_record;
typedef struct {
  const TC_X509_crl_record* records;
  size_t count;
  /* Non-CRL records omitted by collection adapters; zero for DER-only input. */
  size_t other_count;
} TC_X509_crl_index;

/* Read a DER CertificateList. Spans borrow the unchanged input; revoked and
 * extensions retain their SEQUENCE wrappers. Extension values need separate
 * interpretation. Parsing does not verify signatures, freshness or trust.
 * Input, limits, frames, work and out must be disjoint. Frames and work may
 * change on failure; out changes only on OK. Frame capacity counts elements. */
TC_TLV_result TC_X509_crl_read(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_X509_crl* out);

/* Read crl.extensions, or {NULL,0} when absent. present/critical use EXT masks.
 * Number spans contain INTEGER contents; all spans borrow unchanged input.
 * An unknown critical OID is reported, not accepted as an understood extension.
 * This checks syntax and uniqueness, not criticality policy or applicability.
 * Input, limits, workspace metadata/arrays, work and out must be disjoint.
 * Scratch/work are provisional; out changes only on OK. */
TC_TLV_result TC_X509_crl_extensions_read(TC_bytes encoded, const TC_TLV_limits* limits,
    const TC_X509_workspace* workspace, size_t* work, TC_X509_crl_extensions* out);

/* Index DER CRLs once into caller-owned records. Limits apply to each CRL;
 * one work budget covers the collection. capacity counts record slots.
 * Keep encodings and indexed records unchanged while using the index.
 * Inputs/metadata, workspace arrays, records, work and out must be disjoint.
 * Records and scratch are provisional on failure; out changes only on OK.
 * Empty input accepts NULL/0 arrays. No signatures or trust are checked. */
TC_TLV_result TC_X509_crl_index_init(const TC_bytes* encoded, size_t count,
    const TC_TLV_limits* limits, const TC_X509_workspace* workspace, size_t* work,
    TC_X509_crl_record* records, size_t capacity, TC_X509_crl_index* out);

#ifdef __cplusplus
}
#endif
#endif
