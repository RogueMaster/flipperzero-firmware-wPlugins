/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Stepwise CRL preparation from a byte source: bounded reads, parsing and a
 * prepared lookup structure.
 * Standards: RFC 5280 section 5.
 * Configuration: TC_ENABLE_X509_REVOCATION.
 * Contracts: docs/api.md. Guide: docs/x509-crl.md. */
#ifndef TINY_CRYPTO_X509_CRL_SOURCE_H_
#define TINY_CRYPTO_X509_CRL_SOURCE_H_
#include <tiny_crypto/source.h>
#include <tiny_crypto/x509_crl.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct TC_X509_crl_job TC_X509_crl_job;
/* Alignment for statically allocated job storage. */
typedef union {
  uint64_t integer;
  void* pointer;
  long double real;
} TC_X509_crl_storage;
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

#if TC_ENABLE_X509_REVOCATION
/* Bytes and alignment that workspace.state needs for one job. An array of
 * TC_X509_crl_storage meets the alignment. Both charge no work. */
size_t TC_X509_crl_prepare_size(void);
size_t TC_X509_crl_prepare_alignment(void);

/* Start a CRL job over a byte source for the supplied issuer and serial
 * targets (RFC 5280 section 5). begin reads the CRL layout, its metadata and
 * extensions, then prepares the entry scan and the TBSCertList hash.
 * - Hold the source immutable until preparation finishes. Keep state,
 *   metadata, the targets, their issuer and serial bytes and matches until
 *   the last use of the record. Other scratch can be reused after
 *   completion. All regions are disjoint.
 * - workspace.state holds TC_X509_crl_prepare_size bytes at
 *   TC_X509_crl_prepare_alignment. window and metadata are nonempty.
 *   matches holds one slot per target.
 * - options->parsing bounds each metadata or entry object. max_input bounds
 *   the complete CRL. max_read_bytes and max_reads bound the source I/O of
 *   the whole job, and max_entries bounds the entries scanned.
 * - A CRL whose extension policy is INVALID or UNSUPPORTED keeps that policy
 *   in the finished record, with no entry scan and zero matches. The resolver
 *   skips that record.
 *
 * Work: one unit per storage comparison, then the bytes of the metadata and
 * extension passes.
 * Returns OK with out pointing at the job in workspace.state. ARGUMENT for
 * NULL arguments, an empty window or metadata buffer, misaligned state, a
 * target with an empty serial or issuer, or overlap, with all state
 * unchanged. LIMIT for short state or match capacity or a source above
 * max_input, before any work, and for exhausted limits, I/O budgets or work
 * later. IO for a failed source read. INVALID and UNSUPPORTED for a CRL that
 * the metadata or signature algorithm checks reject. out changes only on OK.
 * Work and scratch may change on failure. */
TC_TLV_result TC_X509_crl_prepare_begin(const TC_source* source, const TC_X509_crl_target* targets,
                                        size_t count, const TC_X509_crl_prepare_options* options,
                                        const TC_X509_crl_prepare_workspace* workspace,
                                        size_t* work, TC_X509_crl_job** out);
/* Advance a job by at most max_entries entries and max_bytes hash input.
 * Both limits are nonzero. Refill the per-call work budget between calls.
 * The source I/O budgets span the whole job.
 *
 * Work: one unit per storage comparison, the entry passes and the hashed
 * bytes.
 * Returns OK and writes complete as 1 when TC_X509_crl_prepare_finish can
 * return the record and 0 when more steps are needed. ARGUMENT for NULL
 * arguments, a zero limit, a job outside the scanning and hashing phases, or
 * overlap, with the job unchanged. LIMIT when work cannot cover the storage
 * comparisons, with the job and work unchanged. LIMIT, IO, INVALID and
 * UNSUPPORTED for exhausted budgets, failed reads and rejected entries during
 * the step. Those step failures mark the job failed and wipe its hash state,
 * and a new job is needed. complete changes only on OK. */
TC_TLV_result TC_X509_crl_prepare_step(TC_X509_crl_job* job, size_t max_entries, size_t max_bytes,
                                       size_t* work, int* complete);
/* Write the prepared record of a complete job for use in a
 * TC_X509_crl_index. The record borrows job storage. The resolver verifies
 * its signature and applies trust, scope and time policy. Lookups for targets
 * outside the job return UNSUPPORTED. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL arguments, an incomplete
 * job or overlap. out changes only on OK. */
TC_TLV_result TC_X509_crl_prepare_finish(const TC_X509_crl_job* job, TC_X509_crl_record* out);
/* Wipe the job and invalidate every record borrowed from it. Accepts NULL. */
void TC_X509_crl_prepare_clear(TC_X509_crl_job* job);
#endif

#ifdef __cplusplus
}
#endif
#endif
