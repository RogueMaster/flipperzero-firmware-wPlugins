/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_X509_CRL_SOURCE_H_
#define TINY_CRYPTO_X509_CRL_SOURCE_H_
#include <tiny_crypto/source.h>
#include <tiny_crypto/x509_crl.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct TC_X509_crl_job TC_X509_crl_job;
/* Alignment for statically allocated job storage. */
typedef union { uint64_t integer; void* pointer; long double real; } TC_X509_crl_storage;
typedef struct {
  TC_TLV_limits parsing;
  uint64_t max_input, max_read_bytes, max_reads, max_entries;
} TC_X509_crl_prepare_options;
typedef struct {
  TC_buffer state, window, metadata, entry, issuer;
  TC_X509_workspace parsing;
  TC_X509_name_workspace names;
  TC_X509_crl_match* matches;
  size_t match_capacity;
} TC_X509_crl_prepare_workspace;

/* Size in bytes and alignment required for workspace.state. */
size_t TC_X509_crl_prepare_size(void);
size_t TC_X509_crl_prepare_alignment(void);

/* Start a CRL scan for the supplied issuer/serial targets.
 * Hold the source immutable until preparation finishes. Retain state, metadata,
 * targets, their issuer/serial bytes and matches until the last record use.
 * Other scratch can be reused after completion. All regions are disjoint.
 * parsing limits bound individual metadata/entry objects; max_input bounds the
 * complete CRL. Work and scratch may change on failure; out changes only on OK. */
TC_TLV_result TC_X509_crl_prepare_begin(const TC_source* source,
    const TC_X509_crl_target* targets, size_t count,
    const TC_X509_crl_prepare_options* options,
    const TC_X509_crl_prepare_workspace* workspace, size_t* work, TC_X509_crl_job** out);
/* Each call processes at most max_entries records and max_bytes hash input.
 * Both limits must be nonzero. OK sets complete to 0 for more work or 1 when
 * finish can return the prepared record. Failure leaves complete unchanged.
 * Refill the per-call work budget between calls. The source I/O budget spans
 * the entire job. Any processing failure requires a new job. */
TC_TLV_result TC_X509_crl_prepare_step(TC_X509_crl_job* job, size_t max_entries,
    size_t max_bytes, size_t* work, int* complete);
/* Return a record suitable for TC_X509_crl_index after the complete scan/hash.
 * The resolver verifies its signature and applies trust, scope and time policy.
 * Unqueried targets return UNSUPPORTED during lookup. out borrows job storage
 * and changes only on OK. Calling finish before completion returns ARGUMENT. */
TC_TLV_result TC_X509_crl_prepare_finish(const TC_X509_crl_job* job, TC_X509_crl_record* out);
/* Invalidate the job and every record borrowed from it. */
void TC_X509_crl_prepare_clear(TC_X509_crl_job* job);

#ifdef __cplusplus
}
#endif
#endif
