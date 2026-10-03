/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* X.509 CRL reader, extension decoding, entry lookup and CRL indexes.
 * Standards: RFC 5280 sections 5 and 6.3.
 * Configuration: TC_ENABLE_X509_REVOCATION.
 * Limitations: parsing and lookup. x509_revocation.h authenticates CRLs and
 * applies them to a path.
 * Contracts: docs/api.md, including its size_t work units.
 * Guide: docs/x509-crl.md. */
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
typedef struct {
  TC_bytes serial, issuer;
} TC_X509_crl_target;

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
  TC_X509_CRL_EXT_NUMBER = 1u << 0,
  TC_X509_CRL_EXT_DELTA = 1u << 1,
  TC_X509_CRL_EXT_AUTHORITY = 1u << 2,
  TC_X509_CRL_EXT_DISTRIBUTION = 1u << 3,
  TC_X509_CRL_EXT_FRESHEST = 1u << 4,
  TC_X509_CRL_EXT_ISSUER_ALT = 1u << 5
};
typedef struct {
  TC_bytes number, base_number, distribution_encoded, freshest, issuer_alt;
  TC_bytes unknown_critical_oid;
  TC_X509_authority_key_identifier authority;
  TC_X509_crl_distribution distribution;
  unsigned present, critical;
} TC_X509_crl_extensions;

/* Library-managed index storage. policy describes extension support only. */
typedef struct {
  TC_X509_crl crl;
  TC_X509_crl_extensions extensions;
  TC_TLV_result policy;
} TC_X509_crl_record;
typedef struct {
  const TC_X509_crl_record* records;
  size_t count;
  /* Non-CRL records omitted by collection adapters. Zero for DER-only input. */
  size_t other_count;
} TC_X509_crl_index;

#if TC_ENABLE_X509_REVOCATION
/* Read a DER CertificateList and check every revoked entry (RFC 5280
 * sections 5.1 and 5.3). Spans borrow the unchanged input. revoked and
 * extensions keep their SEQUENCE wrappers. Read CRL extensions with
 * TC_X509_crl_extensions_read. Signature, freshness and trust checks are
 * separate steps. frames holds one entry per constructed nesting level.
 * encoded, limits, frames, work and out must be disjoint.
 *
 * Work: a 10-unit storage check, then the bytes of the framing, metadata and
 * entry passes.
 * Returns OK with out written. ARGUMENT for NULL limits, work or out, or
 * overlap, with all state unchanged. LIMIT for exhausted limits, frames or
 * work. INVALID for schema failures, including an empty revokedCertificates
 * field and entry extensions outside the CRL version. UNSUPPORTED for an
 * unsupported feature in the encoding. out changes only on OK. Frames and
 * work are provisional on failure. */
TC_TLV_result TC_X509_crl_read(TC_bytes encoded, const TC_TLV_limits* limits, TC_TLV_frames frames,
                               size_t* work, TC_X509_crl* out);

/* Read crl.extensions, or {NULL, 0} when absent (RFC 5280 section 5.2).
 * present and critical use the TC_X509_CRL_EXT masks. Number spans hold
 * INTEGER contents. An unknown critical OID is reported in
 * unknown_critical_oid. All spans borrow the unchanged input. This checks
 * syntax and uniqueness. The caller applies criticality policy and
 * applicability, or uses TC_X509_crl_index_init, which records that policy.
 * workspace frames and extension_oids hold the nesting and one OID per
 * extension. encoded, limits, the workspace metadata and arrays, work and out
 * must be disjoint.
 *
 * Work: a fixed storage check and the extension bytes of each pass.
 * Returns OK with out written. ARGUMENT for NULL arguments or overlap, with
 * all state unchanged. LIMIT for exhausted limits, frames, OID slots or work.
 * INVALID for bad syntax or a repeated extension. out changes only on OK.
 * Scratch and work are provisional on failure. */
TC_TLV_result TC_X509_crl_extensions_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                          const TC_X509_workspace* workspace, size_t* work,
                                          TC_X509_crl_extensions* out);

/* Read count DER CRLs once into caller-owned records and publish them as an
 * index. Each record holds the parsed CRL, its extensions and its extension
 * policy (RFC 5280 section 5.2). A CRL whose policy is INVALID or UNSUPPORTED
 * is indexed with that policy, and the revocation check skips it. limits
 * apply to each CRL. capacity counts record slots. Keep the encodings and
 * records unchanged while the index is used. The encoded array, its bytes,
 * limits and the workspace metadata must be disjoint from the workspace
 * arrays, records, work and out. An empty collection accepts NULL/0 arrays.
 *
 * Work: one unit per storage comparison, charged before any parsing, then the
 * passes of TC_X509_crl_read and TC_X509_crl_extensions_read for each CRL.
 * Returns OK with out written. ARGUMENT for NULL limits, workspace, work or
 * out, or overlap, with all state unchanged. LIMIT for count above capacity,
 * after the storage check, and for exhausted limits, frames or work. INVALID
 * and UNSUPPORTED for a CRL that TC_X509_crl_read rejects. out changes only
 * on OK. Records and scratch are provisional on failure. Signature and trust
 * checks are separate steps. */
TC_TLV_result TC_X509_crl_index_init(const TC_bytes* encoded, size_t count,
                                     const TC_TLV_limits* limits,
                                     const TC_X509_workspace* workspace, size_t* work,
                                     TC_X509_crl_record* records, size_t capacity,
                                     TC_X509_crl_index* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
