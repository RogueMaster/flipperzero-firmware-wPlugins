/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Per-call state shared by the PIV and TWIC credential validators: path and
 * revocation policy from the context, the card profile, the trust source
 * guarded against every write range, and the revocation evidence of the last
 * signature check. */
#ifndef TC_CREDENTIAL_SESSION_INTERNAL_H_
#define TC_CREDENTIAL_SESSION_INTERNAL_H_

#include <tiny_crypto/credential.h>
#include "cms_internal.h"
#include "pki_source_internal.h"
#include "validation_internal.h"

enum { TC_CREDENTIAL_SESSION_WRITES = TC_VALIDATION_WRITES + 1 };

/* The guard and source refer to this struct, so it stays in place for the
 * whole call. revocation_checked is set by tc_credential_session_verify on
 * VALID. */
typedef struct {
  TC_CMS_path_options policy;
  TC_X509_path_options crl_policy;
  TC_CMS_revocation_policy revocation;
  int piv;
  TC_PIV_oid_profile oids;
  TC_bytes writes[TC_CREDENTIAL_SESSION_WRITES];
  tc_pki_source_guard guard;
  TC_X509_store_source source;
  uint8_t revocation_checked;
} tc_credential_session;

/* Caller buffers checked by tc_credential_session_bind. scratch is an
 * optional extra write range. objects adds each inventory part as an
 * input. */
typedef struct {
  const TC_bytes* inputs;
  size_t input_count;
  void* out;
  size_t out_size;
  uint8_t* scratch;
  size_t scratch_size;
  const TC_PIV_security_data* objects;
  size_t object_count;
} tc_credential_storage;

/* Resolve policies and the card profile. Returns 0 for an incomplete context,
 * an unknown profile or a purpose the profile does not accept. */
int tc_credential_session_open(tc_credential_session* session, const TC_validation_context* context,
                               TC_PIV_card_profile profile);

/* Check every write range against the others and every input against the
 * writes, commit the preflight work, then guard the trust source. */
TC_TLV_result tc_credential_session_bind(tc_credential_session* session,
                                         const TC_validation_context* context,
                                         const tc_credential_storage* storage, size_t* work);

/* Charge the encoded object against the input limit and work. */
TC_TLV_result tc_credential_session_input(const tc_credential_session* session, TC_bytes encoded,
                                          size_t* work);

/* Verify the CMS signature, the signer path and its revocation with the
 * prepared envelope under the context's evidence policy. */
TC_credential_status tc_credential_session_verify(tc_credential_session* session,
                                                  const TC_validation_context* context,
                                                  const TC_CMS_validation_request* cms,
                                                  const TC_PIV_CMS_object* object, size_t* work);

/* A dependent object binds to a result accepted under the same card profile
 * at the context's evaluation time. */
int tc_credential_result_current(TC_PIV_card_profile result_profile, const TC_X509_time* result_at,
                                 TC_PIV_card_profile profile, const TC_validation_context* context);

/* An accepted CHUID result: a 25-byte FASC-N, a 16-byte GUID, a signer and
 * the same profile and evaluation time. */
int tc_credential_chuid_bound(const TC_PIV_CHUID_report* chuid, TC_PIV_card_profile profile,
                              const TC_validation_context* context);

/* The CMS identifier set that matches a card's PIV OID profile. */
static inline TC_CMS_attribute_oids tc_credential_attribute_oids(TC_PIV_oid_profile oids)
{
  return oids == TC_PIV_OIDS_ONLY ? TC_CMS_ATTRIBUTE_OIDS_PIV : TC_CMS_ATTRIBUTE_OIDS_PIV_TWIC;
}

static inline const TC_X509_path_workspace*
tc_credential_scratch(const TC_validation_context* context)
{
  return &context->workspace->path->validation;
}

static inline TC_TLV_frames tc_credential_frames(const TC_validation_context* context)
{
  return tc_credential_scratch(context)->frames;
}

#endif
