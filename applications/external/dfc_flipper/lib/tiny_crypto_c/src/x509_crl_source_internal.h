/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_X509_CRL_SOURCE_INTERNAL_H_
#define TINY_CRYPTO_X509_CRL_SOURCE_INTERNAL_H_
#include "source_der_internal.h"
#include "x509_crl_internal.h"

typedef struct { uint64_t offset, length; } tc_source_span;

typedef struct {
  tc_source_span tbs, algorithm, signature;
  tc_source_span version, inner_algorithm, issuer, this_update, next_update;
  tc_source_span revoked, extensions;
} tc_x509_crl_layout;

/* Locate encoded fields without reading the revoked entries. Optional absent
 * fields have zero length. Typed field checks and authentication follow before
 * this provisional layout can contribute to a revocation result. */
TC_TLV_result tc_x509_crl_source_layout(tc_source_reader* reader,
    tc_x509_crl_layout* out);

/* Copy bounded metadata into stable caller storage, then apply shared typed
 * checks. Layout comes from this immutable source. All writable storage and
 * inputs are disjoint. Scratch is provisional; out changes only on success.
 * encoded/tbs/revoked remain empty; their source offsets stay in layout. */
TC_TLV_result tc_x509_crl_source_metadata(tc_source_reader* reader,
    const tc_x509_crl_layout* layout, TC_buffer storage,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree, tc_x509_crl* out);

typedef struct {
  uint64_t cursor, end, remaining;
  unsigned version;
} tc_x509_crl_source_entries;

TC_TLV_result tc_x509_crl_source_entries_init(tc_source_reader* reader,
    tc_source_span encoded, unsigned version, uint64_t max_entries,
    tc_x509_crl_source_entries* out);
/* Borrow an entry from the read window when it fits; copy fragmented entries to
 * scratch. Returned spans last until the next reader/scratch operation. Cursor
 * and out change only on success; scratch and I/O budgets are provisional.
 * Entry extension policy and indirect issuer inheritance are separate steps. */
TC_TLV_result tc_x509_crl_source_entry_next(tc_source_reader* reader,
    tc_x509_crl_source_entries* entries, TC_buffer scratch,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    tc_x509_crl_entry* out);

typedef struct {
  tc_x509_crl_source_entries entries;
  const tc_x509_crl_extension_info* extensions;
  tc_x509_crl_entry_issuer issuer;
  TC_buffer issuer_storage;
} tc_x509_crl_source_revoked;

/* Metadata and extensions stay alive and unchanged during iteration. Separate
 * issuer storage retains GeneralNames across read-window and entry-scratch reuse.
 * All input, state, output and scratch regions are disjoint. */
TC_TLV_result tc_x509_crl_source_revoked_init(tc_source_reader* reader,
    tc_source_span encoded, const tc_x509_crl* metadata,
    const tc_x509_crl_extension_info* extensions, uint64_t max_entries,
    TC_buffer issuer_storage, tc_x509_crl_source_revoked* out);
TC_TLV_result tc_x509_crl_source_revoked_next(tc_source_reader* reader,
    tc_x509_crl_source_revoked* revoked, TC_buffer scratch,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_x509_crl_revoked_entry* out);

typedef enum { TC_CRL_SCAN_ACTIVE, TC_CRL_SCAN_COMPLETE, TC_CRL_SCAN_FAILED } tc_crl_scan_phase;
typedef struct {
  tc_source_reader* reader;
  tc_x509_crl_source_revoked revoked;
  const tc_x509_crl_serial_query* queries;
  tc_x509_crl_match* matches;
  size_t count;
  tc_crl_scan_phase phase;
} tc_x509_crl_source_scan;

/* Start from a freshly initialized iterator. Queries and metadata remain stable;
 * matches is provisional caller scratch. All writable/input storage is disjoint.
 * An empty query batch still validates every entry. */
TC_TLV_result tc_x509_crl_source_scan_init(tc_source_reader* reader,
    const tc_x509_crl_source_revoked* revoked, const tc_x509_crl_serial_query* queries,
    size_t count, tc_x509_crl_match* matches, size_t capacity, tc_x509_crl_source_scan* out);
/* Process at most max_entries; work and I/O limits bound each call further.
 * Failure makes the scan terminal. complete changes only on success. */
TC_TLV_result tc_x509_crl_source_scan_step(tc_x509_crl_source_scan* scan,
    size_t max_entries, TC_buffer scratch, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t capacity, int* complete);
/* Copy matches after the complete entry scan. Signature, signer trust, freshness
 * and CRL applicability must also succeed before a credential verdict is issued. */
TC_TLV_result tc_x509_crl_source_scan_finish(const tc_x509_crl_source_scan* scan,
    tc_x509_crl_match* out, size_t capacity);
#endif
