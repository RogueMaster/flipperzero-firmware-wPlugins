/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_X509_REVOCATION_H_
#define TINY_CRYPTO_X509_REVOCATION_H_
#include <tiny_crypto/x509_crl.h>
#include <tiny_crypto/x509_path.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_X509_CRL_UNDETERMINED, TC_X509_CRL_UNREVOKED, TC_X509_CRL_REVOKED
} TC_X509_revocation_status;
/* ReasonFlags bits 1..8; bit 0 is unused. */
enum { TC_X509_CRL_ALL_REASONS = 0x1fe };
typedef struct {
  uint16_t reasons;
  TC_X509_crl_match revocation;
} TC_X509_crl_evidence;
/* Provisional workspace entries; fields are managed by the resolver. */
typedef struct {
  TC_bytes certificate;
  TC_X509_revocation_status status;
} TC_X509_revocation_node;
typedef enum {
  TC_X509_CRL_COMPLETE_ONLY, TC_X509_CRL_DELTA_IF_AVAILABLE, TC_X509_CRL_DELTA_REQUIRED
} TC_X509_crl_delta_policy;
typedef enum {
  TC_X509_CRL_ORDER_NUMBER, TC_X509_CRL_ORDER_THIS_UPDATE
} TC_X509_crl_order_policy;
typedef struct {
  const TC_X509_crl_index* index;
  const TC_X509_store_source* source;
  const TC_X509_path_options* signer_policy;
  size_t anchor_index, max_candidate_bytes;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
} TC_X509_revocation_options;
typedef struct {
  const TC_X509_path_workspace* validation;
  const TC_X509_search_workspace* search;
  uint8_t* states;
  size_t state_capacity;
  TC_X509_revocation_node* nodes;
  size_t node_capacity;
} TC_X509_revocation_workspace;
typedef struct {
  TC_X509_revocation_status status;
  size_t certificate_index;
  TC_X509_crl_evidence evidence;
} TC_X509_revocation_result;

/* Check a previously validated path, anchor-issued first, anchor excluded.
 * Keep the selected anchor, time, CRL index and source snapshot fixed. Source
 * candidates supply CRL signers and their paths; signer_policy is distinct from
 * the holder's purpose/usage policy. max_candidate_bytes bounds their collection.
 * states needs one byte per indexed CRL; nodes covers the path and distinct
 * signer dependencies. Verified nodes are reused only within this call.
 * Input/metadata, workspace arrays, work and out must be disjoint. Save path
 * spans outside search scratch before calling. Work and scratch are provisional.
 * OK means a determined result: callers must inspect status. REVOKED includes
 * the member index and evidence; UNREVOKED uses SIZE_MAX and zero evidence.
 * Missing evidence/cycles return UNSUPPORTED; failures leave out unchanged. */
TC_TLV_result TC_X509_path_check_revocation(const TC_bytes* chain, size_t count,
    const TC_X509_revocation_options* options, const TC_X509_revocation_workspace* workspace,
    size_t* work, TC_X509_revocation_result* out);

#ifdef __cplusplus
}
#endif
#endif
