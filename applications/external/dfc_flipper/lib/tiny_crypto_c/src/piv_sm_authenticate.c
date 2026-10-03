/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_sm_internal.h"
#include "credential_status_internal.h"
#include <tiny_crypto/piv_sm_authenticate.h>

#if TC_ENABLE_PIV_SM && TC_ENABLE_X509 && TC_ENABLE_PIV_CVC
TC_credential_status TC_PIV_SM_authenticate_response(TC_PIV_SM* session,
                                                     const TC_PIV_SM_authentication* authentication,
                                                     size_t* work,
                                                     TC_PIV_SM_authentication_workspace* workspace)
{
  if (!session || !authentication || !authentication->signer || !authentication->limits ||
      !authentication->signatures || !work || !workspace ||
      !authentication->peer.certificate.data || !authentication->peer.certificate.length ||
      /* The chain check takes an absent or 16-byte card UUID. Reject other
       * lengths here so the argument error keeps the session. */
      (authentication->expected_uuid.length && authentication->expected_uuid.length != 16) ||
      session->state != TC_PIV_SM_ESTABLISHING)
    return TC_CREDENTIAL_ERROR;
  {
    const TC_bytes writable[] = {{(const uint8_t*)session, sizeof *session},
                                 {(const uint8_t*)work, sizeof *work},
                                 {(const uint8_t*)workspace, sizeof *workspace}};
    const TC_X509_public_key* key = &authentication->signer->public_key;
    const TC_bytes input[] = {
        {(const uint8_t*)authentication, sizeof *authentication},
        authentication->peer.certificate,
        authentication->peer.nonce,
        authentication->peer.cryptogram,
        authentication->intermediate,
        authentication->expected_uuid,
        {(const uint8_t*)authentication->signer, sizeof *authentication->signer},
        {(const uint8_t*)authentication->limits, sizeof *authentication->limits},
        {(const uint8_t*)authentication->signatures, sizeof *authentication->signatures},
        authentication->signer->encoded,
        authentication->signer->extensions,
        key->algorithm.oid,
        key->algorithm.parameters,
        key->key,
        key->modulus,
        key->exponent,
        key->curve_oid};
    if (!tc_sm_disjoint(writable, 3, input, sizeof input / sizeof *input))
      return TC_CREDENTIAL_ERROR;
  }

  const tc_sm_suite* settings = tc_sm_suite_get(session->suite);
  if (!settings) {
    TC_PIV_SM_clear(session);
    TC_secure_zero(workspace, sizeof *workspace);
    return TC_CREDENTIAL_UNSUPPORTED;
  }
  /* SP 800-73-5 Part 2 section 4.1 checks CB_ICC (step H4) before the CVC
   * (step H5). */
  if (authentication->peer.card_control != 0 ||
      authentication->peer.nonce.length != settings->nonce_bytes ||
      authentication->peer.cryptogram.length != 16) {
    TC_PIV_SM_clear(session);
    TC_secure_zero(workspace, sizeof *workspace);
    return TC_CREDENTIAL_INVALID;
  }
  const TC_PIV_CVC_chain_request chain = {
      authentication->peer.certificate, authentication->intermediate, authentication->expected_uuid,
      settings->curve, authentication->signer};
  TC_PIV_CVC verified;
  TC_X509_signature_result checked =
      TC_PIV_CVC_chain_verify(&chain, authentication->limits, authentication->signatures,
                              &workspace->point, work, &verified);
  TC_credential_status result = tc_credential_signature_status(checked);
  if (result == TC_CREDENTIAL_VALID) {
    TC_status finished =
        TC_PIV_SM_finish(session, &authentication->peer, verified.public_key, &workspace->session);
    result = finished == TC_OK         ? TC_CREDENTIAL_VALID
             : finished == TC_MISMATCH ? TC_CREDENTIAL_INVALID
                                       : TC_CREDENTIAL_ERROR;
  } else {
    TC_PIV_SM_clear(session);
    TC_secure_zero(workspace, sizeof *workspace);
  }
  return result;
}
#endif
