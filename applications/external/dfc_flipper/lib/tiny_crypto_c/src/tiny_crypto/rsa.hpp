/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_RSA_HPP_
#define TINY_CRYPTO_RSA_HPP_
#include <tiny_crypto/common.hpp>
#include <tiny_crypto/rsa.h>

namespace tiny_crypto {
typedef ::TC_RSA_result rsa_result;
typedef ::TC_RSA_public_key rsa_public_key;
typedef ::TC_RSA_private_key rsa_private_key;
typedef ::TC_RSA_crt rsa_crt;
typedef ::TC_RSA_crt_output rsa_crt_output;
typedef ::TC_RSA_workspace rsa_workspace;
typedef ::TC_buffer buffer;
typedef ::TC_RSA_execution rsa_execution;
typedef ::TC_RSA_v15_options rsa_v15_options;
typedef ::TC_RSA_pss_options rsa_pss_options;
typedef ::TC_RSA_oaep_options rsa_oaep_options;
typedef ::TC_RSA_keygen_output rsa_keygen_output;
typedef ::TC_RSA_keygen_limits rsa_keygen_limits;
typedef ::TC_RSA_keygen_state rsa_keygen_state;

/* The returned view borrows the caller's array. */
template<size_t N>
inline rsa_workspace rsa_workspace_for(TC_RSA_word (&words)[N]) noexcept {
    rsa_workspace workspace = {words, N};
    return workspace;
}

inline rsa_result rsa_keygen_init(rsa_keygen_state& state, size_t bits,
    const rsa_keygen_output& output, rsa_keygen_limits limits,
    const rsa_workspace& workspace) noexcept {
    return ::TC_RSA_keygen_init(&state, bits, &output, limits, &workspace);
}

inline rsa_result rsa_keygen_step(rsa_keygen_state& state,
    TC_random_source random, TC_RSA_cancel_fn cancel, void* cancel_context,
    TC_work_budget& work) noexcept {
    return ::TC_RSA_keygen_step(&state, random, cancel, cancel_context, &work);
}

inline void rsa_keygen_clear(rsa_keygen_state& state) noexcept {
    ::TC_RSA_keygen_clear(&state);
}

inline rsa_result rsa_verify_v15_digest(const rsa_public_key& key,
    const rsa_v15_options& options, bytes digest, bytes signature,
    const rsa_workspace& workspace, TC_work_budget& work) noexcept {
    return ::TC_RSA_verify_v15_digest(&key, &options, digest, signature,
        &workspace, &work);
}

inline rsa_result rsa_encode_v15_digest(const rsa_v15_options& options,
    bytes digest, buffer encoded, TC_work_budget& work) noexcept {
    return ::TC_RSA_encode_v15_digest(&options, digest, encoded, &work);
}

template<size_t N>
inline rsa_result rsa_encode_v15_digest(const rsa_v15_options& options,
    bytes digest, uint8_t (&encoded)[N], TC_work_budget& work) noexcept {
    buffer output = {encoded, N};
    return rsa_encode_v15_digest(options, digest, output, work);
}

inline rsa_result rsa_encode_pss_digest(const rsa_pss_options& options,
    bytes digest, bytes salt, buffer encoded, TC_work_budget& work) noexcept {
    return ::TC_RSA_encode_pss_digest(&options, digest, salt, encoded, &work);
}

template<size_t N>
inline rsa_result rsa_encode_pss_digest(const rsa_pss_options& options,
    bytes digest, bytes salt, uint8_t (&encoded)[N], TC_work_budget& work) noexcept {
    buffer output = {encoded, N};
    return rsa_encode_pss_digest(options, digest, salt, output, work);
}

inline rsa_result rsa_validate_private_key(const rsa_private_key& key,
    const rsa_workspace& workspace, rsa_execution& execution) noexcept {
    return ::TC_RSA_validate_private_key(&key, &workspace, &execution);
}

inline rsa_result rsa_verify_pss_digest(const rsa_public_key& key,
    const rsa_pss_options& options, bytes digest, bytes signature,
    const rsa_workspace& workspace, TC_work_budget& work) noexcept {
    return ::TC_RSA_verify_pss_digest(&key, &options, digest, signature,
        &workspace, &work);
}

inline rsa_result rsa_validate_crt(const rsa_private_key& key, const rsa_crt& crt,
    const rsa_workspace& workspace, TC_work_budget& work) noexcept {
    return ::TC_RSA_validate_crt(&key, &crt, &workspace, &work);
}

inline rsa_result rsa_derive_crt(const rsa_private_key& key,
    const rsa_crt_output& output, const rsa_workspace& workspace,
    TC_work_budget& work) noexcept {
    return ::TC_RSA_derive_crt(&key, &output, &workspace, &work);
}

inline rsa_result rsa_sign_v15_digest(const rsa_private_key& key,
    const rsa_v15_options& options, bytes digest, const rsa_workspace& workspace,
    buffer signature, rsa_execution& execution) noexcept {
    return ::TC_RSA_sign_v15_digest(&key, &options, digest, &workspace,
        signature, &execution);
}

inline rsa_result rsa_sign_pss_digest(const rsa_private_key& key,
    const rsa_pss_options& options, bytes digest, const rsa_workspace& workspace,
    buffer signature, rsa_execution& execution) noexcept {
    return ::TC_RSA_sign_pss_digest(&key, &options, digest, &workspace,
        signature, &execution);
}

inline rsa_result rsa_encrypt_oaep(const rsa_public_key& key,
    const rsa_oaep_options& options, bytes plaintext,
    const rsa_workspace& workspace, buffer ciphertext,
    rsa_execution& execution) noexcept {
    return ::TC_RSA_encrypt_oaep(&key, &options, plaintext, &workspace,
        ciphertext, &execution);
}

inline rsa_result rsa_decrypt_oaep(const rsa_private_key& key,
    const rsa_oaep_options& options, bytes ciphertext,
    const rsa_workspace& workspace, buffer plaintext, size_t& plaintext_length,
    rsa_execution& execution) noexcept {
    return ::TC_RSA_decrypt_oaep(&key, &options, ciphertext, &workspace,
        plaintext, &plaintext_length, &execution);
}
} // namespace tiny_crypto
#endif
