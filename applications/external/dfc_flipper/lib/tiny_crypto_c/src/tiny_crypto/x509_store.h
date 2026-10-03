/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Certificate store: published certificate sources, trust anchors with path
 * controls, snapshot lifetimes and anchors built from root certificates.
 * Standards: RFC 5914 section 2.5, RFC 5937.
 * Configuration: TC_ENABLE_X509_PATH.
 * Contracts: docs/api.md. Guide: docs/x509-store.md. */
#ifndef TINY_CRYPTO_X509_STORE_H_
#define TINY_CRYPTO_X509_STORE_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/snapshot.h>
#ifdef __cplusplus
extern "C" {
#endif

/* replaced_controls bits. Each marks a TrustAnchorInfo CertPathControls field
 * whose normalized value replaces the corresponding certificate_extensions
 * control (RFC 5914 section 2.5). */
enum {
  /* policySet replaces certificatePolicies in policy_set. */
  TC_X509_ANCHOR_REPLACED_POLICY_SET = 1u << 0,
  /* policyFlags replace policyConstraints and inhibitAnyPolicy in
   * policy_flags. */
  TC_X509_ANCHOR_REPLACED_POLICY_FLAGS = 1u << 1,
  /* nameConstr replaces nameConstraints in names. */
  TC_X509_ANCHOR_REPLACED_NAMES = 1u << 2,
  /* pathLenConstraint replaces the basicConstraints pathLen in path_len. */
  TC_X509_ANCHOR_REPLACED_PATH_LEN = 1u << 3
};

/* Local authorization carried with a configured anchor. Certificate-derived
 * records set CRL_SIGN for v1/v2 certificates or for v3 keyUsage with
 * cRLSign. TrustAnchorInfo and caller-built records opt in explicitly. */
enum { TC_X509_ANCHOR_USAGE_CRL_SIGN = 1u << 0 };

/* One trust anchor with its RFC 5937 path controls. Validation applies the
 * normalized fields names, policy_set, policy_flags and path_len. The
 * extension spans are checked for controls those fields must reflect.
 *
 * Borrowed DER spans. Policy and extension spans contain SEQUENCE contents.
 * extensions holds TrustAnchorInfo exts, which must omit
 * certificatePolicies, policyConstraints, inhibitAnyPolicy or nameConstraints
 * (RFC 5914 section 2.6). certificate_extensions holds the anchor
 * certificate's own extensions. A path control there whose normalized field
 * is empty makes validation return UNSUPPORTED, unless replaced_controls marks
 * it as replaced. Build records from certificates with
 * TC_X509_store_anchor_from_certificate, or from a TrustAnchorList with
 * TC_X509_trust_anchor_next. */
typedef struct {
  TC_X509_trust_anchor trust;
  TC_X509_name_constraints names;
  TC_bytes key_id, title, title_language;
  TC_bytes policy_set, extensions, certificate_extensions;
  /* TC_X509_PATH_REQUIRE_EXPLICIT_POLICY, _INHIBIT_MAPPING and
   * _INHIBIT_ANY_POLICY only. */
  unsigned policy_flags;
  /* TC_X509_ANCHOR_USAGE_* bits. Unknown bits make validation fail. */
  unsigned usage;
  /* TC_X509_ANCHOR_REPLACED_* bits. TC_X509_trust_anchor_next sets them.
   * Caller-built records set a bit only when the normalized field holds the
   * replacing value. Other bits make validation return ERROR. */
  unsigned replaced_controls;
  /* Non-self-issued intermediates allowed below the anchor. */
  size_t path_len;
  uint8_t has_path_len;
  /* A TrustAnchorInfo without certPath is valid data with no X.509 name. */
  uint8_t x509_unusable;
} TC_X509_store_anchor;

#if TC_ENABLE_X509_PATH
/* Build an anchor record from a parsed anchor certificate with the rules of
 * the RFC 5914 TrustAnchorList certificate choice. certificatePolicies,
 * nameConstraints, policyConstraints, inhibitAnyPolicy and the
 * basicConstraints pathLen fill the normalized fields (RFC 5937 section 2).
 * The record borrows the certificate's DER. Keep that DER unchanged while
 * the record is in use. The certificate view itself may be discarded.
 * workspace supplies frames and extension OID scratch. Charges no work.
 *
 * Returns OK and writes out. NULL arguments, NULL scratch with a nonzero
 * capacity, and out overlapping the certificate view, its DER or the scratch
 * return ARGUMENT with out unchanged. After those checks, failures zero out:
 * INVALID for an empty subject, a reversed validity period, a keyUsage
 * without keyCertSign, or malformed or duplicate extensions and policies.
 * LIMIT for exhausted limits or scratch. The anchor's validity period is
 * never compared with a time. */
TC_TLV_result TC_X509_store_anchor_from_certificate(const TC_X509_certificate* certificate,
                                                    const TC_TLV_limits* limits,
                                                    const TC_X509_workspace* workspace,
                                                    TC_X509_store_anchor* out);
#endif

/* Array-backed source for a fixed, caller-owned snapshot. All records and
 * their borrowed DER must remain stable until readers release the snapshot. */
typedef struct {
  const TC_bytes* candidates;
  size_t candidate_count;
  const TC_X509_store_anchor* anchors;
  size_t anchor_count;
} TC_X509_store_array;

/* Candidates are untrusted certificates. Anchors carry explicit local trust.
 * Callbacks return borrowed records, consume work without increasing it, and
 * return OK only after writing out. An unavailable record is a read error.
 * Reads propagate LIMIT and UNSUPPORTED. Other failure codes become ARGUMENT.
 * Record bytes and ordering remain stable while the source is in use. */
typedef struct {
  void* context;
  size_t candidate_count, anchor_count;
  TC_TLV_result (*candidate)(void* context, size_t index, size_t* work, TC_bytes* out);
  TC_TLV_result (*anchor)(void* context, size_t index, size_t* work, TC_X509_store_anchor* out);
} TC_X509_store_source;
#if TC_ENABLE_X509_PATH
/* Describe a TC_X509_store_array as a source. out->context points at array,
 * so keep the array and its records stable while the source is used. Each
 * record read charges one work unit. Charges no work itself.
 * Returns OK with out written. ARGUMENT for NULL arguments, NULL record
 * arrays with nonzero counts, or out overlapping array. out changes only on
 * OK. */
TC_TLV_result TC_X509_store_array_source(const TC_X509_store_array* array,
                                         TC_X509_store_source* out);
#endif

/* Zero-initialize these objects. Fields are managed by the store functions.
 * state follows the TC_snapshot_state lifecycle in snapshot.h. */
typedef struct {
  TC_X509_store_source source;
  size_t readers;
  TC_snapshot_state state;
} TC_X509_store_snapshot;
typedef struct {
  TC_X509_store_snapshot* current;
  size_t revision;
} TC_X509_store;

#if TC_ENABLE_X509_PATH
/* Snapshot store. Serialize every call with the application's lock. Store,
 * slots, source and result pointers occupy disjoint storage. publish and
 * acquire check the store, the published slot and their arguments for overlap.
 * Keep each live slot at one address. Keep the source context and record bytes
 * unchanged until the slot becomes FREE. The state machine is described in
 * snapshot.h. Certificate validation and trust authorization are caller
 * responsibilities. None of these functions charges work.
 *
 * prepare copies the source descriptor into a FREE slot and makes it
 * PREPARED. The slot borrows the context and record storage.
 * Returns OK. ARGUMENT for NULL arguments, overlap, or NULL callbacks with
 * nonzero counts. LIMIT for a slot outside FREE or with readers. The slot
 * changes only on OK. */
TC_TLV_result TC_X509_store_prepare(TC_X509_store_snapshot* slot,
                                    const TC_X509_store_source* source);
/* Return a PREPARED slot without readers to FREE and clear its source
 * descriptor. The caller owns the underlying record storage.
 * Returns OK, or ARGUMENT for a NULL slot or any other state, with the slot
 * unchanged. */
TC_TLV_result TC_X509_store_discard(TC_X509_store_snapshot* slot);
/* Make slot CURRENT. Authorize and persist changes before publication.
 * revision must equal store->revision, which then increments. The previous
 * slot becomes RETIRED while it has readers and FREE otherwise. Publishing
 * an empty source removes all anchors for later readers.
 * Returns OK. ARGUMENT for NULL arguments, overlap, a slot outside PREPARED
 * or with readers, or a current slot outside CURRENT. INVALID for a stale
 * revision. LIMIT for exhausted revision space. Failures leave the store and
 * both slots unchanged. */
TC_TLV_result TC_X509_store_publish(TC_X509_store* store, size_t revision,
                                    TC_X509_store_snapshot* slot);
/* Add a reader to the current snapshot and write it to out. Each successful
 * acquire needs one release, after all borrowed results expire. Old readers
 * keep the old trust configuration across publication. Applications that
 * need immediate distrust cancel or revalidate those operations.
 * Returns OK with out written. END when no snapshot is published. ARGUMENT
 * for NULL arguments, overlap or a current slot outside CURRENT. LIMIT when
 * the reader count would overflow. out changes only on OK. */
TC_TLV_result TC_X509_store_acquire(TC_X509_store* store, TC_X509_store_snapshot** out);
/* Release one acquired reference. The last reader of a RETIRED slot returns
 * it to FREE and clears its descriptor, so the caller can reclaim its context
 * and record storage.
 * Returns OK, or ARGUMENT for a NULL slot, no readers, or a slot outside
 * CURRENT and RETIRED, with the slot unchanged. */
TC_TLV_result TC_X509_store_release(TC_X509_store_snapshot* slot);
#endif
#ifdef __cplusplus
}
#endif
#endif
