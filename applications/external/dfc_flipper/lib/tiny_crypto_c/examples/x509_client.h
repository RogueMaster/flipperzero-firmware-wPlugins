/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_X509_CLIENT_H_
#define EXAMPLE_X509_CLIENT_H_
#include "x509_workspace.h"

/* DER order is anchor-issued first, client last. Result spans borrow DER/storage. */
#ifdef __cplusplus
extern "C" {
#endif
TC_X509_path_status example_check_client_certificate(const TC_bytes* chain, size_t count,
    const TC_X509_trust_anchor* anchor, const TC_X509_time* at,
    const TC_X509_signature_provider* verifier, size_t work_limit,
    ExampleX509Workspace* storage, TC_X509_path_result* result);
/* Hold the source snapshot and storage until all result use finishes. */
TC_X509_path_status example_find_client_path(TC_bytes target,
    const TC_X509_store_source* source, const TC_X509_time* at,
    const TC_X509_signature_provider* verifier, size_t work_limit,
    ExampleX509SearchWorkspace* storage, TC_X509_search_result* result);
#ifdef __cplusplus
}
#endif
#endif
