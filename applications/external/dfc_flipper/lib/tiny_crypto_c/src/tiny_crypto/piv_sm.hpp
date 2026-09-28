/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_SM_HPP_
#define TINY_CRYPTO_PIV_SM_HPP_
#include <tiny_crypto/common.hpp>
#include <tiny_crypto/piv_sm.h>
#if TC_ENABLE_X509 && TC_ENABLE_PIV_CVC
#include <tiny_crypto/piv_sm_authenticate.h>
#endif

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
class piv_sm {
    ::TC_PIV_SM session_;
public:
    piv_sm() noexcept : session_{} {}
    ~piv_sm() noexcept { clear(); }
    piv_sm(const piv_sm&) = delete;
    piv_sm& operator=(const piv_sm&) = delete;
    piv_sm(piv_sm&&) = delete;
    piv_sm& operator=(piv_sm&&) = delete;

    void clear() noexcept { ::TC_PIV_SM_clear(&session_); }
    TC_PIV_SM_state state() const noexcept {
        return static_cast<TC_PIV_SM_state>(session_.state);
    }
    TC_status begin(piv_sm_suite suite, const uint8_t (&host_id)[8],
                    TC_random_fn random, void* random_user,
                    piv_sm_handshake& handshake,
                    piv_sm_workspace& workspace) noexcept {
        return ::TC_PIV_SM_begin(&session_, suite, host_id, random, random_user,
                                 &handshake, &workspace);
    }
    TC_status finish(const piv_sm_peer& peer,
                     bytes authenticated_key, piv_sm_workspace& workspace) noexcept {
        return ::TC_PIV_SM_finish(&session_, &peer, authenticated_key, &workspace);
    }
#if TC_ENABLE_X509 && TC_ENABLE_PIV_CVC
    credential_status authenticate_response(const piv_sm_authentication& authentication,
                                              size_t& work,
                                              piv_sm_authentication_workspace& workspace) noexcept {
        return ::TC_PIV_SM_authenticate_response(&session_, &authentication, &work, &workspace);
    }
#endif
    TC_status protect(const piv_sm_protect_request& request,
                      size_t& ciphertext_length, uint8_t (&tag)[8],
                      piv_sm_workspace& workspace) noexcept {
        return ::TC_PIV_SM_protect(&session_, &request, &ciphertext_length,
                                   tag, &workspace);
    }
    TC_status unprotect(const piv_sm_unprotect_request& request, uint8_t* output,
                        size_t capacity, size_t& plaintext_length,
                        piv_sm_workspace& workspace) noexcept {
        return ::TC_PIV_SM_unprotect(&session_, &request, output, capacity,
                                     &plaintext_length, &workspace);
    }
};
} // namespace tiny_crypto
#endif
