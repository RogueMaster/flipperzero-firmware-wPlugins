/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Secure messaging on a PIVLink for piv_sm_apdu.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_SM_APDU_HPP_
#define TINY_CRYPTO_PIV_SM_APDU_HPP_

#ifndef __cplusplus
#error Do not include piv_sm_apdu.hpp in a C project, include piv_sm_apdu.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/piv_command.hpp>
#include <tiny_crypto/piv_sm.hpp>
#include <tiny_crypto/piv_sm_apdu.h>
#if TC_ENABLE_PIV_SM_APDU

namespace tiny_crypto {

/* TC_PIV_SM_key_request. The link holds a pointer to session until it is
 * unbound, so declare the PIVSM before the PIVLink: the link is then
 * destroyed first and clears the session while it still exists. peer borrows
 * response. */
TC_CPP_NODISCARD inline piv_result
piv_sm_key_request(PIVLink& link, PIVSM& session, piv_sm_suite suite, const uint8_t (&host_id)[8],
                   TC_random_source random, buffer response, piv_sm_peer& peer,
                   piv_sm_workspace& workspace) noexcept
{
  return ::TC_PIV_SM_key_request(link.native(), session.native(), suite, host_id, random, response,
                                 &peer, &workspace);
}

/* TC_PIV_link_secure. workspace and sm_scratch stay borrowed while the
 * session is bound. */
TC_CPP_NODISCARD inline piv_result piv_link_secure(PIVLink& link, piv_sm_workspace& workspace,
                                                   buffer sm_scratch) noexcept
{
  return ::TC_PIV_link_secure(link.native(), &workspace, sm_scratch);
}

/* TC_PIV_link_unsecure. */
inline void piv_link_unsecure(PIVLink& link) noexcept
{
  ::TC_PIV_link_unsecure(link.native());
}

} // namespace tiny_crypto
#endif
#endif
