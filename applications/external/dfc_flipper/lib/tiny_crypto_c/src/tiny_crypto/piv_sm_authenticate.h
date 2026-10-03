/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV secure-messaging response authentication: verifies the card CVC chain
 * under a validated content-signing certificate and completes key
 * confirmation in one call.
 * Standards: SP 800-73-5 Part 2 section 4.1.
 * Configuration: TC_ENABLE_PIV_SM, TC_ENABLE_PIV_CVC and TC_ENABLE_X509.
 * Contracts: docs/api.md, including its size_t work units.
 * Guide: docs/piv-sm.md. */
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

#if TC_ENABLE_PIV_SM
/* Authenticate the peer CVC under signer with TC_PIV_CVC_chain_verify, then
 * complete key confirmation with TC_PIV_SM_finish (SP 800-73-5 Part 2
 * section 4.1.1, steps H4 onward). signer must already satisfy path, usage,
 * policy, time and revocation checks, for example through
 * TC_PIV_CVC_validate. expected_uuid is empty or the 16-byte Card UUID.
 * - All inputs stay stable and disjoint from session, work and workspace.
 *   The workspace union serves the point check and then the session step,
 *   and is wiped before return.
 *
 * Work: the TC_PIV_CVC_chain_verify charges. Key confirmation charges no
 * work.
 * Returns VALID with the session READY for protected requests. ERROR for
 * NULL arguments, an empty peer certificate, an expected_uuid of another
 * nonzero length, a session outside ESTABLISHING or overlap, with the
 * session, work and workspace unchanged. The following end the session and
 * wipe the workspace. INVALID for a nonzero peer.card_control (step H4) or a
 * wrongly sized nonce or cryptogram, before any CVC work, and for a CVC chain
 * failure or a key-confirmation mismatch. UNSUPPORTED for an unknown session
 * suite or an unsupported chain. LIMIT for exhausted limits or work. ERROR
 * for a chain argument error, an invalid peer key or a KDF failure. */
TC_credential_status TC_PIV_SM_authenticate_response(TC_PIV_SM* session,
                                                     const TC_PIV_SM_authentication* authentication,
                                                     size_t* work,
                                                     TC_PIV_SM_authentication_workspace* workspace);
#endif

#ifdef __cplusplus
}
#endif
#endif
#endif
