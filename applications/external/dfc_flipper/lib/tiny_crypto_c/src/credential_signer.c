/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Content signer validation and the secure-messaging CVC chain built on it.
 * The signer certificate gets the content-signing policy of the card
 * profile, then TC_X509_validate checks its path and revocation. */
#include <tiny_crypto/credential.h>

#if TC_ENABLE_CREDENTIAL
#include "internal.h"
#include "credential_session_internal.h"
#include "credential_status_internal.h"
#include "credential_policy_internal.h"

/* Apply the content-signer policy of the session profile to certificate and
 * validate it under options, a copy of the context options that receives
 * the policy. Call after tc_credential_session_bind. */
static TC_credential_status signer_validate(TC_bytes certificate, tc_credential_session* session,
                                            const TC_validation_context* context,
                                            TC_validation_options* options, size_t* work,
                                            TC_X509_validation_report* out)
{
  TC_X509_certificate parsed;
  TC_TLV_result checked = tc_credential_signer_read(certificate, &session->policy.path.parsing,
                                                    tc_credential_scratch(context), work, &parsed);
  if (checked == TC_TLV_OK)
    checked =
        tc_credential_signer_policy(&parsed, session->piv, !session->piv, NULL,
                                    &session->policy.path, tc_credential_scratch(context), work);
  if (checked != TC_TLV_OK)
    return tc_validation_status(checked);
  const TC_X509_path_options* path = &session->policy.path;
  *options = *context->options;
  options->certificate =
      (TC_validation_certificate_policy){path->initial_policies, path->initial_policy_count,
                                         path->anchor_names,     path->purpose,
                                         path->key_usage,        path->flags};
  TC_validation_context signer_context = *context;
  signer_context.options = options;
  signer_context.trust.certificates = &session->source;
  return TC_X509_validate(certificate, &signer_context, work, out);
}

TC_credential_status TC_PIV_content_signer_validate(TC_bytes certificate,
                                                    TC_PIV_card_profile profile,
                                                    const TC_validation_context* context,
                                                    size_t* work, TC_X509_validation_report* out)
{
  tc_credential_session session;
  if (!certificate.data || !certificate.length || !work || !out ||
      !tc_credential_session_open(&session, context, profile))
    return TC_CREDENTIAL_ERROR;
  const tc_credential_storage storage = {&certificate, 1, out, sizeof *out, NULL, 0, NULL, 0};
  TC_TLV_result checked = tc_credential_session_bind(&session, context, &storage, work);
  if (checked != TC_TLV_OK)
    return tc_validation_status(checked);
  TC_validation_options options;
  return signer_validate(certificate, &session, context, &options, work, out);
}

#if TC_ENABLE_PIV_CVC
TC_credential_status TC_PIV_CVC_validate(const TC_PIV_CVC_validation_request* request,
                                         const TC_validation_context* context,
                                         TC_EC_workspace* point, size_t* work, TC_PIV_CVC* out)
{
  tc_credential_session session;
  if (!request || !request->card.data || !request->card.length ||
      !request->signer_certificate.data || !request->signer_certificate.length || !point || !work ||
      !out || !tc_credential_session_open(&session, context, request->profile))
    return TC_CREDENTIAL_ERROR;
  const TC_bytes inputs[] = {request->card,
                             request->intermediate,
                             request->expected_uuid,
                             request->signer_certificate,
                             {(const uint8_t*)request, sizeof *request}};
  const tc_credential_storage storage = {
      inputs, sizeof inputs / sizeof *inputs, out, sizeof *out, NULL, 0, NULL, 0};
  TC_TLV_result checked = tc_credential_session_bind(&session, context, &storage, work);
  if (checked != TC_TLV_OK)
    return tc_validation_status(checked);
  TC_validation_options options;
  TC_X509_validation_report signer;
  TC_credential_status status =
      signer_validate(request->signer_certificate, &session, context, &options, work, &signer);
  if (status != TC_CREDENTIAL_VALID)
    return status;
  const TC_PIV_CVC_chain_request chain = {request->card, request->intermediate,
                                          request->expected_uuid, request->curve,
                                          &signer.certificate};
  return tc_credential_signature_status(
      TC_PIV_CVC_chain_verify(&chain, &options.parsing, &options.signatures, point, work, out));
}
#endif
#endif
