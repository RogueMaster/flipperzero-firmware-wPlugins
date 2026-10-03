/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The PIVSM session class for piv_sm.h and piv_sm_authenticate.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_SM_HPP_
#define TINY_CRYPTO_PIV_SM_HPP_

#ifndef __cplusplus
#error Do not include piv_sm.hpp in a C project, include piv_sm.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/piv_sm.h>
#if TC_ENABLE_X509 && TC_ENABLE_PIV_CVC
#include <tiny_crypto/piv_sm_authenticate.h>
#endif
#if TC_ENABLE_PIV_SM

namespace tiny_crypto {
typedef ::TC_PIV_SM_workspace piv_sm_workspace;
typedef ::TC_PIV_SM_handshake piv_sm_handshake;
typedef ::TC_PIV_SM_peer piv_sm_peer;
typedef ::TC_PIV_SM_protect_request piv_sm_protect_request;
typedef ::TC_PIV_SM_unprotect_request piv_sm_unprotect_request;
typedef ::TC_PIV_SM_suite piv_sm_suite;
#if TC_ENABLE_X509 && TC_ENABLE_PIV_CVC
typedef ::TC_PIV_SM_authentication piv_sm_authentication;
typedef ::TC_PIV_SM_authentication_workspace piv_sm_authentication_workspace;
#endif

// Moving or copying a session could reuse its message counter.
class PIVSM {
  ::TC_PIV_SM session_;

public:
  PIVSM() noexcept : session_{}
  {}
  ~PIVSM() noexcept
  {
    clear();
  }
  PIVSM(const PIVSM&) = delete;
  PIVSM& operator=(const PIVSM&) = delete;
  PIVSM(PIVSM&&) = delete;
  PIVSM& operator=(PIVSM&&) = delete;

  void clear() noexcept
  {
    ::TC_PIV_SM_clear(&session_);
  }
  // The C session, for the layers that take a TC_PIV_SM pointer.
  TC_CPP_NODISCARD ::TC_PIV_SM* native() noexcept
  {
    return &session_;
  }
  TC_CPP_NODISCARD TC_PIV_SM_state state() const noexcept
  {
    return ::TC_PIV_SM_get_state(&session_);
  }
  TC_CPP_NODISCARD TC_status begin(piv_sm_suite suite, const uint8_t (&host_id)[8],
                                   TC_random_source random, piv_sm_handshake& handshake,
                                   piv_sm_workspace& workspace) noexcept
  {
    return ::TC_PIV_SM_begin(&session_, suite, host_id, random, &handshake, &workspace);
  }
  TC_CPP_NODISCARD TC_status finish(const piv_sm_peer& peer, bytes authenticated_key,
                                    piv_sm_workspace& workspace) noexcept
  {
    return ::TC_PIV_SM_finish(&session_, &peer, authenticated_key, &workspace);
  }
#if TC_ENABLE_X509 && TC_ENABLE_PIV_CVC
  TC_CPP_NODISCARD
  credential_status authenticate_response(const piv_sm_authentication& authentication, size_t& work,
                                          piv_sm_authentication_workspace& workspace) noexcept
  {
    return ::TC_PIV_SM_authenticate_response(&session_, &authentication, &work, &workspace);
  }
#endif
  TC_CPP_NODISCARD TC_status protect(const piv_sm_protect_request& request,
                                     size_t& ciphertext_length, uint8_t (&tag)[8],
                                     piv_sm_workspace& workspace) noexcept
  {
    return ::TC_PIV_SM_protect(&session_, &request, &ciphertext_length, buffer{tag, sizeof tag},
                               &workspace);
  }
  // Write at most plaintext.capacity bytes and report the count in plaintext_length.
  TC_CPP_NODISCARD TC_status unprotect(const piv_sm_unprotect_request& request, buffer plaintext,
                                       size_t& plaintext_length,
                                       piv_sm_workspace& workspace) noexcept
  {
    return ::TC_PIV_SM_unprotect(&session_, &request, plaintext, &plaintext_length, &workspace);
  }
};
} // namespace tiny_crypto
#endif
#endif
