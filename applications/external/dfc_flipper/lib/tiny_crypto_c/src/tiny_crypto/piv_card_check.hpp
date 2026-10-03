/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The composed card check for piv_card_check.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_CARD_CHECK_HPP_
#define TINY_CRYPTO_PIV_CARD_CHECK_HPP_

#ifndef __cplusplus
#error Do not include piv_card_check.hpp in a C project, include piv_card_check.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/piv_card_check.h>
#include <tiny_crypto/piv_command.hpp>

#if TC_ENABLE_PIV_CARD_CHECK
namespace tiny_crypto {

typedef ::TC_PIV_check_outcome piv_check_outcome;
typedef ::TC_PIV_check_kind piv_check_kind;
typedef ::TC_PIV_check_reason piv_check_reason;
typedef ::TC_PIV_check piv_check;
typedef ::TC_PIV_check_requirement piv_check_requirement;
typedef ::TC_PIV_card_report piv_card_report;
typedef ::TC_PIV_card_check_ocsp piv_card_check_ocsp;
typedef ::TC_PIV_card_check_request piv_card_check_request;
typedef ::TC_PIV_card_check_workspace piv_card_check_workspace;
typedef ::TC_PIV_card_certificate_request piv_card_certificate_request;
typedef ::TC_PIV_card_certificate_report piv_card_certificate_result;

/* TC_PIV_card_check. */
TC_CPP_NODISCARD inline piv_result piv_card_check(const piv_card_check_request& request,
                                                  piv_card_check_workspace& workspace, size_t& work,
                                                  piv_card_report& out) noexcept
{
  return ::TC_PIV_card_check(&request, &workspace, &work, &out);
}

/* TC_PIV_card_report_find. */
TC_CPP_NODISCARD inline const piv_check*
piv_card_report_find(const piv_card_report& report,
                     const piv_check_requirement& requirement) noexcept
{
  return ::TC_PIV_card_report_find(&report, &requirement);
}

/* TC_PIV_card_report_accepts. */
TC_CPP_NODISCARD inline bool piv_card_report_accepts(const piv_card_report& report,
                                                     const piv_check_requirement* required,
                                                     size_t count) noexcept
{
  return ::TC_PIV_card_report_accepts(&report, required, count) != 0;
}

/* TC_PIV_card_report_accepts over an array of requirements. */
template <size_t N>
TC_CPP_NODISCARD inline bool
piv_card_report_accepts(const piv_card_report& report,
                        const piv_check_requirement (&required)[N]) noexcept
{
  return ::TC_PIV_card_report_accepts(&report, required, N) != 0;
}

/* TC_PIV_card_certificate_validate. */
TC_CPP_NODISCARD inline credential_status
piv_card_certificate_validate(const piv_card_certificate_request& request,
                              const TC_validation_context& context, size_t& work,
                              piv_card_certificate_result& out) noexcept
{
  return ::TC_PIV_card_certificate_validate(&request, &context, &work, &out);
}

#if TC_ENABLE_PIV_KEY_PROOF
typedef ::TC_PIV_card_proof_request piv_card_proof_request;

/* TC_PIV_card_prove_keys on a PIVLink. */
TC_CPP_NODISCARD inline piv_result piv_card_prove_keys(PIVLink& link,
                                                       const piv_card_proof_request& request,
                                                       TC_PIV_key_proof_workspace& workspace,
                                                       TC_work_budget& work,
                                                       piv_card_report& report) noexcept
{
  return ::TC_PIV_card_prove_keys(link.native(), &request, &workspace, &work, &report);
}
#endif

} // namespace tiny_crypto
#endif
#endif
