/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_SM_AUTHENTICATE_H_
#define TINY_CRYPTO_PIV_SM_AUTHENTICATE_H_
#include <tiny_crypto/piv_cvc.h>
#include <tiny_crypto/piv_sm.h>

#if TC_ENABLE_X509 && TC_ENABLE_PIV_CVC
typedef struct {
  TC_PIV_SM_peer peer;
  TC_bytes intermediate, expected_uuid;
  /* Content-signing certificate accepted by path and revocation validation. */
  const TC_X509_certificate* signer;
  const TC_TLV_limits* limits;
  const TC_X509_signature_provider* signatures;
} TC_PIV_SM_authentication;

/* CVC point validation and session establishment have separate lifetimes. */
typedef union {
  TC_EC_workspace point;
  TC_PIV_SM_workspace session;
} TC_PIV_SM_authentication_workspace;

#ifdef __cplusplus
extern "C" {
#endif

/* Authenticate the peer CVC under signer, then complete key confirmation.
 * signer must already satisfy path, usage, policy, time and revocation checks.
 * Any peer or cryptographic failure clears the establishing session. Argument
 * and overlap errors leave it unchanged. Processing consumes work and clears
 * workspace. VALID leaves the session ready for protected requests. */
TC_credential_status TC_PIV_SM_authenticate_response(TC_PIV_SM* session,
    const TC_PIV_SM_authentication* authentication, size_t* work,
    TC_PIV_SM_authentication_workspace* workspace);

#ifdef __cplusplus
}
#endif
#endif
#endif
