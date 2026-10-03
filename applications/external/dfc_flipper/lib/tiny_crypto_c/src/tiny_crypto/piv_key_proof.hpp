/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Key proofs on a PIVLink for piv_key_proof.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_KEY_PROOF_HPP_
#define TINY_CRYPTO_PIV_KEY_PROOF_HPP_

#ifndef __cplusplus
#error Do not include piv_key_proof.hpp in a C project, include piv_key_proof.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/piv_command.hpp>
#include <tiny_crypto/piv_key_proof.h>
#if TC_ENABLE_PIV_KEY_PROOF

namespace tiny_crypto {

typedef ::TC_PIV_rsa_padding piv_rsa_padding;
typedef ::TC_PIV_key_policy piv_key_policy;
typedef ::TC_PIV_key_parameters piv_key_parameters;
typedef ::TC_PIV_key_proof_request piv_key_proof_request;
typedef ::TC_PIV_key_proof_workspace piv_key_proof_workspace;

/* TC_PIV_key_parameters_select. */
TC_CPP_NODISCARD inline piv_result piv_key_parameters_select(const TC_X509_certificate& certificate,
                                                             const piv_key_policy& policy,
                                                             piv_key_parameters& out) noexcept
{
  return ::TC_PIV_key_parameters_select(&certificate, &policy, &out);
}

/* TC_PIV_key_prove. */
TC_CPP_NODISCARD inline piv_result
piv_key_prove(PIVLink& link, const piv_key_proof_request& request, TC_random_source random,
              const TC_X509_signature_provider& provider, piv_key_proof_workspace& workspace,
              TC_work_budget& work) noexcept
{
  return ::TC_PIV_key_prove(link.native(), &request, random, &provider, &workspace, &work);
}

} // namespace tiny_crypto
#endif
#endif
