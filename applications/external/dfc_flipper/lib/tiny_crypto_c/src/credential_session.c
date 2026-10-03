/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Shared validator session: context policies, storage preflight, the
 * guarded trust source and the signed-object verification step. */
#include <tiny_crypto/credential.h>

#if TC_ENABLE_CREDENTIAL
#include "internal.h"
#include "credential_session_internal.h"
#include "credential_policy_internal.h"
#include "pki_budget_internal.h"

int tc_credential_session_open(tc_credential_session* session, const TC_validation_context* context,
                               TC_PIV_card_profile profile)
{
  session->revocation_checked = 0;
  return tc_validation_policies(context, &session->policy, &session->crl_policy,
                                &session->revocation) &&
         tc_credential_profile(profile, context->options, &session->piv, &session->oids);
}

TC_TLV_result tc_credential_session_bind(tc_credential_session* session,
                                         const TC_validation_context* context,
                                         const tc_credential_storage* storage, size_t* work)
{
  tc_pki_storage_plan plan;
  tc_pki_storage_plan_begin(&plan, session->writes, TC_CREDENTIAL_SESSION_WRITES, *work);
  tc_validation_plan_writes(&plan, context, work, storage->out, storage->out_size);
  if (storage->scratch)
    tc_pki_storage_plan_write(&plan, storage->scratch, storage->scratch_size, 1);
  tc_pki_storage_plan_seal(&plan);
  tc_validation_plan_inputs(&plan, context, storage->inputs, storage->input_count);
  if (storage->objects)
    TC_PKI_PLAN_INPUT(&plan, storage->objects, storage->object_count);
  for (size_t i = 0; plan.status == TC_TLV_OK && i < storage->object_count; ++i) {
    TC_PKI_PLAN_INPUT(&plan, storage->objects[i].parts, storage->objects[i].count);
    tc_pki_storage_plan_input_spans(&plan, storage->objects[i].parts, storage->objects[i].count);
  }
  TC_TLV_result result = tc_pki_storage_plan_finish(&plan, work);
  if (result != TC_TLV_OK)
    return result;
  session->guard = (tc_pki_source_guard){context->trust.certificates, session->writes, plan.count};
  session->source = tc_pki_source_guard_bind(&session->guard);
  return TC_TLV_OK;
}

TC_TLV_result tc_credential_session_input(const tc_credential_session* session, TC_bytes encoded,
                                          size_t* work)
{
  if (encoded.length > session->policy.path.parsing.max_input)
    return TC_TLV_LIMIT;
  return tc_pki_work_charge(work, encoded.length);
}

TC_credential_status tc_credential_session_verify(tc_credential_session* session,
                                                  const TC_validation_context* context,
                                                  const TC_CMS_validation_request* cms,
                                                  const TC_PIV_CMS_object* object, size_t* work)
{
  const tc_cms_prepared_signed_data prepared = {&object->envelope, &object->signer};
  const tc_cms_validation_extras extras = {
      NULL, 0, &prepared, tc_validation_evidence(context, &session->revocation_checked)};
  return tc_cms_credential_validate_internal(cms, &session->source, &session->policy,
                                             &session->revocation, context->workspace, work,
                                             &extras);
}

int tc_credential_result_current(TC_PIV_card_profile result_profile, const TC_X509_time* result_at,
                                 TC_PIV_card_profile profile, const TC_validation_context* context)
{
  int order;
  return result_profile == profile &&
         TC_X509_time_compare(&context->options->at, result_at, &order) == TC_TLV_OK && !order;
}

int tc_credential_chuid_bound(const TC_PIV_CHUID_report* chuid, TC_PIV_card_profile profile,
                              const TC_validation_context* context)
{
  enum { FASCN_BYTES = 25, GUID_BYTES = 16 };
  return chuid && chuid->object.fascn.data && chuid->object.fascn.length == FASCN_BYTES &&
         chuid->object.card_uuid.data && chuid->object.card_uuid.length == GUID_BYTES &&
         chuid->signer.data && chuid->signer.length &&
         tc_credential_result_current(chuid->profile, &chuid->at, profile, context);
}

#endif
