/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_X509_PATH_H_
#define TINY_CRYPTO_X509_PATH_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/x509_store.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Caller-owned storage elements. Their fields are managed by the validator. */
typedef struct {
  TC_bytes oid;
  size_t depth;
  int alive, mapped;
} TC_X509_policy_node;
typedef struct { size_t parent, child; } TC_X509_policy_edge;
typedef struct { size_t node; TC_bytes oid; } TC_X509_policy_expected;

typedef struct {
  TC_TLV_frame* frames;
  size_t frame_capacity;
  /* Shared by extension decoding, policy decoding and EKU checks. */
  TC_bytes* oids;
  size_t oid_capacity;
  TC_X509_name_workspace names;
  TC_X509_policy_node* nodes;
  size_t node_capacity;
  TC_X509_policy_edge* edges;
  size_t edge_capacity;
  TC_X509_policy_expected* expected;
  size_t expected_capacity;
  TC_X509_policy_mapping* mappings;
  size_t mapping_capacity;
  TC_bytes* policies;
  size_t policy_capacity;
} TC_X509_path_workspace;

/* Array arguments only, not pointers. The shorter name buffer sets the limit.
 * Use as an initializer in C or C++: TC_X509_path_workspace w = ...; */
#define TC_X509_PATH_ARRAY_COUNT_(a) (sizeof(a) / sizeof((a)[0]))
#define TC_X509_PATH_WORKSPACE_INIT(frames_, oids_, left_, right_, matched_, nodes_, edges_, expected_, mappings_, policies_) \
  { (frames_), TC_X509_PATH_ARRAY_COUNT_(frames_), (oids_), TC_X509_PATH_ARRAY_COUNT_(oids_), \
    { (left_), (right_), \
      TC_X509_PATH_ARRAY_COUNT_(left_) < TC_X509_PATH_ARRAY_COUNT_(right_) \
        ? TC_X509_PATH_ARRAY_COUNT_(left_) : TC_X509_PATH_ARRAY_COUNT_(right_), \
      (matched_), TC_X509_PATH_ARRAY_COUNT_(matched_) }, \
    (nodes_), TC_X509_PATH_ARRAY_COUNT_(nodes_), (edges_), TC_X509_PATH_ARRAY_COUNT_(edges_), \
    (expected_), TC_X509_PATH_ARRAY_COUNT_(expected_), (mappings_), TC_X509_PATH_ARRAY_COUNT_(mappings_), \
    (policies_), TC_X509_PATH_ARRAY_COUNT_(policies_) }

enum {
  TC_X509_PATH_REQUIRE_EXPLICIT_POLICY = 1u,
  TC_X509_PATH_INHIBIT_MAPPING = 2u,
  TC_X509_PATH_INHIBIT_ANY_POLICY = 4u,
  TC_X509_PATH_REQUIRE_KEY_USAGE = 8u,
  TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE = 16u,
  /* Require an explicit purpose match when a certificate contains EKU. */
  TC_X509_PATH_INHIBIT_ANY_PURPOSE = 32u
};
typedef struct {
  TC_X509_time at;
  TC_TLV_limits parsing;
  size_t max_certificates, max_input, max_work;
  TC_X509_signature_provider signatures;
  const TC_bytes* initial_policies;
  size_t initial_policy_count;
  TC_X509_name_constraints anchor_names;
  TC_bytes purpose;
  uint16_t key_usage;
  unsigned flags;
} TC_X509_path_options;

typedef enum {
  TC_X509_PATH_VALID, TC_X509_PATH_INVALID, TC_X509_PATH_UNSUPPORTED,
  TC_X509_PATH_LIMIT, TC_X509_PATH_ERROR
} TC_X509_path_status;
typedef struct {
  TC_X509_public_key public_key;
  const TC_bytes* policies;
  size_t policy_count, work_used;
} TC_X509_path_result;

/* Search scratch fields are managed by the library. */
typedef struct {
  size_t candidate, anchor, bytes;
  TC_bytes issuer;
} TC_X509_search_frame;
typedef struct {
  TC_bytes* path;
  TC_X509_search_frame* frames;
  size_t capacity;
} TC_X509_search_workspace;
typedef struct {
  const TC_bytes* path;
  size_t count, anchor_index;
  TC_X509_path_result validation;
} TC_X509_search_result;

/* Construct and validate a path to an explicit source anchor. Search consumes
 * options.max_work across all branches; depth and total chain bytes use the same
 * limits as ordered validation. Source callbacks must keep returned records stable
 * and separate from both workspaces and out. Hold the source snapshot until all
 * result use finishes. No network fetching or revocation checking is performed.
 *
 * On VALID, path borrows a suffix of search.path, anchor-issued first, target last.
 * anchor_index identifies the selected source anchor. Key and policy lifetimes
 * match TC_X509_path_validate. Scratch may change on any result; out only on VALID.
 * Copy path and policy span arrays before reusing their workspaces. Copying the
 * result alone retains pointers into scratch; DER bytes need not be copied.
 * Workspace arrays must be mutually disjoint and separate from input storage. */
TC_X509_path_status TC_X509_path_build(TC_bytes target,
    const TC_X509_store_source* source, const TC_X509_path_options* options,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    TC_X509_search_result* out);

/* Anchor-issued certificate first, target last; the anchor is not in chain.
 * Checks signatures, time, CA/usage, names, policies and critical extensions.
 * Does not discover paths or check revocation. No native verifier is selected
 * implicitly: options.signatures must provide one for the algorithms in use.
 *
 * DER and option spans are borrowed and must remain unchanged during the call.
 * Result key bytes borrow target DER; policy spans also borrow issuer DER or
 * initial_policies. Keep those buffers and workspace.policies alive while used.
 * Workspace arrays must be mutually disjoint and disjoint from inputs/out.
 * Provider context must also be separate from workspace and out.
 * Workspace/provider state may change on any result; out changes only on VALID. */
TC_X509_path_status TC_X509_path_validate(const TC_bytes* chain, size_t count,
    const TC_X509_trust_anchor* anchor, const TC_X509_path_options* options,
    const TC_X509_path_workspace* workspace, TC_X509_path_result* out);
#ifdef __cplusplus
}
#endif
#endif
