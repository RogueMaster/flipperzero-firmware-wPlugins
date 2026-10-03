/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* EC wrappers for ec.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_EC_HPP_
#define TINY_CRYPTO_EC_HPP_

#ifndef __cplusplus
#error Do not include ec.hpp in a C project, include ec.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/ec.h>
#if TC_ENABLE_EC

namespace tiny_crypto {
typedef ::TC_EC_curve ec_curve;
typedef ::TC_EC_result ec_result;
typedef ::TC_EC_execution ec_execution;
typedef ::TC_ECDSA_sign_options ecdsa_sign_options;
typedef ::TC_EC_workspace ec_workspace;
typedef ::TC_ECDSA_workspace ecdsa_workspace;

/* Thin wrappers over the C API. Contracts, statuses and work rules match
 * <tiny_crypto/ec.h>. Output arrays select their capacity from N. */
TC_CPP_NODISCARD inline size_t ec_coordinate_bytes(ec_curve curve) noexcept
{
  return ::TC_EC_coordinate_bytes(curve);
}
TC_CPP_NODISCARD inline uint32_t ec_operation_work(ec_curve curve,
                                                   TC_EC_operation operation) noexcept
{
  return ::TC_EC_operation_work(curve, operation);
}
template <size_t N>
TC_CPP_NODISCARD inline ec_result ec_public_key(ec_curve curve, bytes private_key,
                                                uint8_t (&public_key)[N], ec_workspace& workspace,
                                                TC_work_budget& work) noexcept
{
  return ::TC_EC_public_key(curve, private_key, TC_buffer{public_key, N}, &workspace, &work);
}
template <size_t P, size_t Q>
TC_CPP_NODISCARD inline ec_result
ec_generate_key_pair(ec_curve curve, uint8_t (&private_key)[P], uint8_t (&public_key)[Q],
                     ec_workspace& workspace, ec_execution& execution) noexcept
{
  return ::TC_EC_generate_key_pair(curve, TC_buffer{private_key, P}, TC_buffer{public_key, Q},
                                   &workspace, &execution);
}
TC_CPP_NODISCARD inline ec_result ec_validate_public_key(ec_curve curve, bytes public_key,
                                                         ec_workspace& workspace,
                                                         TC_work_budget& work) noexcept
{
  return ::TC_EC_validate_public_key(curve, public_key, &workspace, &work);
}
template <size_t N>
TC_CPP_NODISCARD inline ec_result ecdh(ec_curve curve, bytes private_key, bytes peer_public_key,
                                       uint8_t (&shared_secret)[N], ec_workspace& workspace,
                                       TC_work_budget& work) noexcept
{
  return ::TC_ECDH(curve, private_key, peer_public_key, TC_buffer{shared_secret, N}, &workspace,
                   &work);
}
TC_CPP_NODISCARD inline ec_result ecdsa_verify_digest(ec_curve curve, bytes public_key,
                                                      bytes digest, bytes signature,
                                                      ecdsa_workspace& workspace,
                                                      TC_work_budget& work) noexcept
{
  return ::TC_ECDSA_verify_digest(curve, public_key, digest, signature, &workspace, &work);
}
template <size_t N>
TC_CPP_NODISCARD inline ec_result
ecdsa_sign_digest(ec_curve curve, bytes private_key, bytes public_key, bytes digest,
                  uint8_t (&signature)[N], ecdsa_workspace& workspace,
                  const ecdsa_sign_options& options, TC_work_budget& work) noexcept
{
  return ::TC_ECDSA_sign_digest(curve, &options, private_key, public_key, digest,
                                TC_buffer{signature, N}, &workspace, &work);
}
template <size_t N>
TC_CPP_NODISCARD inline ec_result
ecdsa_sign_digest_external_random(ec_curve curve, bytes private_key, bytes public_key, bytes digest,
                                  uint8_t (&signature)[N], ecdsa_workspace& workspace,
                                  ec_execution& execution) noexcept
{
  return ::TC_ECDSA_sign_digest_external_random(curve, private_key, public_key, digest,
                                                TC_buffer{signature, N}, &workspace, &execution);
}
} // namespace tiny_crypto
#endif
#endif
