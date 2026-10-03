/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* RSA wrappers for rsa.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_RSA_HPP_
#define TINY_CRYPTO_RSA_HPP_

#ifndef __cplusplus
#error Do not include rsa.hpp in a C project, include rsa.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/rsa.h>
#if TC_ENABLE_RSA

namespace tiny_crypto {
typedef ::TC_RSA_result rsa_result;
typedef ::TC_RSA_public_key rsa_public_key;
typedef ::TC_RSA_private_key rsa_private_key;
typedef ::TC_RSA_crt rsa_crt;
typedef ::TC_RSA_crt_output rsa_crt_output;
typedef ::TC_RSA_workspace rsa_workspace;
typedef ::TC_RSA_execution rsa_execution;
typedef ::TC_RSA_v15_options rsa_v15_options;
typedef ::TC_RSA_pss_options rsa_pss_options;
typedef ::TC_RSA_oaep_options rsa_oaep_options;
typedef ::TC_RSA_keygen_output rsa_keygen_output;
typedef ::TC_RSA_keygen_limits rsa_keygen_limits;
typedef ::TC_RSA_keygen_state rsa_keygen_state;
typedef ::TC_RSA_prepared_public_key rsa_prepared_public_key;

/* The returned view borrows the caller's array. */
template <size_t N>
TC_CPP_NODISCARD inline rsa_workspace rsa_workspace_for(TC_RSA_word (&words)[N]) noexcept
{
  rsa_workspace workspace = {words, N};
  return workspace;
}

/* Sizing and preflight helpers. rsa_workspace_words returns the limbs an
 * operation needs at a modulus size in bits, or 0 for an unsupported size or
 * operation. The *_work helpers return the exact work units of one successful
 * call as documented in rsa.h, or 0 for rejected parameters. */
TC_CPP_NODISCARD inline size_t rsa_workspace_words(TC_RSA_operation operation, size_t bits) noexcept
{
  return ::TC_RSA_workspace_words(operation, bits);
}

TC_CPP_NODISCARD inline bool rsa_modulus_supported(size_t bits) noexcept
{
  return ::TC_RSA_modulus_supported(bits) != 0;
}

/* True for an odd exponent with 2^16 < e < 2^256 (FIPS 186-5 A.1.1). */
TC_CPP_NODISCARD inline bool rsa_exponent_in_fips_range(bytes exponent) noexcept
{
  return ::TC_RSA_exponent_in_fips_range(exponent) != 0;
}

TC_CPP_NODISCARD inline uint32_t rsa_encode_v15_work(const rsa_v15_options& options,
                                                     size_t modulus_bytes) noexcept
{
  return ::TC_RSA_encode_v15_work(&options, modulus_bytes);
}

TC_CPP_NODISCARD inline uint32_t rsa_encode_pss_work(const rsa_pss_options& options,
                                                     size_t modulus_bytes) noexcept
{
  return ::TC_RSA_encode_pss_work(&options, modulus_bytes);
}

TC_CPP_NODISCARD inline uint32_t rsa_oaep_work(const rsa_oaep_options& options,
                                               size_t modulus_bytes) noexcept
{
  return ::TC_RSA_oaep_work(&options, modulus_bytes);
}

TC_CPP_NODISCARD inline uint32_t rsa_public_work(const rsa_public_key& key) noexcept
{
  return ::TC_RSA_public_work(&key);
}

TC_CPP_NODISCARD inline uint32_t
rsa_prepared_public_work(const rsa_prepared_public_key& setup) noexcept
{
  return ::TC_RSA_prepared_public_work(&setup);
}

TC_CPP_NODISCARD inline uint32_t rsa_private_work(const rsa_private_key& key,
                                                  size_t attempts) noexcept
{
  return ::TC_RSA_private_work(&key, attempts);
}

/* Raw RSA over one formatted representative (RFC 8017 sections 5.1 and 5.2).
 * The output needs the modulus length. A shorter buffer returns TC_RSA_LIMIT
 * and an input at or above the modulus returns TC_RSA_INVALID. Output is
 * written only on TC_RSA_OK. The array overloads take the capacity from the
 * array. rsa.h documents the workspace, blinding and work contracts. */
TC_CPP_NODISCARD inline rsa_result rsa_raw_public(const rsa_public_key& key, bytes input,
                                                  const rsa_workspace& workspace, buffer output,
                                                  TC_work_budget& work) noexcept
{
  return ::TC_RSA_raw_public(&key, input, output, &workspace, &work);
}

template <size_t N>
TC_CPP_NODISCARD inline rsa_result
rsa_raw_public(const rsa_public_key& key, bytes input, const rsa_workspace& workspace,
               uint8_t (&output)[N], TC_work_budget& work) noexcept
{
  return rsa_raw_public(key, input, workspace, buffer{output, N}, work);
}

TC_CPP_NODISCARD inline rsa_result rsa_raw_private(const rsa_public_key& key,
                                                   bytes private_exponent, bytes input,
                                                   const rsa_workspace& workspace, buffer output,
                                                   rsa_execution& execution) noexcept
{
  return ::TC_RSA_raw_private(&key, private_exponent, input, output, &workspace, &execution);
}

template <size_t N>
TC_CPP_NODISCARD inline rsa_result
rsa_raw_private(const rsa_public_key& key, bytes private_exponent, bytes input,
                const rsa_workspace& workspace, uint8_t (&output)[N],
                rsa_execution& execution) noexcept
{
  return rsa_raw_private(key, private_exponent, input, workspace, buffer{output, N}, execution);
}

TC_CPP_NODISCARD inline rsa_result rsa_keygen_init(rsa_keygen_state& state, size_t bits,
                                                   const rsa_keygen_output& output,
                                                   rsa_keygen_limits limits,
                                                   const rsa_workspace& workspace) noexcept
{
  return ::TC_RSA_keygen_init(&state, bits, &output, limits, &workspace);
}

TC_CPP_NODISCARD inline rsa_result rsa_keygen_step(rsa_keygen_state& state, TC_random_source random,
                                                   TC_RSA_cancel_fn cancel, void* cancel_context,
                                                   TC_work_budget& work) noexcept
{
  return ::TC_RSA_keygen_step(&state, random, cancel, cancel_context, &work);
}

inline void rsa_keygen_clear(rsa_keygen_state& state) noexcept
{
  ::TC_RSA_keygen_clear(&state);
}

TC_CPP_NODISCARD inline rsa_result rsa_verify_v15_digest(const rsa_public_key& key,
                                                         const rsa_v15_options& options,
                                                         bytes digest, bytes signature,
                                                         const rsa_workspace& workspace,
                                                         TC_work_budget& work) noexcept
{
  return ::TC_RSA_verify_v15_digest(&key, &options, digest, signature, &workspace, &work);
}

TC_CPP_NODISCARD inline rsa_result rsa_prepare_public_key(rsa_prepared_public_key& setup,
                                                          const rsa_public_key& key,
                                                          const rsa_workspace& cache,
                                                          const rsa_workspace& scratch,
                                                          TC_work_budget& work) noexcept
{
  return ::TC_RSA_prepare_public_key(&setup, &key, &cache, &scratch, &work);
}

inline void rsa_prepared_public_key_clear(rsa_prepared_public_key& setup) noexcept
{
  ::TC_RSA_prepared_public_key_clear(&setup);
}

TC_CPP_NODISCARD inline rsa_result rsa_verify_v15_prepared(const rsa_prepared_public_key& setup,
                                                           const rsa_v15_options& options,
                                                           bytes digest, bytes signature,
                                                           const rsa_workspace& workspace,
                                                           TC_work_budget& work) noexcept
{
  return ::TC_RSA_verify_v15_prepared(&setup, &options, digest, signature, &workspace, &work);
}

TC_CPP_NODISCARD inline rsa_result rsa_encode_v15_digest(const rsa_v15_options& options,
                                                         bytes digest, buffer encoded,
                                                         TC_work_budget& work) noexcept
{
  return ::TC_RSA_encode_v15_digest(&options, digest, encoded, &work);
}

template <size_t N>
TC_CPP_NODISCARD inline rsa_result rsa_encode_v15_digest(const rsa_v15_options& options,
                                                         bytes digest, uint8_t (&encoded)[N],
                                                         TC_work_budget& work) noexcept
{
  buffer output = {encoded, N};
  return rsa_encode_v15_digest(options, digest, output, work);
}

TC_CPP_NODISCARD inline rsa_result rsa_encode_pss_digest(const rsa_pss_options& options,
                                                         bytes digest, bytes salt, buffer encoded,
                                                         TC_work_budget& work) noexcept
{
  return ::TC_RSA_encode_pss_digest(&options, digest, salt, encoded, &work);
}

template <size_t N>
TC_CPP_NODISCARD inline rsa_result
rsa_encode_pss_digest(const rsa_pss_options& options, bytes digest, bytes salt,
                      uint8_t (&encoded)[N], TC_work_budget& work) noexcept
{
  buffer output = {encoded, N};
  return rsa_encode_pss_digest(options, digest, salt, output, work);
}

TC_CPP_NODISCARD inline rsa_result
rsa_validate_private_key(const rsa_private_key& key, const rsa_workspace& workspace,
                         rsa_execution& execution,
                         TC_RSA_exponent_policy exponent_policy = TC_RSA_EXPONENT_FIPS) noexcept
{
  return ::TC_RSA_validate_private_key(&key, exponent_policy, &workspace, &execution);
}

TC_CPP_NODISCARD inline rsa_result rsa_verify_pss_digest(const rsa_public_key& key,
                                                         const rsa_pss_options& options,
                                                         bytes digest, bytes signature,
                                                         const rsa_workspace& workspace,
                                                         TC_work_budget& work) noexcept
{
  return ::TC_RSA_verify_pss_digest(&key, &options, digest, signature, &workspace, &work);
}

TC_CPP_NODISCARD inline rsa_result rsa_verify_pss_prepared(const rsa_prepared_public_key& setup,
                                                           const rsa_pss_options& options,
                                                           bytes digest, bytes signature,
                                                           const rsa_workspace& workspace,
                                                           TC_work_budget& work) noexcept
{
  return ::TC_RSA_verify_pss_prepared(&setup, &options, digest, signature, &workspace, &work);
}

TC_CPP_NODISCARD inline rsa_result rsa_validate_crt(const rsa_private_key& key, const rsa_crt& crt,
                                                    const rsa_workspace& workspace,
                                                    TC_work_budget& work) noexcept
{
  return ::TC_RSA_validate_crt(&key, &crt, &workspace, &work);
}

TC_CPP_NODISCARD inline rsa_result rsa_derive_crt(const rsa_private_key& key,
                                                  const rsa_crt_output& output,
                                                  const rsa_workspace& workspace,
                                                  TC_work_budget& work) noexcept
{
  return ::TC_RSA_derive_crt(&key, &output, &workspace, &work);
}

TC_CPP_NODISCARD inline rsa_result rsa_sign_v15_digest(const rsa_private_key& key,
                                                       const rsa_v15_options& options, bytes digest,
                                                       const rsa_workspace& workspace,
                                                       buffer signature,
                                                       rsa_execution& execution) noexcept
{
  return ::TC_RSA_sign_v15_digest(&key, &options, digest, signature, &workspace, &execution);
}

TC_CPP_NODISCARD inline rsa_result rsa_sign_pss_digest(const rsa_private_key& key,
                                                       const rsa_pss_options& options, bytes digest,
                                                       const rsa_workspace& workspace,
                                                       buffer signature,
                                                       rsa_execution& execution) noexcept
{
  return ::TC_RSA_sign_pss_digest(&key, &options, digest, signature, &workspace, &execution);
}

TC_CPP_NODISCARD inline rsa_result rsa_encrypt_oaep(const rsa_public_key& key,
                                                    const rsa_oaep_options& options,
                                                    bytes plaintext, const rsa_workspace& workspace,
                                                    buffer ciphertext,
                                                    rsa_execution& execution) noexcept
{
  return ::TC_RSA_encrypt_oaep(&key, &options, plaintext, ciphertext, &workspace, &execution);
}

TC_CPP_NODISCARD inline rsa_result
rsa_decrypt_oaep(const rsa_private_key& key, const rsa_oaep_options& options, bytes ciphertext,
                 const rsa_workspace& workspace, buffer plaintext, size_t& plaintext_length,
                 rsa_execution& execution) noexcept
{
  return ::TC_RSA_decrypt_oaep(&key, &options, ciphertext, plaintext, &plaintext_length, &workspace,
                               &execution);
}
} // namespace tiny_crypto
#endif
#endif
