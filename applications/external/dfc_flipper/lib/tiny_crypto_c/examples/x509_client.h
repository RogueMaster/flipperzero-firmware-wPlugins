/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_X509_CLIENT_H_
#define EXAMPLE_X509_CLIENT_H_
#include <tiny_crypto/x509_path.h>

/* Client paths hold up to four certificates. Size a search workspace with
 * this many path spans and frames. */
enum { EXAMPLE_CLIENT_PATH_CAPACITY = 4 };

#ifdef __cplusplus
extern "C" {
#endif
/* Write the arena bytes the client examples need. Check a static arena
 * against it once at startup. The arena needs
 * TC_X509_path_workspace_alignment, which an array of TC_X509_path_storage
 * meets. Returns the TC_X509_path_workspace_size status. */
TC_result example_client_workspace_size(size_t* bytes);
/* DER order is anchor-issued first, client last. The validation workspace is
 * laid out in arena for this call. Result spans borrow the DER and arena, so
 * keep both alive and the arena unused until result use finishes. LIMIT for
 * an arena shorter than example_client_workspace_size reports. ERROR for a
 * NULL or misaligned arena. */
TC_X509_path_status example_check_client_certificate(const TC_bytes* chain, size_t count,
                                                     const TC_X509_trust_anchor* anchor,
                                                     const TC_X509_time* at,
                                                     const TC_X509_signature_provider* verifier,
                                                     size_t work_limit, TC_buffer arena,
                                                     TC_X509_path_report* result);
/* Discover and validate a client path from source. search holds the path
 * spans and is disjoint from arena. Hold the source snapshot, arena and
 * search storage until all result use finishes. Arena statuses match
 * example_check_client_certificate. */
TC_X509_path_status example_find_client_path(TC_bytes target, const TC_X509_store_source* source,
                                             const TC_X509_time* at,
                                             const TC_X509_signature_provider* verifier,
                                             size_t work_limit, TC_buffer arena,
                                             const TC_X509_search_workspace* search,
                                             TC_X509_search_report* result);
#ifdef __cplusplus
}
#endif
#endif
